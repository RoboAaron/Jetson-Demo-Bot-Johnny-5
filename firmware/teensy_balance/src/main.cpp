#include <Arduino.h>
#include "config.h"
#include "imu_bno085.h"
#include "vesc_can.h"
#include "balance_pid.h"

// =============================================================
// main.cpp — Balance controller main loop
// Jetson Demo Bot (Johnny-5) — Teensy 4.1
//
// Control flow:
//   1. Initialize IMU and VESC CAN
//   2. Loop at BALANCE_LOOP_HZ:
//      a. Poll VESC CAN for telemetry
//      b. Read IMU pitch angle
//      c. Fall detection (disable if |pitch| > FALL_THRESHOLD_DEG)
//      d. PID → current command → VESC
//      e. Serial debug output
//      f. Handle serial CLI commands
//
// See firmware/FIRMWARE_DESIGN.md for architecture decisions.
// =============================================================

// ---------------------------------------------------------------
// Subsystem instances
// ---------------------------------------------------------------
static IMU_BNO085 imu;
static VescCan    vesc;
static BalancePID pid(PID_KP, PID_KI, PID_KD, MIN_CURRENT_A, MAX_CURRENT_A);

// ---------------------------------------------------------------
// Loop timing
// ---------------------------------------------------------------
static uint32_t lastLoopUs  = 0;
static const uint32_t LOOP_PERIOD_US = 1000000UL / BALANCE_LOOP_HZ;

// ---------------------------------------------------------------
// State
// ---------------------------------------------------------------
static bool balanceEnabled = false;
static bool fellDown       = false;  // Latched fall flag (require re-enable)

// ---------------------------------------------------------------
// Serial debug
// ---------------------------------------------------------------
static void printTelemetry(float pitch, float pidOut) {
    static uint32_t lastPrintMs = 0;
    const uint32_t PRINT_PERIOD_MS = 1000 / DEBUG_PRINT_HZ;

    if (millis() - lastPrintMs < PRINT_PERIOD_MS) return;
    lastPrintMs = millis();

    // Tab-separated for Serial Plotter compatibility
    Serial.print("PITCH:");    Serial.print(pitch, 2);
    Serial.print("\tSP:");     Serial.print(PITCH_TRIM_DEG, 2);
    Serial.print("\tPID:");    Serial.print(pidOut, 2);
    Serial.print("\tP:");      Serial.print(pid.getPTerm(), 2);
    Serial.print("\tI:");      Serial.print(pid.getITerm(), 2);
    Serial.print("\tD:");      Serial.print(pid.getDTerm(), 2);
    Serial.print("\tRPM_L:");  Serial.print(vesc.leftStatus().rpm,  0);
    Serial.print("\tRPM_R:");  Serial.print(vesc.rightStatus().rpm, 0);
    Serial.print("\tI_L:");    Serial.print(vesc.leftStatus().current_a,  1);
    Serial.print("\tI_R:");    Serial.print(vesc.rightStatus().current_a, 1);
    Serial.print("\tEN:");     Serial.print(balanceEnabled ? 1 : 0);
    Serial.println();
}

static void printHelp() {
    Serial.println("--- CLI commands ---");
    Serial.println("  e   : toggle balance enable/disable");
    Serial.println("  p   : print current PID gains");
    Serial.println("  +/- : increase/decrease Kp by 0.5");
    Serial.println("  r   : reset PID integrator");
    Serial.println("  h   : print this help");
    Serial.println("--------------------");
}

static void handleSerial() {
    if (!Serial.available()) return;
    char c = Serial.read();

    switch (c) {
        case 'e':
        case 'E':
            if (fellDown) {
                Serial.println("[CMD] Re-enabling after fall (integral reset)");
                pid.reset();
                fellDown = false;
            }
            balanceEnabled = !balanceEnabled;
            Serial.print("[CMD] Balance ");
            Serial.println(balanceEnabled ? "ENABLED" : "DISABLED");
            if (!balanceEnabled) vesc.coast();
            break;

        case 'p':
        case 'P':
            Serial.print("[PID] Kp="); Serial.print(pid.getKp(), 3);
            Serial.print(" Ki=");      Serial.print(pid.getKi(), 3);
            Serial.print(" Kd=");      Serial.print(pid.getKd(), 3);
            Serial.print(" SP=");      Serial.println(pid.getSetpoint(), 2);
            break;

        case '+':
            pid.setGains(pid.getKp() + 0.5f, pid.getKi(), pid.getKd());
            Serial.print("[PID] Kp -> "); Serial.println(pid.getKp(), 3);
            break;

        case '-':
            pid.setGains(max(0.0f, pid.getKp() - 0.5f), pid.getKi(), pid.getKd());
            Serial.print("[PID] Kp -> "); Serial.println(pid.getKp(), 3);
            break;

        case 'r':
        case 'R':
            pid.reset();
            Serial.println("[PID] Integrator reset");
            break;

        case 'h':
        case 'H':
        case '?':
            printHelp();
            break;

        default:
            break;
    }
}

