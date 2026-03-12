/*
 * BALANCE ROBOT - CASCADED VELOCITY CONTROL (Phase 1)
 * 
 * ARCHITECTURE: Cascaded PID (Velocity → Angle → Motor Current)
 * - Phase 1: Velocity control loop added to working single-loop balance
 * - Velocity PID output adjusts angle setpoint
 * - Angle PID maintains balance (inherited from single-loop)
 * 
 * CONTROL FLOW:
 * 1. Read IMU roll angle
 * 2. Read VESC encoder RPM → convert to velocity (m/s)
 * 3. Velocity PID: velocity error → angle setpoint offset
 * 4. Angle PID: (baseSetpoint + angleSetpointFromVel) → motor current
 * 5. Send current to motors
 * 
 * When velocitySetpoint = 0.0: Robot balances in place (like single-loop)
 * When velocitySetpoint > 0.0: Robot tilts forward and moves forward
 *
 * === CHANGED ===
 * FORCE_SINGLE_LOOP_MODE: set to 1 to disable velocity loop at compile time (pure angle PID = single-loop).
 * useVelocityLoop: runtime flag (default false); 'v' toggles. When false, angleSetpoint = baseSetpoint exactly.
 */

// Set to 1 to behave exactly like single-loop (velocity loop never active)
#define FORCE_SINGLE_LOOP_MODE 0

#include <Wire.h>
#include <string.h>
#include <Adafruit_BNO08x.h>
#include <VescUart.h>
#include <PID_v1.h>
#include <EEPROM.h>

// Global objects
Adafruit_BNO08x bno08x(-1);  // I2C mode (no reset pin needed)
sh2_SensorValue_t sensorValue;
VescUart vescLeft;
VescUart vescRight;

// IMU data
bool imuWorking = false;
float pitch = 0.0, roll = 0.0, yaw = 0.0;

// VESC communication tracking (for diagnostics)
unsigned long vescFailCount = 0;
unsigned long vescSuccessCount = 0;

// VELOCITY CONTROL LOOP (Outer Loop - Phase 1)
double velocitySetpoint = 0.0;        // Target velocity (m/s)
double velocityInput;                  // Current velocity from encoders (m/s)
double angleSetpointFromVel = 0.0;    // Output: angle setpoint offset (degrees)

// Velocity PID Gains - Start conservative (P only for tuning, no integral, no derivative)
double Kp_vel = 1.0;    // Proportional gain (increased significantly - start at 1.0, then 2.0, 3.0)
double Ki_vel = 0.0;    // Integral gain (KEEP AT 0 for initial tuning)
double Kd_vel = 0.0;    // Derivative gain (ALWAYS 0 - PI only)

// Velocity PID: Input=velocity, Output=angleSetpointFromVel, Setpoint=velocitySetpoint
// REVERSE mode: When velocity is too low (negative error), increase angle offset (tilt forward)
// SIGN VERIFICATION: +VelSet with -velocity → positive error → REVERSE gives negative angleFromVel (tilt forward)
PID velocityPID(&velocityInput, &angleSetpointFromVel, &velocitySetpoint, Kp_vel, Ki_vel, Kd_vel, REVERSE);

// Velocity filtering and control
// === CHANGED === Stronger filtering and wider deadband so standstill stays in deadband (VESC ERPM noise).
float filteredVelocity = 0.0;           // EMA + moving-average filtered velocity (m/s)
const float VELOCITY_FILTER_ALPHA = 0.04;  // EMA coefficient (lower = more filtering; 0.04 = heavy smoothing)
const float VELOCITY_DEADBAND = 0.25;      // Deadband when setpoint = 0 (m/s) — wider so noise doesn't exit
const float VELOCITY_OUTPUT_MAX = 0.5;    // Maximum velocity PID output (±degrees)
const float VELOCITY_SLEW_RATE = 0.05;    // Maximum change per update (degrees)
float lastAngleSetpointFromVel = 0.0;     // For slew rate limiting
// Moving-average buffer (samples of EMA output) for extra smoothing at standstill
const int VELOCITY_MA_SIZE = 6;
float velocityEmaBuffer[VELOCITY_MA_SIZE];
int velocityEmaIndex = 0;
bool velocityMaFilled = false;

// === CHANGED === Runtime flag: when false, velocity loop is off and angle setpoint = baseSetpoint exactly.
bool useVelocityLoop = false;  // Default false for testing; 'v' toggles. Match single-loop first.

// ANGLE CONTROL LOOP (Inner Loop - from single-loop)
double baseSetpoint = -0.70;  // Base balance angle (degrees) - from LAST_WORKING_CONFIG.md
double angleSetpoint = 0.0;   // Active setpoint = baseSetpoint + angleSetpointFromVel (or baseSetpoint when vel loop off)
double angleInput;             // Current roll angle (degrees)
double motorCurrent;           // PID output: motor current (Amps)

// Angle PID defaults tuned to match the filtered single-loop controller.
double Kp = 5.0;    // Proportional gain (single-loop used 5.0; tune 4.5–6.0)
double Ki = 0.0;    // Integral gain (start 0, add small later if needed)
double Kd = 0.03;   // Lower starting Kd: raw 500 Hz angle data made 0.3 saturate constantly

// Angle input low-pass filter for derivative-noise suppression.
// Alpha = 0.3 gives roughly a 24 Hz cutoff at the 500 Hz angle PID rate.
float angleFilterAlpha = 0.3f;

PID balancePID(&angleInput, &motorCurrent, &angleSetpoint, Kp, Ki, Kd, DIRECT);

// YAW CONTROL: Prevents unwanted rotation (from single-loop)
double yawSetpoint = 0.0;    // Target yaw angle (degrees) - initialized when balancing starts
double yawInput;             // Current yaw angle (degrees)
double yawOutput;            // Yaw PID output: differential current (Amps)

// Yaw PID Gains - DISABLED BY DEFAULT
double Kp_yaw = 0.0;   // Proportional gain for yaw (0 = disabled)
double Ki_yaw = 0.0;   // Integral gain for yaw (0 = disabled)
double Kd_yaw = 0.0;   // Derivative gain for yaw (0 = disabled)

PID yawPID(&yawInput, &yawOutput, &yawSetpoint, Kp_yaw, Ki_yaw, Kd_yaw, DIRECT);
bool yawControlEnabled = false;  // DISABLED BY DEFAULT

// Velocity measurement from encoders
float leftVelocity = 0.0;   // m/s (persists between VESC reads)
float rightVelocity = 0.0;  // m/s (persists between VESC reads)
float avgVelocity = 0.0;    // m/s
const float WHEEL_DIAMETER = 0.165;  // meters (from VESC XML: si_wheel_diameter=0.165)
const int MOTOR_POLES = 30;          // Total motor poles (from VESC XML: si_motor_poles=30)
const int POLE_PAIRS = 15;           // Pole pairs (MOTOR_POLES / 2 = 15)
const float GEAR_RATIO = 1.0;        // Gear ratio (from VESC XML: gear_ratio=1, direct drive)
// Convert mechanical RPM to m/s: mps = mech_rpm * pi * wheel_diameter / 60
const float RPM_TO_MPS = (WHEEL_DIAMETER * PI) / 60.0;

// Motor control parameters
float maxCurrent = 6.5;  // Maximum motor current (Amps) - from LAST_WORKING_CONFIG.md
float minCurrent = 0.0;  // DEPRECATED: Use stiction compensation instead (kept for EEPROM compatibility)

// Stiction (static friction) compensation
// Motors need ~0.55A to overcome static friction and start moving.
// Without this, small PID corrections produce no motion, destabilizing velocity control.
const float MIN_DRIVE_CURRENT = 0.55f;  // Stiction breakaway current (Amps)
const float DRIVE_ZERO_EPS = 0.01f;     // Treat commands below this as true zero (Amps)

// Single-loop parity mode for algorithm A/B isolation:
// when velocity+yaw are OFF, emulate single-loop actuator behavior.
const bool SINGLE_LOOP_PARITY_WHEN_VEL_OFF = true;
const float PARITY_MIN_CURRENT = 0.1f;
// True = do not read VESC feedback when velocity loop is OFF (clean angle-only A/B mode).
const bool BYPASS_VESC_FEEDBACK_WHEN_VEL_OFF = true;

// Fine adjust mode (for smaller tuning steps)
bool fineAdjust = false;
// Dry-run mode: keep computing/logging commanded currents but suppress motor commands.
bool motorOutputEnabled = true;
const float KP_STEP_COARSE = 0.5;
const float KP_STEP_FINE = 0.1;
const float KI_STEP_COARSE = 0.05;
const float KI_STEP_FINE = 0.01;
const float KD_STEP_COARSE = 0.05;
const float KD_STEP_FINE = 0.01;
const float SETPOINT_STEP_COARSE = 0.1;
const float SETPOINT_STEP_FINE = 0.02;
const float MAXCURRENT_STEP_COARSE = 0.5;
const float MAXCURRENT_STEP_FINE = 0.1;
const float MINCURRENT_STEP_COARSE = 0.1;
const float MINCURRENT_STEP_FINE = 0.02;
const float VELOCITY_STEP_COARSE = 0.1;
const float VELOCITY_STEP_FINE = 0.02;
const float VELOCITY_MAX = 1.0;  // Maximum velocity setpoint (m/s) for safety

// Velocity PID tuning steps
const float KP_VEL_STEP_COARSE = 0.05;
const float KP_VEL_STEP_FINE = 0.01;
const float KI_VEL_STEP_COARSE = 0.01;
const float KI_VEL_STEP_FINE = 0.002;
const float KD_VEL_STEP_COARSE = 0.01;
const float KD_VEL_STEP_FINE = 0.002;

// Persistent settings (EEPROM)
struct SavedSettings {
  uint32_t magic;
  float kp;
  float ki;
  float kd;
  float baseSetpoint;
  float maxCurrent;
  float minCurrent;
  float kp_vel;
  float ki_vel;
  float kd_vel;
  float kp_yaw;
  float ki_yaw;
  float kd_yaw;
  bool yawControlEnabled;
  float angleFilterAlpha;
};
const uint32_t SETTINGS_MAGIC = 0xC45C4DEE;  // Bump to add angleFilterAlpha and reset unstable defaults

bool loadSettings();
void saveSettings();

// Control mode
enum ControlMode {
  MODE_DIAGNOSTIC,  // Direct angle → current mapping (no PID) - for testing
  MODE_PID          // Normal PID control
};
ControlMode controlMode = MODE_PID;

// Motor direction configuration
// Per-motor direction compensation (for VESC config inversion or wiring differences)
//
// BEST PRACTICES:
// 1. Motor direction signs compensate for VESC inversion or wiring differences
// 2. Velocity signs MUST match motor direction signs (if motor inverted, velocity also inverted)
// 3. Signs are applied AFTER computing control currents (balance, velocity, yaw)
// 4. This ensures correct behavior for all control modes: balance, velocity, and position
//
// CONFIGURATION:
// - If left motor spins backward with positive current: LEFT_MOTOR_DIRECTION_SIGN = -1.0
// - If right motor spins backward with positive current: RIGHT_MOTOR_DIRECTION_SIGN = -1.0
// - Velocity signs must match: LEFT_VELOCITY_SIGN = LEFT_MOTOR_DIRECTION_SIGN (same for right)
//
// See docs/hardware/motor_direction_configuration.md for detailed configuration guide
// Right motor inverted so both wheels spin same direction for balance (per hardware/VESC config).
const float LEFT_MOTOR_DIRECTION_SIGN = 1.0;   // -1.0 if left motor is inverted, 1.0 if normal
const float RIGHT_MOTOR_DIRECTION_SIGN = -1.0; // -1.0: right motor inverted (wheels were opposite; fix for balance)
const float LEFT_VELOCITY_SIGN = 1.0;          // Must match LEFT_MOTOR_DIRECTION_SIGN
const float RIGHT_VELOCITY_SIGN = -1.0;       // Must match RIGHT_MOTOR_DIRECTION_SIGN
const float VELOCITY_SIGN_DISAGREE_THRESHOLD = 0.05;  // m/s threshold to detect sign mismatch

// I2C Configuration
const uint32_t I2C_CLOCK_SPEED = 400000;  // 400kHz Fast Mode
const uint32_t IMU_UPDATE_RATE_HZ = 400;  // 400Hz update rate
const uint32_t IMU_REPORT_INTERVAL_US = 2500;  // 2500 microseconds = 2.5ms = 400Hz

// PID Update Rates
const uint32_t PID_SAMPLE_TIME_MS = 2;  // 2ms = 500Hz (angle loop - fast)
const uint32_t VELOCITY_PID_SAMPLE_TIME_MS = 50;  // 50ms = 20Hz (velocity loop - slower, stable)

// VESC Communication Rate Limiting (CRITICAL: Prevents serial buffer overflow)
const uint32_t VESC_UPDATE_INTERVAL_MS = 15;  // 15ms = 67Hz (prevents buffer overflow)

// Logging control
bool loggingEnabled = false;
bool streamData = true;
unsigned long logStartTime = 0;
unsigned long lastLogTime = 0;
const unsigned long LOG_INTERVAL = 20; // 50Hz logging

// Motor write rate limit (CRITICAL-7: match VESC read rate to prevent buffer overflow / loop jitter)
static unsigned long lastMotorWrite = 0;

// Data arrays for analysis
struct LogData {
  unsigned long timestamp;
  float roll;
  float pitch;
  float yaw;
  double pidOutput;
  float leftCurrent;
  float rightCurrent;
  bool motorsActive;
  float velocity;
  float velocitySetpoint;
};

LogData logBuffer[1000]; // 20 seconds at 50Hz
int logIndex = 0;
bool bufferFull = false;
bool logBufferFullNotified = false;

// Stiction compensation helper function
// - If command is essentially zero (< DRIVE_ZERO_EPS), return 0 (true zero stays zero)
// - If command is non-zero but below stiction threshold, jump to MIN_DRIVE_CURRENT
// - Otherwise, pass through unchanged
float applyStictionComp(float cmdA) {
  if (fabs(cmdA) < DRIVE_ZERO_EPS) return 0.0f;
  float s = (cmdA > 0) ? 1.0f : -1.0f;
  float mag = fabs(cmdA);
  if (mag < MIN_DRIVE_CURRENT) return s * MIN_DRIVE_CURRENT;
  return cmdA;
}

// Send current commands unless dry-run mode is active.
void sendMotorCurrents(float leftA, float rightA) {
  static bool dryRunZeroSent = false;
  if (motorOutputEnabled) {
    dryRunZeroSent = false;
    vescLeft.setCurrent(leftA);
    vescRight.setCurrent(rightA);
  } else if (!dryRunZeroSent) {
    // Send a single zero command on transition to dry-run, then suppress further writes.
    vescLeft.setCurrent(0.0f);
    vescRight.setCurrent(0.0f);
    dryRunZeroSent = true;
  }
}