// ---------------------------------------------------------------
// setup()
// ---------------------------------------------------------------
void setup() {
    Serial.begin(DEBUG_BAUD);
    delay(500);   // Brief pause for USB serial host to connect

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    Serial.println();
    Serial.println("==============================================");
    Serial.println(" Jetson Demo Bot (Johnny-5)");
    Serial.println(" Teensy 4.1 Balance Controller  v0.1-skeleton");
    Serial.println("==============================================");
    Serial.println();

    // ----------------------------------------------------------
    // IMU
    // ----------------------------------------------------------
    Serial.println("[BOOT] Initializing BNO085 IMU...");
    if (!imu.begin()) {
        // imu.begin() already printed the error
        Serial.println("[BOOT] HALTED — fix IMU before continuing");
        while (true) {
            digitalWrite(LED_PIN, !digitalRead(LED_PIN));
            delay(200);  // Rapid blink = IMU error
        }
    }

    // ----------------------------------------------------------
    // VESC CAN
    // ----------------------------------------------------------
    Serial.println("[BOOT] Initializing VESC CAN interface...");
    vesc.begin();
    Serial.println("[BOOT] Waiting for VESC heartbeat (power on ESCs if not done)...");

    // Wait up to 5 s for at least one STATUS frame from any VESC
    uint32_t t0 = millis();
    while (!vesc.isAlive() && (millis() - t0) < 5000) {
        vesc.update();
        delay(10);
    }
    if (vesc.isAlive()) {
        Serial.println("[BOOT] VESC STATUS frames received — ESCs online");
    } else {
        Serial.println("[BOOT] WARNING: No VESC STATUS received in 5 s.");
        Serial.println("[BOOT]   Check CAN wiring and that ESCs are powered.");
        Serial.println("[BOOT]   Continuing — balance will not enable until VESCs respond.");
    }

    // ----------------------------------------------------------
    // PID
    // ----------------------------------------------------------
    pid.setSetpoint(PITCH_TRIM_DEG);
    Serial.println("[BOOT] PID initialized (NOT ENABLED)");

    printHelp();
    Serial.println("[BOOT] Ready.  Press 'e' to enable balance.");
    Serial.println();

    lastLoopUs = micros();
}

// ---------------------------------------------------------------
// loop()
// ---------------------------------------------------------------
void loop() {
    // ----------------------------------------------------------
    // Rate limiter — run at BALANCE_LOOP_HZ
    // ----------------------------------------------------------
    uint32_t nowUs  = micros();
    uint32_t elapsed = nowUs - lastLoopUs;
    if (elapsed < LOOP_PERIOD_US) return;

    float dt_s = (float)elapsed * 1e-6f;
    lastLoopUs = nowUs;

    // ----------------------------------------------------------
    // 1. Drain CAN receive buffer (VESC telemetry)
    // ----------------------------------------------------------
    vesc.update();

    // ----------------------------------------------------------
    // 2. Read IMU — skip if no new data this cycle
    // ----------------------------------------------------------
    bool newData = imu.update();
    if (!newData) {
        // If IMU is silent for too long something is wrong
        if (imu.msSinceLastUpdate() > 100) {
            Serial.println("[IMU] WARNING: no data for 100 ms — check connection");
        }
        return;
    }

    float pitch = imu.getPitch();

    // ----------------------------------------------------------
    // 3. Fall detection — latch off if beyond threshold
    // ----------------------------------------------------------
    if (fabsf(pitch) > FALL_THRESHOLD_DEG) {
        if (balanceEnabled) {
            Serial.print("[FALL] Pitch = ");
            Serial.print(pitch, 1);
            Serial.println("° — motors disabled. Press 'e' to re-enable.");
            fellDown = true;
        }
        balanceEnabled = false;
        vesc.coast();
        pid.reset();
        digitalWrite(LED_PIN, HIGH);  // Solid LED = fallen
        return;
    }

    // ----------------------------------------------------------
    // 4. Balance PID
    // ----------------------------------------------------------
    float pidOutput = 0.0f;

    if (balanceEnabled) {
        if (!vesc.isAlive()) {
            // No CAN heartbeat — coast and warn
            static uint32_t lastWarnMs = 0;
            if (millis() - lastWarnMs > 1000) {
                Serial.println("[VESC] No heartbeat — coasting (check CAN wiring)");
                lastWarnMs = millis();
            }
            vesc.coast();
        } else {
            pidOutput = pid.compute(pitch, dt_s);
            vesc.setCurrentBoth(pidOutput);
        }
        digitalWrite(LED_PIN, (millis() / 250) & 1);  // Slow blink = enabled
    } else {
        vesc.coast();
        digitalWrite(LED_PIN, fellDown ? HIGH : LOW);
    }

    // ----------------------------------------------------------
    // 5. Serial telemetry
    // ----------------------------------------------------------
    printTelemetry(pitch, pidOutput);

    // ----------------------------------------------------------
    // 6. Serial CLI
    // ----------------------------------------------------------
    handleSerial();
}