void setup() {
  Serial.begin(2000000);
  delay(1000);
  
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  
  Serial.println("╔════════════════════════════════════════════════════╗");
  Serial.println("║  BALANCE ROBOT - CASCADED VELOCITY CONTROL       ║");
  Serial.println("║  Phase 1: Velocity Control Loop                    ║");
  Serial.println("╚════════════════════════════════════════════════════╝");
  Serial.println();
  
  Serial.println("🚀 SYSTEM CONFIGURATION:");
  Serial.printf("   • I2C Clock Speed: %d kHz (Fast Mode)\n", I2C_CLOCK_SPEED / 1000);
  Serial.printf("   • IMU Update Rate: %d Hz\n", IMU_UPDATE_RATE_HZ);
  Serial.printf("   • Angle PID Rate: %d Hz\n", 1000 / PID_SAMPLE_TIME_MS);
  Serial.printf("   • Velocity PID Rate: %d Hz\n", 1000 / VELOCITY_PID_SAMPLE_TIME_MS);
  Serial.printf("   • VESC Update Rate: %d Hz (limited)\n", 1000 / VESC_UPDATE_INTERVAL_MS);
  Serial.println();
  
  // Initialize I2C at optimized speed
  Serial.println("📡 Initializing I2C bus...");
  Wire.begin();
  Wire.setClock(I2C_CLOCK_SPEED);
  Serial.printf("   ✓ I2C initialized at %d kHz\n", I2C_CLOCK_SPEED / 1000);
  Serial.println();
  
  // Initialize IMU
  Serial.println("🔧 Initializing IMU...");
  
  // Try 0x4B first (matches working baseline)
  if (bno08x.begin_I2C(0x4B)) {
    Serial.println("   ✅ IMU initialized at 0x4B");
    
    // Enable rotation vector at optimized rate
    Serial.printf("   Enabling rotation vector at %d Hz...\n", IMU_UPDATE_RATE_HZ);
    if (!bno08x.enableReport(SH2_ROTATION_VECTOR, IMU_REPORT_INTERVAL_US)) {
      Serial.println("   ❌ Could not enable rotation vector");
    } else {
      Serial.printf("   ✅ Rotation vector enabled at %d Hz\n", IMU_UPDATE_RATE_HZ);
      imuWorking = true;
    }
  } else {
    Serial.println("   Trying alternate address 0x4A...");
    if (bno08x.begin_I2C(0x4A)) {
      Serial.println("   ✅ IMU initialized at 0x4A");
      
      // Enable rotation vector at optimized rate
      Serial.printf("   Enabling rotation vector at %d Hz...\n", IMU_UPDATE_RATE_HZ);
      if (!bno08x.enableReport(SH2_ROTATION_VECTOR, IMU_REPORT_INTERVAL_US)) {
        Serial.println("   ❌ Could not enable rotation vector");
      } else {
        Serial.printf("   ✅ Rotation vector enabled at %d Hz\n", IMU_UPDATE_RATE_HZ);
        imuWorking = true;
      }
    } else {
      Serial.println("   ❌ IMU failed to initialize at both addresses");
      Serial.println();
      Serial.println("   🔧 TROUBLESHOOTING:");
      Serial.println("      • Verify I2C wiring: SDA→Pin18, SCL→Pin19");
      Serial.println("      • Check PS0/PS1: Should be floating or GND for I2C");
      Serial.println("      • Power cycle: Disconnect power, wait 30s, reconnect");
      Serial.println("      • Try slower I2C speed: May need 100kHz initially");
    }
  }
  
  Serial.println();
  
  // Initialize VESCs
  Serial.println("⚙️  Initializing VESC motor controllers...");
  Serial1.begin(115200);
  Serial2.begin(115200);
  vescLeft.setSerialPort(&Serial1);
  vescRight.setSerialPort(&Serial2);
  Serial.println("   ✅ VESCs initialized");
  Serial.println();
  
  // Load saved settings (if available)
  if (loadSettings()) {
    Serial.println("💾 Loaded saved settings from EEPROM");
  } else {
    Serial.println("💾 No saved settings found (using defaults)");
  }
  if (FORCE_SINGLE_LOOP_MODE) {
    useVelocityLoop = false;
  }
  memset(velocityEmaBuffer, 0, sizeof(velocityEmaBuffer));
  velocityEmaIndex = 0;
  velocityMaFilled = false;

  // Validate yaw PID gains and reset if corrupted
  if (isnan(Kp_yaw) || isinf(Kp_yaw) || Kp_yaw < 0 || Kp_yaw > 10.0) {
    Kp_yaw = 0.0;
  }
  if (isnan(Ki_yaw) || isinf(Ki_yaw) || Ki_yaw < 0 || Ki_yaw > 5.0) {
    Ki_yaw = 0.0;
  }
  if (isnan(Kd_yaw) || isinf(Kd_yaw) || Kd_yaw < 0 || Kd_yaw > 5.0) {
    Kd_yaw = 0.0;
  }
  
  yawPID.SetTunings(Kp_yaw, Ki_yaw, Kd_yaw);
  
  // Ensure velocity PID is PI only (Kd always 0)
  Kd_vel = 0.0;
  velocityPID.SetTunings(Kp_vel, Ki_vel, Kd_vel);
  
  // Initialize angle PID controller (inner loop - fast)
  Serial.println("🎛️  Initializing PID controllers...");
  balancePID.SetMode(AUTOMATIC);
  balancePID.SetOutputLimits(-maxCurrent, maxCurrent);
  balancePID.SetSampleTime(PID_SAMPLE_TIME_MS);  // 500Hz update rate
  
  // === CHANGED === Velocity PID: start in MANUAL when velocity loop disabled (useVelocityLoop false).
  if (useVelocityLoop && !FORCE_SINGLE_LOOP_MODE) {
    velocityPID.SetMode(AUTOMATIC);
  } else {
    velocityPID.SetMode(MANUAL);
    angleSetpointFromVel = 0.0;
    lastAngleSetpointFromVel = 0.0;
  }
  velocityPID.SetOutputLimits(-VELOCITY_OUTPUT_MAX, VELOCITY_OUTPUT_MAX);
  velocityPID.SetSampleTime(VELOCITY_PID_SAMPLE_TIME_MS);
  
  // Initialize yaw PID controller
  yawPID.SetMode(AUTOMATIC);
  yawPID.SetOutputLimits(-maxCurrent, maxCurrent);
  yawPID.SetSampleTime(PID_SAMPLE_TIME_MS);  // 500Hz update rate
  
  Serial.printf("   ✅ Angle PID: %d Hz update rate\n", 1000 / PID_SAMPLE_TIME_MS);
  Serial.printf("   ✅ Velocity PID: %d Hz update rate\n", 1000 / VELOCITY_PID_SAMPLE_TIME_MS);
  Serial.printf("   ✅ Angle Gains: Kp=%.2f, Ki=%.2f, Kd=%.2f\n", Kp, Ki, Kd);
  Serial.printf("   ✅ Velocity Gains: Kp=%.2f, Ki=%.2f, Kd=%.2f\n", Kp_vel, Ki_vel, Kd_vel);
  Serial.printf("   ✅ Max Current: %.1fA\n", maxCurrent);
  Serial.println();
  
  Serial.println("════════════════════════════════════════════════════");
  Serial.println("✅ SYSTEM READY - Cascaded Velocity Control");
  Serial.println("════════════════════════════════════════════════════");
  Serial.println();
  Serial.println("📊 CONTROL ARCHITECTURE:");
  Serial.println("   • Cascaded PID: Velocity → Angle → Motor Current");
  Serial.println("   • IMU: 400Hz @ 400kHz I2C");
  Serial.println("   • Angle PID: 500Hz (inner loop)");
  Serial.println("   • Velocity PID: 20Hz (outer loop)");
  Serial.println();
  Serial.println("=== CONTROL MODES ===");
  Serial.println("d - Toggle Diagnostic Mode (direct angle→current, no PID)");
  Serial.println();
  Serial.println("=== VELOCITY CONTROL ===");
  Serial.println("v - Toggle velocity loop ON/OFF (default OFF = single-loop behavior)");
  Serial.println("6/V - Decrease/Increase velocity setpoint (when velocity loop ON)");
  Serial.println("0 - Set velocity setpoint to 0.0 (stop and balance)");
  Serial.printf("  useVelocityLoop=%d (0=single-loop mode)\n", useVelocityLoop ? 1 : 0);
  if (FORCE_SINGLE_LOOP_MODE) Serial.println("  FORCE_SINGLE_LOOP_MODE=1 at compile time");
  Serial.println();
  Serial.println("=== VELOCITY PID TUNING ===");
  Serial.println("w/W - Decrease/Increase velocity Kp");
  Serial.println("e/E - Decrease/Increase velocity Ki");
  Serial.println("r/R - Decrease/Increase velocity Kd");
  Serial.println();
  Serial.println("=== ANGLE PID TUNING ===");
  Serial.println("p/P - Decrease/Increase angle Kp");
  Serial.println("i/I - Decrease/Increase angle Ki");
  Serial.println("j - Decrease angle Kd");
  Serial.println("D - Increase angle Kd (NOTE: 'd' toggles diagnostic mode)");
  Serial.println("z/Z - Decrease/Increase Angle Setpoint");
  Serial.println("m/M - Decrease/Increase Max Current");
  Serial.println("q/Q - Decrease/Increase Min Current (DEPRECATED - stiction comp used)");
  Serial.println();
  Serial.println("=== OTHER COMMANDS ===");
  Serial.println("x - Show current tuning values");
  Serial.println("@ - Sync parameters to GUI (machine-readable format)");
  Serial.println("k - Save tuning values to EEPROM");
  Serial.println("g - Load tuning values from EEPROM");
  Serial.println("t - Toggle fine adjust (smaller step sizes)");
  Serial.println("o - Toggle motor output ON/OFF (dry-run keeps commanded amps in logs)");
  Serial.println();
  Serial.println("=== LOGGING COMMANDS ===");
  Serial.println("l - Start logging");
  Serial.println("s - Stop logging");
  Serial.println("b/B - Download logged data (when logging enabled)");
  Serial.println("c/C - Clear log buffer");
  Serial.println("SPACE - Pause/Resume data stream");
  Serial.println();
  Serial.println("Ready!");
}

void loop() {
  static unsigned long lastPrint = 0;
  static unsigned long lastIMUUpdate = 0;
  static unsigned long imuReadCount = 0;
  static unsigned long imuFailCount = 0;
  static unsigned long lastIMUStats = 0;
  
  // Handle serial commands (process ALL available commands to prevent buffer overflow)
  while (Serial.available()) {
    char cmd = Serial.read();
    handleCommand(cmd);
  }
  bool velocityLoopActive = useVelocityLoop && !FORCE_SINGLE_LOOP_MODE;
  
  // Get IMU data with failure tracking
  bool imuDataReceived = false;
  if (imuWorking) {
    if (bno08x.getSensorEvent(&sensorValue)) {
      imuDataReceived = true;
      imuReadCount++;
      lastIMUUpdate = millis();
    } else {
      imuFailCount++;
      // Check if IMU has stopped responding (no data for >100ms at 400Hz)
      if (lastIMUUpdate > 0 && (millis() - lastIMUUpdate > 100)) {
        static unsigned long lastIMUWarning = 0;
        if (millis() - lastIMUWarning > 2000) {  // Warn every 2 seconds
          Serial.printf("\n⚠️  IMU COMMUNICATION LOST! Last update: %lu ms ago\n", millis() - lastIMUUpdate);
          Serial.printf("   Read success: %lu, Read failures: %lu\n", imuReadCount, imuFailCount);
          Serial.println("   Possible causes: Loose wiring, I2C speed too high, weak pull-ups");
          printTuningValues();
          lastIMUWarning = millis();
          
          // Attempt I2C bus recovery
          static unsigned long lastRecoveryAttempt = 0;
          if (millis() - lastRecoveryAttempt > 5000) {
            Serial.println("   Attempting I2C bus recovery...");
            Wire.end();
            delay(10);
            Wire.begin();
            Wire.setClock(I2C_CLOCK_SPEED);
            lastRecoveryAttempt = millis();
          }
        }
      }
    }
    
    // Print I2C statistics every 5 seconds
    if (millis() - lastIMUStats > 5000) {
      unsigned long totalReads = imuReadCount + imuFailCount;
      float successRate = (totalReads > 0) ? (100.0f * imuReadCount / totalReads) : 0.0f;
      Serial.printf("📊 I2C Stats: Success=%lu (%.1f%%), Fail=%lu, Total=%lu\n", 
                    imuReadCount, successRate, imuFailCount, totalReads);
      
      if (BYPASS_VESC_FEEDBACK_WHEN_VEL_OFF && !velocityLoopActive) {
        Serial.println("📊 VESC Stats: Feedback bypassed (velocity loop OFF)");
      } else {
        unsigned long totalVesc = vescSuccessCount + vescFailCount;
        if (totalVesc > 0) {
          float vescRate = 100.0f * vescSuccessCount / totalVesc;
          // Calculate VESC read rate (Hz) - based on update interval and success rate
          float theoreticalHz = 1000.0f / VESC_UPDATE_INTERVAL_MS;  // Max theoretical rate
          float actualHz = theoreticalHz * (vescRate / 100.0f);  // Actual rate based on success
          
          if (actualHz > 0.1 && vescSuccessCount > 10) {
            Serial.printf("📊 VESC Stats: Success=%lu (%.1f%%), Fail=%lu, Rate=%.1f Hz\n", 
                          vescSuccessCount, vescRate, vescFailCount, actualHz);
          } else {
            Serial.printf("📊 VESC Stats: Success=%lu (%.1f%%), Fail=%lu, Rate=-- Hz (inactive)\n", 
                          vescSuccessCount, vescRate, vescFailCount);
          }
        } else {
          Serial.printf("📊 VESC Stats: No reads yet, Rate=-- Hz (inactive)\n");
        }
      }
      
      lastIMUStats = millis();
    }
  }
  
  if (imuDataReceived) {
    if (sensorValue.sensorId == SH2_ROTATION_VECTOR) {
      // Get quaternion
      float qw = sensorValue.un.rotationVector.real;
      float qx = sensorValue.un.rotationVector.i;
      float qy = sensorValue.un.rotationVector.j;
      float qz = sensorValue.un.rotationVector.k;
      
      // Convert to Euler angles
      float sinp = 2.0f * (qw * qy - qz * qx);
      if (abs(sinp) >= 1)
        pitch = copysign(PI / 2, sinp);
      else
        pitch = asin(sinp);
      
      roll = atan2(2.0f * (qw * qx + qy * qz), 1.0f - 2.0f * (qx * qx + qy * qy));
      yaw = atan2(2.0f * (qw * qz + qx * qy), 1.0f - 2.0f * (qy * qy + qz * qz));
      
      pitch *= 180.0f / PI;
      roll *= 180.0f / PI;
      yaw *= 180.0f / PI;
      
      // IMU orientation correction (mounted upside down) - matches working baseline
      pitch = -pitch;
      roll += 180.0f;
      if (roll > 180.0f) roll -= 360.0f;
      if (roll < -180.0f) roll += 360.0f;
    }
  }
  
  // Read VESC encoder data (rate limited to prevent buffer overflow)
  static unsigned long lastVescRead = 0;
  static float lastLeftERPM = 0.0, lastRightERPM = 0.0;
  static float lastLeftMechRPM = 0.0, lastRightMechRPM = 0.0;
  
  if (!BYPASS_VESC_FEEDBACK_WHEN_VEL_OFF || velocityLoopActive) {
    if (millis() - lastVescRead >= VESC_UPDATE_INTERVAL_MS) {
    lastVescRead = millis();
    
    // Read left VESC
    if (vescLeft.getVescValues()) {
      vescSuccessCount++;
      float leftERPM = vescLeft.data.rpm;  // VESC returns ERPM (electrical RPM)
      // Convert ERPM to mechanical RPM: mech_rpm = erpm / pole_pairs
      // Note: gear_ratio is 1.0 (direct drive), so not needed in division
      float leftMechRPM = leftERPM / POLE_PAIRS;
      // Convert mechanical RPM to m/s: mps = mech_rpm * pi * wheel_diameter / 60
      leftVelocity = leftMechRPM * RPM_TO_MPS * LEFT_VELOCITY_SIGN;
      // SIGN CHECK: Forward spin should give positive velocity
      lastLeftERPM = leftERPM;
      lastLeftMechRPM = leftMechRPM;
    } else {
      vescFailCount++;
      // Keep last known velocity (don't reset to 0)
    }
    
    // Read right VESC
    if (vescRight.getVescValues()) {
      vescSuccessCount++;
      float rightERPM = vescRight.data.rpm;  // VESC returns ERPM (electrical RPM)
      // Convert ERPM to mechanical RPM: mech_rpm = erpm / pole_pairs
      // Note: gear_ratio is 1.0 (direct drive), so not needed in division
      float rightMechRPM = rightERPM / POLE_PAIRS;
      // Convert mechanical RPM to m/s: mps = mech_rpm * pi * wheel_diameter / 60
      rightVelocity = rightMechRPM * RPM_TO_MPS * RIGHT_VELOCITY_SIGN;
      // SIGN CHECK: Forward spin should give positive velocity
      lastRightERPM = rightERPM;
      lastRightMechRPM = rightMechRPM;
    } else {
      vescFailCount++;
      // Keep last known velocity (don't reset to 0)
    }
    
    // Calculate average velocity (raw)
    avgVelocity = (leftVelocity + rightVelocity) / 2.0;
    if ((leftVelocity * rightVelocity) < 0.0 &&
        fabs(leftVelocity) > VELOCITY_SIGN_DISAGREE_THRESHOLD &&
        fabs(rightVelocity) > VELOCITY_SIGN_DISAGREE_THRESHOLD) {
      avgVelocity = 0.0;
      static unsigned long lastSignMismatchWarning = 0;
      if (millis() - lastSignMismatchWarning > 1000) {
        Serial.printf("⚠️  VEL SIGN MISMATCH: L=%.4f m/s, R=%.4f m/s -> Avg forced to 0.0\n",
                      leftVelocity, rightVelocity);
        lastSignMismatchWarning = millis();
      }
    }
    
    // Debug print for velocity computation (every 10th read = ~6.7Hz)
    // Shows: raw ERPM, mechanical RPM, and computed m/s for sign/unit verification
    static int velocityDebugCounter = 0;
    velocityDebugCounter++;
    if (velocityDebugCounter >= 10) {
      velocityDebugCounter = 0;
      Serial.printf("🔍 VEL_COMP: L_rawRPM=%.1f L_mechRPM=%.2f L_mps=%.4f | R_rawRPM=%.1f R_mechRPM=%.2f R_mps=%.4f | Avg=%.4f m/s\n", 
                   lastLeftERPM, lastLeftMechRPM, leftVelocity,
                   lastRightERPM, lastRightMechRPM, rightVelocity, avgVelocity);
    }
    
    // === CHANGED === EMA then moving-average for reliable deadband at standstill (VESC noise).
    float emaOut = VELOCITY_FILTER_ALPHA * avgVelocity + (1.0f - VELOCITY_FILTER_ALPHA) * filteredVelocity;
    velocityEmaBuffer[velocityEmaIndex] = emaOut;
    velocityEmaIndex = (velocityEmaIndex + 1) % VELOCITY_MA_SIZE;
    if (velocityEmaIndex == 0) velocityMaFilled = true;
    float sum = 0.0f;
    int n = velocityMaFilled ? VELOCITY_MA_SIZE : (velocityEmaIndex == 0 ? 1 : velocityEmaIndex);
    for (int i = 0; i < n; i++) sum += velocityEmaBuffer[i];
      filteredVelocity = sum / (float)n;
    }
  } else {
    // True angle-only mode: bypass velocity feedback path entirely.
    leftVelocity = 0.0f;
    rightVelocity = 0.0f;
    avgVelocity = 0.0f;
    filteredVelocity = 0.0f;
  }
  
  // === CHANGED === Compute every loop so angleSetpoint is correct every cycle (not only every 50ms).
  bool inDeadband = (fabs(velocitySetpoint) < 0.01f) && (fabs(filteredVelocity) < VELOCITY_DEADBAND);
  bool velocityOutputActive = velocityLoopActive && !inDeadband;

  // Guarantee angle setpoint: when velocity loop off or in deadband, exactly baseSetpoint (no creep).
  if (!velocityOutputActive) {
    angleSetpointFromVel = 0.0;
    angleSetpoint = baseSetpoint;
  } else {
    angleSetpoint = baseSetpoint + angleSetpointFromVel;
  }

  static unsigned long lastVelocityPIDUpdate = 0;
  static bool deadbandActive = false;  // State flag to prevent mode thrashing
  if (millis() - lastVelocityPIDUpdate >= VELOCITY_PID_SAMPLE_TIME_MS) {
    lastVelocityPIDUpdate = millis();

    if (!velocityOutputActive) {
      // Velocity loop off or in deadband: force angleFromVel and internal state to exactly 0 (no creep).
      angleSetpointFromVel = 0.0;
      lastAngleSetpointFromVel = 0.0;
      if (velocityLoopActive && inDeadband) {
        if (!deadbandActive) {
          deadbandActive = true;
          velocityPID.SetMode(MANUAL);
        }
      } else if (!velocityLoopActive) {
        deadbandActive = true;
        velocityPID.SetMode(MANUAL);
      }
    } else {
      // useVelocityLoop true and not in deadband: run velocity PID
      if (deadbandActive) {
        deadbandActive = false;
        velocityPID.SetMode(AUTOMATIC);
      }
      velocityInput = constrain(filteredVelocity, -2.0, 2.0);
      velocityPID.Compute();
      angleSetpointFromVel = constrain(angleSetpointFromVel, -VELOCITY_OUTPUT_MAX, VELOCITY_OUTPUT_MAX);
      float desiredChange = angleSetpointFromVel - lastAngleSetpointFromVel;
      if (fabs(desiredChange) > VELOCITY_SLEW_RATE) {
        angleSetpointFromVel = lastAngleSetpointFromVel + copysign(VELOCITY_SLEW_RATE, desiredChange);
      }
      lastAngleSetpointFromVel = angleSetpointFromVel;
    }

    // Debug at 10 Hz (unchanged)
    static int velocityDebugCounter = 0;
    velocityDebugCounter++;
    if (velocityDebugCounter >= 5) {
      velocityDebugCounter = 0;
      float velocityError = velocitySetpoint - filteredVelocity;
      Serial.printf("🔍 VEL: raw=%.3f filt=%.3f err=%.3f angleFromVel=%.4f° setpt=%.3f totalSetpt=%.2f° [Kp=%.3f Ki=%.0f]\n",
                   avgVelocity, filteredVelocity, velocityError, angleSetpointFromVel,
                   velocitySetpoint, baseSetpoint + angleSetpointFromVel, Kp_vel, Ki_vel);
    }
    static int signVerificationCounter = 0;
    signVerificationCounter++;
    if (signVerificationCounter >= 20) {
      signVerificationCounter = 0;
      Serial.printf("✅ SIGN CHECK: setpt=%.3f m/s, filtVel=%.3f m/s, angleFromVel=%.4f° | Expected: +setpt with -vel → -angleFromVel (forward tilt)\n",
                   velocitySetpoint, filteredVelocity, angleSetpointFromVel);
    }
  }

  // CASCADED BALANCE CONTROL
  float leftMotorCurrent = 0.0;
  float rightMotorCurrent = 0.0;
  float outputCurrent = 0.0;
  yawOutput = 0.0;
  
  if (imuWorking) {
    // Safety check - disable motors if robot is too far tilted
    bool isBalanceable = (abs(roll) < 25.0);
    // CRITICAL-1: Freeze PIDs when in safety cutoff so integral doesn't wind up; thaw on re-enter.
    static bool pidFrozen = false;

    if (!isBalanceable) {
      if (!pidFrozen) {
        balancePID.SetMode(MANUAL);
        velocityPID.SetMode(MANUAL);
        yawPID.SetMode(MANUAL);
        pidFrozen = true;
      }
      motorCurrent = 0.0;
      leftMotorCurrent = 0.0;
      rightMotorCurrent = 0.0;
      angleSetpointFromVel = 0.0;  // Reset velocity PID output
      if (millis() - lastMotorWrite >= VESC_UPDATE_INTERVAL_MS) {
        lastMotorWrite = millis();
        sendMotorCurrents(0.0f, 0.0f);
      }

      static unsigned long lastSafetyDebug = 0;
      if (millis() - lastSafetyDebug > 1000) {
        Serial.printf("⚠️  SAFETY: Roll=%.2f° exceeds limit (25°) - Motors DISABLED\n", roll);
        lastSafetyDebug = millis();
      }
    } else {
      if (pidFrozen) {
        balancePID.SetMode(AUTOMATIC);
        velocityPID.SetMode(AUTOMATIC);
        yawPID.SetMode(AUTOMATIC);
        pidFrozen = false;
      }
      // NORMAL CONTROL: Cascaded PID
      // === CHANGED === angleSetpoint already set above (baseSetpoint or base+angleFromVel); do not overwrite here.
      // Low-pass filter the angle input before the derivative term sees it.
      // Without this, tiny sample-to-sample IMU changes at 500 Hz become huge Kd spikes.
      static float angleFiltered = 0.0f;
      static bool angleFilterInitialized = false;
      if (!angleFilterInitialized) {
        angleFiltered = roll;
        angleFilterInitialized = true;
      }
      angleFiltered = angleFilterAlpha * roll + (1.0f - angleFilterAlpha) * angleFiltered;
      angleInput = angleFiltered;
      
      outputCurrent = 0.0;
      
      if (controlMode == MODE_DIAGNOSTIC) {
        // DIAGNOSTIC MODE: Direct angle → current mapping (no PID)
        float angleError = roll - angleSetpoint;
        
        if (abs(angleError) < 0.2) {
          outputCurrent = 0.0;
        } else {
          outputCurrent = angleError * 2.0;
          outputCurrent = constrain(outputCurrent, -maxCurrent, maxCurrent);
        }
      } else {
        // PID MODE: Normal cascaded control
        balancePID.Compute();  // Computes motorCurrent
        outputCurrent = motorCurrent;
        // NOTE: minCurrent deadzone removed - stiction compensation handles this better
      }
      
      // YAW CONTROL: Compute yaw correction to prevent unwanted rotation
      if (yawControlEnabled) {
        static bool yawSetpointInitialized = false;
        if (!yawSetpointInitialized) {
          yawSetpoint = yaw;
          yawSetpointInitialized = true;
          Serial.printf("🎯 Yaw setpoint initialized to %.2f°\n", yawSetpoint);
        }
        
        yawInput = yaw;
        yawPID.Compute();
        
        if (isnan(yawOutput) || isinf(yawOutput)) {
          yawOutput = 0.0;
        }
        
        float maxYawOutput = maxCurrent * 0.3;
        yawOutput = constrain(yawOutput, -maxYawOutput, maxYawOutput);
        
        // Split balance current and yaw correction into left/right
        // Balance: left = -outputCurrent, right = +outputCurrent (differential drive)
        // Yaw: subtract from left, add to right (or vice versa depending on yaw direction)
        leftMotorCurrent = -outputCurrent - yawOutput;
        rightMotorCurrent = outputCurrent + yawOutput;
      } else {
        // No yaw control: simple differential drive
        leftMotorCurrent = -outputCurrent;
        rightMotorCurrent = outputCurrent;
      }
      
      // Apply per-motor direction signs (for VESC inversion or wiring differences)
      // This compensates for motors that are inverted in VESC config
      leftMotorCurrent *= LEFT_MOTOR_DIRECTION_SIGN;
      rightMotorCurrent *= RIGHT_MOTOR_DIRECTION_SIGN;

      // In parity mode, match single-loop actuation when velocity and yaw are off.
      bool parityModeActive = SINGLE_LOOP_PARITY_WHEN_VEL_OFF && !velocityLoopActive && !yawControlEnabled;
      if (parityModeActive) {
        // Single-loop style small-output deadzone (instead of 0.55A stiction jump).
        if (fabs(leftMotorCurrent) < PARITY_MIN_CURRENT) leftMotorCurrent = 0.0f;
        if (fabs(rightMotorCurrent) < PARITY_MIN_CURRENT) rightMotorCurrent = 0.0f;

        leftMotorCurrent = constrain(leftMotorCurrent, -maxCurrent, maxCurrent);
        rightMotorCurrent = constrain(rightMotorCurrent, -maxCurrent, maxCurrent);

        // Match single-loop behavior: write each loop (no 67Hz gate).
        sendMotorCurrents(leftMotorCurrent, rightMotorCurrent);
        lastMotorWrite = millis();
      } else {
        // Apply stiction compensation: jump over static friction threshold
        // This ensures small PID corrections produce actual motor torque
        leftMotorCurrent = applyStictionComp(leftMotorCurrent);
        rightMotorCurrent = applyStictionComp(rightMotorCurrent);

        // Constrain to safety limits after stiction compensation
        leftMotorCurrent = constrain(leftMotorCurrent, -maxCurrent, maxCurrent);
        rightMotorCurrent = constrain(rightMotorCurrent, -maxCurrent, maxCurrent);

        // CRITICAL-7: Rate-limit motor writes to ~67 Hz (match VESC read rate)
        if (millis() - lastMotorWrite >= VESC_UPDATE_INTERVAL_MS) {
          lastMotorWrite = millis();
          sendMotorCurrents(leftMotorCurrent, rightMotorCurrent);
        }
      }
      
      // === CHANGED === Rich debug every 100 ms when logging enabled (angleInput, setpoint, vel, deadband, useVelocityLoop, current).
      static unsigned long lastDebug = 0;
      if (loggingEnabled && (millis() - lastDebug >= 100)) {
        lastDebug = millis();
        Serial.printf("DBG100: angleIn=%.3f setpt=%.3f fromVel=%.3f filtVel=%.3f inDB=%d useVel=%d motor=%.3f\n",
                     angleInput, angleSetpoint, angleSetpointFromVel, filteredVelocity,
                     inDeadband ? 1 : 0, useVelocityLoop ? 1 : 0, outputCurrent);
      } else if (!loggingEnabled && (millis() - lastDebug > 500)) {
        lastDebug = millis();
        Serial.printf("🔧 DEBUG: Roll=%.2f°, Vel=%.3f/%.3f, VelPID=%.3f°, Out=%.4fA, Left=%.4fA, Right=%.4fA\n",
                     roll, filteredVelocity, velocitySetpoint, angleSetpointFromVel, outputCurrent,
                     leftMotorCurrent, rightMotorCurrent);
      }
      
      // Log data if enabled
      if (loggingEnabled && (millis() - lastLogTime >= LOG_INTERVAL)) {
        logData(outputCurrent, leftMotorCurrent, rightMotorCurrent, yawOutput);
        lastLogTime = millis();
      }
    }
  } else {
    // IMU not working - disable motors (rate-limited)
    if (millis() - lastMotorWrite >= VESC_UPDATE_INTERVAL_MS) {
      lastMotorWrite = millis();
      sendMotorCurrents(0.0f, 0.0f);
    }
    motorCurrent = 0.0;
    leftMotorCurrent = 0.0;
    rightMotorCurrent = 0.0;
    yawOutput = 0.0;
    angleSetpointFromVel = 0.0;
  }
  
  // Print status
  if (streamData && (millis() - lastPrint >= 50)) {
    lastPrint = millis();
    
    if (imuWorking) {
      float angleError = roll - angleSetpoint;
      float yawError = yaw - yawSetpoint;
      const char* modeStr = (controlMode == MODE_DIAGNOSTIC) ? "DIAG" : "PID";
      // Log: Roll, Pitch, Yaw, RollError, YawError, Vel (filtered), RawVel (avg before filter), VelSetpt, VelPID_Out, RollPID_Out, YawPID_Out, LeftMotor, RightMotor, Setpoint, Mode, YawCtrl, Logging
      Serial.printf("R:%.2f,P:%.2f,Y:%.2f,Err:%.2f,YawErr:%.2f,Vel:%.3f,RawVel:%.3f,VelSet:%.3f,VelPID:%.3f,RollOut:%.2f,YawOut:%.2f,Left:%.2f,Right:%.2f,Setpt:%.2f,Mode:%s,Yaw:%s,Log:%s\n",
                   roll, pitch, yaw, angleError, yawError, filteredVelocity, avgVelocity, velocitySetpoint, angleSetpointFromVel,
                   motorCurrent, yawOutput, leftMotorCurrent, rightMotorCurrent, angleSetpoint,
                   modeStr, yawControlEnabled ? "ON" : "OFF", loggingEnabled ? "ON" : "OFF");
    } else {
      static unsigned long lastIMUWarning = 0;
      if (millis() - lastIMUWarning > 2000) {
        Serial.println("⚠️  IMU NOT WORKING - Motors DISABLED");
        lastIMUWarning = millis();
      }
    }
  }
  
  // Heartbeat LED
  static unsigned long lastHeartbeat = 0;
  unsigned long heartbeatTime = millis() - lastHeartbeat;
  if (heartbeatTime >= 5000) {
    lastHeartbeat = millis();
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }
}

void handleCommand(char cmd) {
  switch (cmd) {
    case 'l':
    case 'L':
      startLogging();
      break;
      
    case 's':
    case 'S':
      stopLogging();
      break;
      
    case 'w':
    case 'W':
      // Velocity Kp tuning (always available, regardless of logging status)
      if (cmd == 'w') {
        // Decrease velocity Kp
        Kp_vel -= (fineAdjust ? KP_VEL_STEP_FINE : KP_VEL_STEP_COARSE);
        if (Kp_vel < 0) Kp_vel = 0;
      } else {
        // Increase velocity Kp
        Kp_vel += (fineAdjust ? KP_VEL_STEP_FINE : KP_VEL_STEP_COARSE);
      }
      velocityPID.SetTunings(Kp_vel, Ki_vel, Kd_vel);
      Serial.printf("Velocity Kp = %.3f (%s)\n", Kp_vel, (cmd == 'w') ? "decreased" : "increased");
      break;
      
    case 'c':
    case 'C':
      clearLogBuffer();
      break;
      
    case 'b':
    case 'B':
      // Download logged data (moved from 'w'/'W' to avoid conflict with Velocity Kp tuning)
      if (loggingEnabled || logIndex > 0 || bufferFull) {
        downloadLogData();
      } else {
        Serial.println("No log data available. Use 'l' to start logging first.");
      }
      break;
      
    case ' ':
      streamData = !streamData;
      if (streamData) {
        Serial.println("\n✅ Data streaming RESUMED");
      } else {
        Serial.println("\n⏸️  Data streaming PAUSED");
      }
      break;
    
    // DIAGNOSTIC MODE
    case 'd':
      controlMode = (controlMode == MODE_DIAGNOSTIC) ? MODE_PID : MODE_DIAGNOSTIC;
      if (controlMode == MODE_DIAGNOSTIC) {
        Serial.println("🔧 DIAGNOSTIC MODE: Direct angle→current mapping (no PID)");
      } else {
        Serial.println("✅ PID MODE: Normal cascaded control enabled");
      }
      break;
    
    // === CHANGED === v = toggle velocity loop (single-loop mode when off). 6/V = decrease/increase setpoint.
    case 'v':
      if (FORCE_SINGLE_LOOP_MODE) {
        Serial.println("Velocity loop disabled by FORCE_SINGLE_LOOP_MODE (compile-time)");
      } else {
        useVelocityLoop = !useVelocityLoop;
        if (useVelocityLoop) {
          velocityPID.SetMode(AUTOMATIC);
          Serial.println("Velocity loop ENABLED");
        } else {
          velocityPID.SetMode(MANUAL);
          angleSetpointFromVel = 0.0;
          lastAngleSetpointFromVel = 0.0;
          Serial.println("Velocity loop DISABLED (single-loop mode)");
        }
      }
      break;

    case '6':
      velocitySetpoint -= (fineAdjust ? VELOCITY_STEP_FINE : VELOCITY_STEP_COARSE);
      if (velocitySetpoint < -VELOCITY_MAX) velocitySetpoint = -VELOCITY_MAX;
      Serial.printf("Velocity Setpoint = %.3f m/s (decreased)\n", velocitySetpoint);
      break;

    case 'V':
      velocitySetpoint += (fineAdjust ? VELOCITY_STEP_FINE : VELOCITY_STEP_COARSE);
      if (velocitySetpoint > VELOCITY_MAX) velocitySetpoint = VELOCITY_MAX;
      Serial.printf("Velocity Setpoint = %.3f m/s (increased)\n", velocitySetpoint);
      break;

    case '0':
      velocitySetpoint = 0.0;
      Serial.println("Velocity Setpoint = 0.000 m/s (stop and balance)");
      break;

    case 'o':
    case 'O':
      motorOutputEnabled = !motorOutputEnabled;
      if (!motorOutputEnabled) {
        sendMotorCurrents(0.0f, 0.0f);
        Serial.println("Motor output DISABLED (dry-run): no motor commands sent, commanded amps still logged");
      } else {
        Serial.println("Motor output ENABLED");
      }
      break;
    
    // VELOCITY PID TUNING (PI only - Kd always 0)
    case 'e':
    case 'E':
      if (cmd == 'e') {
        Ki_vel -= (fineAdjust ? KI_VEL_STEP_FINE : KI_VEL_STEP_COARSE);
        if (Ki_vel < 0) Ki_vel = 0;
      } else {
        Ki_vel += (fineAdjust ? KI_VEL_STEP_FINE : KI_VEL_STEP_COARSE);
      }
      Kd_vel = 0.0;  // Always keep Kd = 0 (PI only)
      velocityPID.SetTunings(Kp_vel, Ki_vel, Kd_vel);
      Serial.printf("Velocity Ki = %.3f (%s) [Keep at 0 for initial tuning - P only]\n", Ki_vel, (cmd == 'e') ? "decreased" : "increased");
      break;
    
    case 'r':
    case 'R':
      // Kd tuning disabled - velocity loop is PI only
      Serial.println("Velocity Kd is always 0 (PI only controller)");
      break;
    
    // ANGLE PID TUNING
    case 'p':
      Kp -= (fineAdjust ? KP_STEP_FINE : KP_STEP_COARSE); if (Kp < 0) Kp = 0;
      balancePID.SetTunings(Kp, Ki, Kd);
      Serial.printf("Angle Kp = %.2f (decreased)\n", Kp);
      break;
      
    case 'P':
      Kp += (fineAdjust ? KP_STEP_FINE : KP_STEP_COARSE);
      balancePID.SetTunings(Kp, Ki, Kd);
      Serial.printf("Angle Kp = %.2f (increased)\n", Kp);
      break;
    
    case 'i':
      Ki -= (fineAdjust ? KI_STEP_FINE : KI_STEP_COARSE); if (Ki < 0) Ki = 0;
      balancePID.SetTunings(Kp, Ki, Kd);
      Serial.printf("Angle Ki = %.2f (decreased)\n", Ki);
      break;
      
    case 'I':
      Ki += (fineAdjust ? KI_STEP_FINE : KI_STEP_COARSE);
      balancePID.SetTunings(Kp, Ki, Kd);
      Serial.printf("Angle Ki = %.2f (increased)\n", Ki);
      break;
    
    case 'j':
    case 'J':
      Kd -= (fineAdjust ? KD_STEP_FINE : KD_STEP_COARSE); if (Kd < 0) Kd = 0;
      balancePID.SetTunings(Kp, Ki, Kd);
      Serial.printf("Angle Kd = %.2f (decreased)\n", Kd);
      break;
      
    case 'D':
      Kd += (fineAdjust ? KD_STEP_FINE : KD_STEP_COARSE);
      balancePID.SetTunings(Kp, Ki, Kd);
      Serial.printf("Angle Kd = %.2f (increased)\n", Kd);
      break;

    case 'a':
      angleFilterAlpha -= (fineAdjust ? 0.01f : 0.05f);
      if (angleFilterAlpha < 0.0f) angleFilterAlpha = 0.0f;
      Serial.printf("Angle Filter Alpha = %.2f (decreased, more smoothing)\n", angleFilterAlpha);
      break;

    case 'A':
      angleFilterAlpha += (fineAdjust ? 0.01f : 0.05f);
      if (angleFilterAlpha > 1.0f) angleFilterAlpha = 1.0f;
      Serial.printf("Angle Filter Alpha = %.2f (increased, less smoothing)\n", angleFilterAlpha);
      break;
    
    // ANGLE SETPOINT tuning
    case 'z':
      baseSetpoint -= (fineAdjust ? SETPOINT_STEP_FINE : SETPOINT_STEP_COARSE);
      Serial.printf("Base Angle Setpoint = %.2f° (decreased)\n", baseSetpoint);
      Serial.printf("Current roll: %.2f°, Error: %.2f°\n", roll, roll - angleSetpoint);
      break;
      
    case 'Z':
      baseSetpoint += (fineAdjust ? SETPOINT_STEP_FINE : SETPOINT_STEP_COARSE);
      Serial.printf("Base Angle Setpoint = %.2f° (increased)\n", baseSetpoint);
      Serial.printf("Current roll: %.2f°, Error: %.2f°\n", roll, roll - angleSetpoint);
      break;
    
    // MAX CURRENT tuning
    case 'm':
      maxCurrent -= (fineAdjust ? MAXCURRENT_STEP_FINE : MAXCURRENT_STEP_COARSE); if (maxCurrent < 1.0) maxCurrent = 1.0;
      balancePID.SetOutputLimits(-maxCurrent, maxCurrent);
      yawPID.SetOutputLimits(-maxCurrent, maxCurrent);
      Serial.printf("Max Current = %.1fA (decreased)\n", maxCurrent);
      break;
      
    case 'M':
      maxCurrent += (fineAdjust ? MAXCURRENT_STEP_FINE : MAXCURRENT_STEP_COARSE); if (maxCurrent > 10.0) maxCurrent = 10.0;
      balancePID.SetOutputLimits(-maxCurrent, maxCurrent);
      yawPID.SetOutputLimits(-maxCurrent, maxCurrent);
      Serial.printf("Max Current = %.1fA (increased)\n", maxCurrent);
      break;
    
    // MIN CURRENT (deadzone) CONTROL
    case 'q':
      minCurrent -= (fineAdjust ? MINCURRENT_STEP_FINE : MINCURRENT_STEP_COARSE); 
      if (minCurrent < 0.0) minCurrent = 0.0;
      Serial.printf("Min Current = %.3fA (decreased)\n", minCurrent);
      break;
    
    case 'Q':
      minCurrent += (fineAdjust ? MINCURRENT_STEP_FINE : MINCURRENT_STEP_COARSE); 
      if (minCurrent > 2.0) minCurrent = 2.0;  // Cap at 2.0A (reasonable max for deadzone)
      Serial.printf("Min Current = %.3fA (increased)\n", minCurrent);
      break;

    // SAVE SETTINGS
    case 'k':
    case 'K':
      saveSettings();
      Serial.println("✅ Settings saved to EEPROM");
      break;
    
    // LOAD SETTINGS
    case 'g':
    case 'G':
      if (loadSettings()) {
        balancePID.SetTunings(Kp, Ki, Kd);
        balancePID.SetOutputLimits(-maxCurrent, maxCurrent);
        velocityPID.SetTunings(Kp_vel, Ki_vel, Kd_vel);
        yawPID.SetTunings(Kp_yaw, Ki_yaw, Kd_yaw);
        yawPID.SetOutputLimits(-maxCurrent, maxCurrent);
        Serial.println("✅ Settings loaded from EEPROM");
        printTuningValues();
      } else {
        Serial.println("⚠️  No saved settings found");
      }
      break;
    
    case 't':
    case 'T':
      fineAdjust = !fineAdjust;
      Serial.printf("Fine Adjust: %s\n", fineAdjust ? "ON" : "OFF");
      break;
    
    // YAW PID TUNING (from single-loop, kept for compatibility)
    case 'y':
      if (isnan(Kp_yaw) || isinf(Kp_yaw)) Kp_yaw = 0.5;
      Kp_yaw -= (fineAdjust ? KP_STEP_FINE : KP_STEP_COARSE); if (Kp_yaw < 0) Kp_yaw = 0;
      yawPID.SetTunings(Kp_yaw, Ki_yaw, Kd_yaw);
      Serial.printf("Yaw Kp = %.2f (decreased)\n", Kp_yaw);
      break;
      
    case 'Y':
      if (isnan(Kp_yaw) || isinf(Kp_yaw)) Kp_yaw = 0.5;
      Kp_yaw += (fineAdjust ? KP_STEP_FINE : KP_STEP_COARSE);
      yawPID.SetTunings(Kp_yaw, Ki_yaw, Kd_yaw);
      Serial.printf("Yaw Kp = %.2f (increased)\n", Kp_yaw);
      break;
    
    case 'u':
      if (isnan(Ki_yaw) || isinf(Ki_yaw)) Ki_yaw = 0.0;
      Ki_yaw -= (fineAdjust ? KI_STEP_FINE : KI_STEP_COARSE); if (Ki_yaw < 0) Ki_yaw = 0;
      if (isnan(Kp_yaw) || isinf(Kp_yaw)) Kp_yaw = 0.5;
      if (isnan(Kd_yaw) || isinf(Kd_yaw)) Kd_yaw = 0.1;
      yawPID.SetTunings(Kp_yaw, Ki_yaw, Kd_yaw);
      Serial.printf("Yaw Ki = %.2f (decreased)\n", Ki_yaw);
      break;
      
    case 'U':
      if (isnan(Ki_yaw) || isinf(Ki_yaw)) Ki_yaw = 0.0;
      Ki_yaw += (fineAdjust ? KI_STEP_FINE : KI_STEP_COARSE);
      if (isnan(Kp_yaw) || isinf(Kp_yaw)) Kp_yaw = 0.5;
      if (isnan(Kd_yaw) || isinf(Kd_yaw)) Kd_yaw = 0.1;
      yawPID.SetTunings(Kp_yaw, Ki_yaw, Kd_yaw);
      Serial.printf("Yaw Ki = %.2f (increased)\n", Ki_yaw);
      break;
    
    case 'h':
      if (isnan(Kd_yaw) || isinf(Kd_yaw)) Kd_yaw = 0.1;
      Kd_yaw -= (fineAdjust ? KD_STEP_FINE : KD_STEP_COARSE); if (Kd_yaw < 0) Kd_yaw = 0;
      if (isnan(Kp_yaw) || isinf(Kp_yaw)) Kp_yaw = 0.5;
      if (isnan(Ki_yaw) || isinf(Ki_yaw)) Ki_yaw = 0.0;
      yawPID.SetTunings(Kp_yaw, Ki_yaw, Kd_yaw);
      Serial.printf("Yaw Kd = %.2f (decreased)\n", Kd_yaw);
      break;
      
    case 'H':
      if (isnan(Kd_yaw) || isinf(Kd_yaw)) Kd_yaw = 0.1;
      Kd_yaw += (fineAdjust ? KD_STEP_FINE : KD_STEP_COARSE);
      if (isnan(Kp_yaw) || isinf(Kp_yaw)) Kp_yaw = 0.5;
      if (isnan(Ki_yaw) || isinf(Ki_yaw)) Ki_yaw = 0.0;
      yawPID.SetTunings(Kp_yaw, Ki_yaw, Kd_yaw);
      Serial.printf("Yaw Kd = %.2f (increased)\n", Kd_yaw);
      break;
    
    case 'n':
    case 'N':
      yawControlEnabled = !yawControlEnabled;
      Serial.printf("Yaw Control: %s\n", yawControlEnabled ? "ENABLED" : "DISABLED");
      break;
    
    // Show current settings
    case 'x':
    case 'X':
      printTuningValues();
      Serial.println("CURRENT STATE:");
      Serial.printf("  Roll: %.2f° (target: %.2f°, error: %.2f°)\n", roll, angleSetpoint, roll - angleSetpoint);
      Serial.printf("  Velocity: %.3f m/s (raw) / %.3f m/s (filtered) (target: %.3f m/s, error: %.3f m/s)\n", 
                   avgVelocity, filteredVelocity, velocitySetpoint, filteredVelocity - velocitySetpoint);
      Serial.printf("  Velocity PID Output: %.2f° (angle offset)\n", angleSetpointFromVel);
      Serial.printf("  Yaw: %.2f° (target: %.2f°, error: %.2f°)\n", yaw, yawSetpoint, yaw - yawSetpoint);
      Serial.printf("  Base Setpoint: %.2f°  Velocity Offset: %.2f°\n", baseSetpoint, angleSetpointFromVel);
      Serial.printf("  Motor Current: %.2fA  Yaw Output: %.2fA\n", motorCurrent, yawOutput);
      Serial.printf("  Control Mode: %s  Yaw Control: %s\n", 
                    (controlMode == MODE_DIAGNOSTIC) ? "DIAGNOSTIC" : "PID",
                    yawControlEnabled ? "ENABLED" : "DISABLED");
      Serial.println("════════════════════════════════════════════════════\n");
      break;
      
    case '@':
      // SYNC command - outputs all parameters in machine-readable format for GUI sync
      // Format: SYNC:key=value,key=value,...
      sendSyncData();
      break;
  }
}

// Send all tuning parameters in machine-readable format for GUI synchronization
void sendSyncData() {
  Serial.printf("SYNC:Kp=%.3f,Ki=%.3f,Kd=%.3f,Kp_vel=%.3f,Ki_vel=%.3f,Kd_vel=%.3f,"
                "Kp_yaw=%.3f,Ki_yaw=%.3f,Kd_yaw=%.3f,setpoint=%.3f,maxCurrent=%.2f,"
                "angleFilterAlpha=%.3f,"
                "velSetpoint=%.3f,yawEnabled=%d,fineAdjust=%d,useVelLoop=%d,"
                "leftMotorSign=%.1f,rightMotorSign=%.1f,leftVelSign=%.1f,rightVelSign=%.1f\n",
                Kp, Ki, Kd,
                Kp_vel, Ki_vel, Kd_vel,
                Kp_yaw, Ki_yaw, Kd_yaw,
                baseSetpoint, maxCurrent,
                angleFilterAlpha,
                velocitySetpoint, yawControlEnabled ? 1 : 0, fineAdjust ? 1 : 0, useVelocityLoop ? 1 : 0,
                LEFT_MOTOR_DIRECTION_SIGN, RIGHT_MOTOR_DIRECTION_SIGN,
                LEFT_VELOCITY_SIGN, RIGHT_VELOCITY_SIGN);
}

void printTuningValues() {
  Serial.println("\n╔════════════════════════════════════════════════════╗");
  Serial.println("║         CURRENT TUNING VALUES                      ║");
  Serial.println("╚════════════════════════════════════════════════════╝");
  Serial.println("VELOCITY PID CONTROL (Outer Loop - PI only):");
  Serial.printf("  useVelocityLoop: %s  (v to toggle, OFF = single-loop mode)\n", useVelocityLoop ? "ON" : "OFF");
  Serial.printf("  Motor Output: %s (o to toggle dry-run)\n", motorOutputEnabled ? "ENABLED" : "DISABLED");
  Serial.printf("  Velocity Kp: %.3f  Velocity Ki: %.3f  Velocity Kd: %.3f (always 0)\n", Kp_vel, Ki_vel, Kd_vel);
  Serial.printf("  Velocity Setpoint: %.3f m/s  Filtered Velocity: %.3f m/s  Raw Velocity: %.3f m/s\n", velocitySetpoint, filteredVelocity, avgVelocity);
  Serial.printf("  Velocity PID Output: %.3f° (angle offset, clamped to ±%.1f°, slew limited)\n", angleSetpointFromVel, VELOCITY_OUTPUT_MAX);
  Serial.printf("  Velocity Deadband: ±%.3f m/s (when setpoint = 0)  Filter alpha: %.2f\n", VELOCITY_DEADBAND, VELOCITY_FILTER_ALPHA);
  Serial.println("ANGLE PID CONTROL (Inner Loop - Balance):");
  Serial.printf("  Angle Kp: %.2f  Angle Ki: %.2f  Angle Kd: %.2f\n", Kp, Ki, Kd);
  Serial.printf("  Angle Filter Alpha: %.2f (~%.0f Hz cutoff)\n", angleFilterAlpha, -logf(1.0f - angleFilterAlpha) / (2.0f * PI * (PID_SAMPLE_TIME_MS / 1000.0f)));
  Serial.printf("  Base Angle Setpoint: %.2f°\n", baseSetpoint);
  Serial.printf("  Active Setpoint: %.2f° (base + velocity offset)\n", angleSetpoint);
  Serial.println("YAW PID CONTROL (Rotation):");
  Serial.printf("  Kp_yaw: %.2f  Ki_yaw: %.2f  Kd_yaw: %.2f\n", Kp_yaw, Ki_yaw, Kd_yaw);
  Serial.printf("  Yaw Setpoint: %.2f°  Yaw Control: %s\n", yawSetpoint, yawControlEnabled ? "ENABLED" : "DISABLED");
  Serial.println("MOTOR CONTROL:");
  Serial.printf("  Max Current: %.2fA\n", maxCurrent);
  Serial.printf("  Stiction Compensation: %.2fA (breakaway threshold)\n", MIN_DRIVE_CURRENT);
  Serial.printf("  Motor Direction Signs: Left=%.1f, Right=%.1f\n", LEFT_MOTOR_DIRECTION_SIGN, RIGHT_MOTOR_DIRECTION_SIGN);
  Serial.printf("  Velocity Signs: Left=%.1f, Right=%.1f\n", LEFT_VELOCITY_SIGN, RIGHT_VELOCITY_SIGN);
  Serial.println("SYSTEM CONFIG:");
  Serial.printf("  I2C Clock: %d kHz  IMU Rate: %d Hz\n", I2C_CLOCK_SPEED / 1000, IMU_UPDATE_RATE_HZ);
  Serial.printf("  Angle PID Rate: %d Hz  Velocity PID Rate: %d Hz (PI only)\n", 
                1000 / PID_SAMPLE_TIME_MS, 1000 / VELOCITY_PID_SAMPLE_TIME_MS);
  Serial.printf("  Control Mode: %s\n", (controlMode == MODE_DIAGNOSTIC) ? "DIAGNOSTIC" : "PID");
  Serial.printf("  Fine Adjust: %s\n", fineAdjust ? "ON" : "OFF");
  Serial.printf("  Wheel Diameter: %.3f m\n", WHEEL_DIAMETER);
  Serial.println("════════════════════════════════════════════════════");
}

void startLogging() {
  if (!loggingEnabled) {
    loggingEnabled = true;
    logStartTime = millis();
    logIndex = 0;
    bufferFull = false;
    logBufferFullNotified = false;
    Serial.println("\n📊 LOGGING STARTED");
    Serial.println("Data will be logged at 50Hz");
    Serial.println("Press 's' to stop, 'b' to download");
  } else {
    Serial.println("\n⚠️  Logging already active");
  }
}

void stopLogging() {
  if (loggingEnabled) {
    loggingEnabled = false;
    unsigned long logDuration = millis() - logStartTime;
    Serial.printf("\n📊 LOGGING STOPPED\n");
    Serial.printf("Duration: %.1f seconds\n", logDuration / 1000.0);
    Serial.printf("Samples: %d\n", bufferFull ? 1000 : logIndex);
    Serial.println("Press 'b' to download data");
  } else {
    Serial.println("\n⚠️  No active logging session");
  }
}

void logData(float rollPIDOutput, float leftMotorCurrent, float rightMotorCurrent, float yawPIDOutput) {
  if (logIndex < 1000) {
    logBuffer[logIndex].timestamp = millis() - logStartTime;
    logBuffer[logIndex].roll = roll;
    logBuffer[logIndex].pitch = pitch;
    logBuffer[logIndex].yaw = yaw;
    logBuffer[logIndex].pidOutput = rollPIDOutput;
    logBuffer[logIndex].leftCurrent = leftMotorCurrent;
    logBuffer[logIndex].rightCurrent = rightMotorCurrent;
    logBuffer[logIndex].motorsActive = (abs(leftMotorCurrent) > 0.1 || abs(rightMotorCurrent) > 0.1);
    logBuffer[logIndex].velocity = filteredVelocity;  // Log filtered velocity
    logBuffer[logIndex].velocitySetpoint = velocitySetpoint;
    logIndex++;
  } else {
    bufferFull = true;
    if (!logBufferFullNotified) {
      logBufferFullNotified = true;
      Serial.println("\n⚠️  Log buffer full! Logging stopped. Press 'b' to download data.");
      stopLogging();
    }
  }
}

void downloadLogData() {
  if (logIndex == 0 && !bufferFull) {
    Serial.println("\n⚠️  No data to download");
    return;
  }
  
  Serial.println("\n📊 DOWNLOADING LOG DATA");
  Serial.println("Format: timestamp_ms,roll_deg,pitch_deg,yaw_deg,pid_output,left_current_A,right_current_A,motors_active,velocity_mps,velocity_setpoint_mps");
  Serial.println("--- START DATA ---");
  
  int samplesToSend = bufferFull ? 1000 : logIndex;
  
  for (int i = 0; i < samplesToSend; i++) {
    Serial.printf("%lu,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%d,%.3f,%.3f\n",
                 logBuffer[i].timestamp,
                 logBuffer[i].roll,
                 logBuffer[i].pitch,
                 logBuffer[i].yaw,
                 logBuffer[i].pidOutput,
                 logBuffer[i].leftCurrent,
                 logBuffer[i].rightCurrent,
                 logBuffer[i].motorsActive ? 1 : 0,
                 logBuffer[i].velocity,
                 logBuffer[i].velocitySetpoint);
  }
  
  Serial.println("--- END DATA ---");
  Serial.printf("Downloaded %d samples\n", samplesToSend);
  
  // Print control settings for record keeping
  Serial.println("\n=== CONTROL SETTINGS (for records) ===");
  Serial.println("CASCADED PID CONTROL:");
  Serial.printf("  Velocity Kp: %.3f\n", Kp_vel);
  Serial.printf("  Velocity Ki: %.3f\n", Ki_vel);
  Serial.printf("  Velocity Kd: %.3f\n", Kd_vel);
  Serial.printf("  Angle Kp: %.2f\n", Kp);
  Serial.printf("  Angle Ki: %.2f\n", Ki);
  Serial.printf("  Angle Kd: %.2f\n", Kd);
  Serial.printf("  Base Angle Setpoint: %.2f degrees\n", baseSetpoint);
  Serial.println("MOTOR SETTINGS:");
  Serial.printf("  Max Current: %.1f A\n", maxCurrent);
  Serial.printf("  Min Current: %.1f A\n", minCurrent);
  Serial.printf("  Motor Direction Signs: Left=%.1f, Right=%.1f\n", LEFT_MOTOR_DIRECTION_SIGN, RIGHT_MOTOR_DIRECTION_SIGN);
  Serial.printf("  Velocity Signs: Left=%.1f, Right=%.1f\n", LEFT_VELOCITY_SIGN, RIGHT_VELOCITY_SIGN);
  Serial.println("SYSTEM INFO:");
  Serial.printf("  IMU Update Rate: %d Hz\n", IMU_UPDATE_RATE_HZ);
  Serial.printf("  I2C Clock: %d kHz\n", I2C_CLOCK_SPEED / 1000);
  Serial.printf("  Control Architecture: CASCADED (Velocity→Angle→Current)\n");
  Serial.printf("  Control Mode: %s\n", (controlMode == MODE_DIAGNOSTIC) ? "DIAGNOSTIC" : "PID");
  Serial.printf("  Wheel Diameter: %.3f m\n", WHEEL_DIAMETER);
}

bool loadSettings() {
  SavedSettings settings;
  EEPROM.get(0, settings);
  if (settings.magic != SETTINGS_MAGIC) {
    return false;
  }
  Kp = settings.kp;
  Ki = settings.ki;
  Kd = settings.kd;
  baseSetpoint = settings.baseSetpoint;
  maxCurrent = settings.maxCurrent;
  minCurrent = settings.minCurrent;
  Kp_vel = settings.kp_vel;
  Ki_vel = settings.ki_vel;
  Kd_vel = settings.kd_vel;
  Kp_yaw = settings.kp_yaw;
  Ki_yaw = settings.ki_yaw;
  Kd_yaw = settings.kd_yaw;
  yawControlEnabled = settings.yawControlEnabled;
  angleFilterAlpha = settings.angleFilterAlpha;
  
  // Validate and reset if corrupted (PI only - Kd always 0)
  if (isnan(Kp_vel) || isinf(Kp_vel) || Kp_vel < 0 || Kp_vel > 10.0) {
    Kp_vel = 1.0;  // Start at 1.0, then increase to 2.0, 3.0 while keeping clamp/slew limits
  }
  if (isnan(Ki_vel) || isinf(Ki_vel) || Ki_vel < 0 || Ki_vel > 5.0) {
    Ki_vel = 0.0;  // Keep at 0 for initial tuning
  }
  Kd_vel = 0.0;  // Always 0 - PI only controller
  if (isnan(Kp_yaw) || isinf(Kp_yaw) || Kp_yaw < 0 || Kp_yaw > 10.0) {
    Kp_yaw = 0.0;
  }
  if (isnan(Ki_yaw) || isinf(Ki_yaw) || Ki_yaw < 0 || Ki_yaw > 5.0) {
    Ki_yaw = 0.0;
  }
  if (isnan(Kd_yaw) || isinf(Kd_yaw) || Kd_yaw < 0 || Kd_yaw > 5.0) {
    Kd_yaw = 0.0;
  }
  if (isnan(angleFilterAlpha) || isinf(angleFilterAlpha) || angleFilterAlpha < 0.0f || angleFilterAlpha > 1.0f) {
    angleFilterAlpha = 0.3f;
  }
  
  velocityPID.SetTunings(Kp_vel, Ki_vel, Kd_vel);
  balancePID.SetTunings(Kp, Ki, Kd);
  yawPID.SetTunings(Kp_yaw, Ki_yaw, Kd_yaw);
  if (!useVelocityLoop || FORCE_SINGLE_LOOP_MODE) {
    angleSetpointFromVel = 0.0;
    angleSetpoint = baseSetpoint;
  } else {
    angleSetpoint = baseSetpoint + angleSetpointFromVel;
  }
  return true;
}

void saveSettings() {
  SavedSettings settings;
  settings.magic = SETTINGS_MAGIC;
  settings.kp = Kp;
  settings.ki = Ki;
  settings.kd = Kd;
  settings.baseSetpoint = baseSetpoint;
  settings.maxCurrent = maxCurrent;
  settings.minCurrent = minCurrent;
  settings.kp_vel = Kp_vel;
  settings.ki_vel = Ki_vel;
  settings.kd_vel = Kd_vel;
  settings.kp_yaw = Kp_yaw;
  settings.ki_yaw = Ki_yaw;
  settings.kd_yaw = Kd_yaw;
  settings.yawControlEnabled = yawControlEnabled;
  settings.angleFilterAlpha = angleFilterAlpha;
  EEPROM.put(0, settings);
}

void clearLogBuffer() {
  logIndex = 0;
  bufferFull = false;
  logBufferFullNotified = false;
  Serial.println("\n🗑️  Log buffer cleared");
}
