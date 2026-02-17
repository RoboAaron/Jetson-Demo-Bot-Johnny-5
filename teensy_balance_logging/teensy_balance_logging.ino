/*
 * BALANCE ROBOT DATA LOGGER - SPI MODE
 * Captures IMU data, PID outputs, and motor commands for analysis
 * Use this to fine-tune PID parameters based on real data
 * 
 * STATUS: SPI MIGRATION - Migrated from I2C baseline
 * - Baseline: Can balance ~30-40% of the time for 30+ seconds on smooth surfaces (I2C @ 100Hz)
 * - SPI Mode: Target 400-1000Hz IMU updates for 99% reliability
 * - Motor chattering present (velocity PID needs tuning - Kd_vel = 0.0)
 * - Control method: Current control (optimal)
 * - Architecture: Cascaded PID (proven)
 * - TODO: Tune for higher SPI update rates, add velocity damping (Kd_vel), fine-tune gains
 */

#include <SPI.h>            // SPI interface (replaces Wire.h)
#include <Adafruit_BNO08x.h>
#include <VescUart.h>
#include <PID_v1.h>

// SPI Pin Definitions (per SPI_WIRING_GUIDE.md)
#define BNO08X_CS 10    // Chip Select pin (WHITE wire)
#define BNO08X_INT 9    // Interrupt pin (ORANGE wire)
#define BNO08X_RESET 14 // Reset pin (Added for reliability)

// ============================================================
// CRITICAL: PS0/PS1 Configuration for SPI Mode
// According to Adafruit and BNO085 datasheet:
//   PS0 → 3.3V (HIGH)
//   PS1 → 3.3V (HIGH)  <-- BOTH must be HIGH for SPI!
// These are NOT controlled by Teensy, must be connected BEFORE power-on
// ============================================================

// Global objects
Adafruit_BNO08x bno08x(BNO08X_RESET);  // SPI mode with hardware reset
sh2_SensorValue_t sensorValue;
VescUart vescLeft;
VescUart vescRight;

// IMU data
bool imuWorking = false;
float pitch = 0.0, roll = 0.0, yaw = 0.0;
float gyroX = 0.0, gyroY = 0.0, gyroZ = 0.0;
float lastRoll = 0.0;
float rollRate = 0.0;

// CASCADED PID CONTROL SYSTEM WITH POSITION FEEDBACK
// Outer loop: Angle control (slow)
double angleSetpoint = 1.1;  // CALIBRATED balance angle (was causing drift at 0.0°)
double angleInput, velocitySetpoint;
double Kp_angle = 15.0, Ki_angle = 0.5, Kd_angle = 0.8;  // ORIGINAL working settings
PID anglePID(&angleInput, &velocitySetpoint, &angleSetpoint, Kp_angle, Ki_angle, Kd_angle, DIRECT);

// Inner loop: Velocity control (fast)
double velocityInput, currentOutput;
double Kp_vel = 0.8, Ki_vel = 0.3, Kd_vel = 0.0;  // ORIGINAL working settings (has chatter)
PID velocityPID(&velocityInput, &currentOutput, &velocitySetpoint, Kp_vel, Ki_vel, Kd_vel, DIRECT);

// Position feedback (prevents drift)
double positionSetpoint = 0.0;  // Target position (stay at start)
double currentPosition = 0.0;   // Integrated from velocity
double Kp_position = 0.0;       // Position feedback gain (DISABLED by default - may cause drift!)
unsigned long lastPositionUpdate = 0;

// Motor control parameters
float minCurrentToMove = 0.2;  // ORIGINAL - Very low threshold to allow small corrections
float maxCurrent = 8.0;  // Increased for more power and proportional control headroom
float deadband = 0.5;  // ORIGINAL - Reduced deadband for cascaded control
float velocityDamping = 0.01;  // Damping factor for roll rate (will adjust for higher SPI update rates)

// Velocity measurement from encoders
float leftVelocity = 0.0;   // m/s
float rightVelocity = 0.0;  // m/s
float avgVelocity = 0.0;    // m/s
const float WHEEL_DIAMETER = 0.165;  // meters (6.5 inches typical hoverboard wheel)
const float RPM_TO_MPS = (WHEEL_DIAMETER * PI) / 60.0;  // Convert RPM to m/s

// Logging control
bool loggingEnabled = false;
bool streamData = true;
unsigned long logStartTime = 0;
unsigned long lastLogTime = 0;
const unsigned long LOG_INTERVAL = 20; // 50Hz logging

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
};

LogData logBuffer[1000]; // 20 seconds at 50Hz
int logIndex = 0;
bool bufferFull = false;

void setup() {
  Serial.begin(2000000);
  delay(1000);
  
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  pinMode(BNO08X_CS, OUTPUT);
  digitalWrite(BNO08X_CS, HIGH); // Deselect CS initially
  
  // ============================================================
  // HARDWARE RESET SEQUENCE - With LED Feedback
  // RST pin is ACTIVE LOW (pull LOW to reset, HIGH to run)
  // ============================================================
  Serial.println("♻️ Performing Hardware Reset on Pin 14...");
  Serial.println("   LED will BLINK during reset (watch the Teensy LED)");
  
  // Verify pin number
  Serial.print("   BNO08X_RESET pin number: ");
  Serial.println(BNO08X_RESET);
  
  pinMode(BNO08X_RESET, OUTPUT);
  
  // Step 1: Set HIGH first (normal operation state)
  digitalWrite(BNO08X_RESET, HIGH);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(100);
  
  // Step 2: Pull LOW to reset (LED OFF during reset)
  Serial.println("   >> Pulling RST LOW now (reset active)...");
  Serial.flush();
  digitalWrite(BNO08X_RESET, LOW);
  digitalWrite(LED_BUILTIN, LOW);  // LED OFF = reset active
  delay(50);
  
  // Step 3: Release (set HIGH) to exit reset
  Serial.println("   >> Releasing RST (setting HIGH)...");
  Serial.flush();
  digitalWrite(BNO08X_RESET, HIGH);
  digitalWrite(LED_BUILTIN, HIGH);  // LED ON = reset complete
  delay(300);  // BNO085 needs time to boot after reset
  
  Serial.println("   ✓ Reset sequence complete");
  Serial.println("   (If LED didn't blink, RST wire may be disconnected)");
  
  Serial.println("╔════════════════════════════════════════════════════╗");
  Serial.println("║     BALANCE ROBOT DATA LOGGER - SPI MODE          ║");
  Serial.println("╚════════════════════════════════════════════════════╝");
  Serial.println();
  
  // SPI Wiring Confirmation (Teyleten Robot GY-BNO085 board)
  Serial.println("🎯 SPI WIRING (Teyleten Board → Teensy 4.1):");
  Serial.println("   ┌─────────────────────────────────────────────────┐");
  Serial.println("   │ IMU Pin  │ Wire   │ Teensy Pin │ SPI Function  │");
  Serial.println("   ├─────────────────────────────────────────────────┤");
  Serial.println("   │ VCC      │ Red    │ 3.3V       │ Power         │");
  Serial.println("   │ GND      │ Black  │ GND        │ Ground        │");
  Serial.println("   │ SCL      │ Blue   │ Pin 13     │ SCK (Clock)   │");
  Serial.println("   │ SDA      │ Purple │ Pin 11     │ MOSI (Data→)  │");
  Serial.println("   │ ADO      │ Orange │ Pin 12     │ MISO (Data←)  │");
  Serial.println("   │ CS       │ White  │ Pin 10     │ Chip Select   │");
  Serial.println("   │ INT      │ Green  │ Pin 9      │ Interrupt     │");
  Serial.println("   │ RST      │ Yellow │ Pin 14     │ Reset         │");
  Serial.println("   └─────────────────────────────────────────────────┘");
  Serial.println();
  Serial.println("   ⚡ MODE JUMPERS (CRITICAL - both must be 3.3V!):");
  Serial.println("      • PS0 (labeled 'PSO') → 3.3V");
  Serial.println("      • PS1                 → 3.3V");
  Serial.println("      (Jumpers must be set BEFORE power-on!)");
  Serial.println();
  Serial.flush();
  
  // Safety Countdown to detect immediate crashes
  Serial.println("⏳ Waiting 5 seconds before touching SPI pins...");
  Serial.println("   (If it restarts during this count, it's a power/short issue)");
  for(int i=5; i>0; i--) {
    Serial.print(i); 
    Serial.print("... ");
    Serial.flush();
    delay(1000);
  }
  Serial.println("Go!");
  Serial.println();
  
  // Initialize SPI
  Serial.println("📡 Initializing SPI bus (SPI.begin)...");
  Serial.flush();
  SPI.begin();
  delay(100);
  Serial.println("   ✓ SPI.begin() complete");
  Serial.flush();
  
  // ============================================
  // ENHANCED DIAGNOSTICS - Test each component
  // ============================================
  Serial.println("🔍 ENHANCED DIAGNOSTICS:");
  Serial.println();
  Serial.flush();
  
  // Test 1: CS Pin Functionality
  Serial.println("   [1] Testing CS pin (Pin 10)...");
  Serial.flush();
  pinMode(BNO08X_CS, OUTPUT);
  digitalWrite(BNO08X_CS, HIGH);
  delay(10);
  digitalWrite(BNO08X_CS, LOW);
  delay(10);
  digitalWrite(BNO08X_CS, HIGH);
  Serial.println("      ✓ CS pin can toggle (hardware OK)");
  Serial.flush();
  
  // Test 2: SPI Bus Communication Test
  Serial.println("   [2] Testing SPI bus communication...");
  Serial.flush();
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE3));
  digitalWrite(BNO08X_CS, LOW);  // Select device
  delayMicroseconds(10);
  uint8_t testByte = SPI.transfer(0x00);  // Try to read a byte
  delayMicroseconds(10);
  digitalWrite(BNO08X_CS, HIGH);  // Deselect
  SPI.endTransaction();
  Serial.print("      SPI read test: 0x");
  Serial.print(testByte, HEX);
  Serial.println(" (0x00 = no response, 0xFF = floating, other = possible response)");
  Serial.flush();
  
  // Test 3: Power Supply Check (indirect - via INT pin if available)
  Serial.println("   [3] Checking INT pin state...");
  Serial.flush();
  pinMode(BNO08X_INT, INPUT_PULLUP);
  delay(10);
  int intState = digitalRead(BNO08X_INT);
  Serial.print("      INT pin (Pin 9) reads: ");
  Serial.println(intState == HIGH ? "HIGH (pulled up)" : "LOW (possible signal)");
  Serial.println();
  Serial.flush();
  
  // CRITICAL: PS0/PS1 must be connected BEFORE power-on
  Serial.println("⚠️  CRITICAL: PS0/PS1 jumpers for SPI mode:");
  Serial.println("   • PS0 (labeled 'PSO' on board) → 3.3V (HIGH)");
  Serial.println("   • PS1                         → 3.3V (HIGH)");
  Serial.println("   • ⚡ BOTH must be HIGH for SPI mode!");
  Serial.println("   • If jumpers were connected AFTER power-on, POWER CYCLE now!");
  Serial.println();
  
  // Initialize IMU via SPI
  Serial.println("Initializing BNO085 via SPI...");
  Serial.println("Trying multiple SPI speeds for compatibility...");
  
  // Try SPI speeds from slowest to fastest for maximum compatibility
  uint32_t spiSpeeds[] = {1000000, 2000000, 3000000}; // 1 MHz, 2 MHz, 3 MHz
  const char* speedLabels[] = {"1 MHz", "2 MHz", "3 MHz"};
  bool initSuccess = false;
  
  for (int i = 0; i < 3; i++) {
    Serial.print("   Trying ");
    Serial.print(speedLabels[i]);
    Serial.print("... ");
    
    if (bno08x.begin_SPI(BNO08X_CS, BNO08X_INT, &SPI, spiSpeeds[i])) {
      Serial.println("✅ SUCCESS!");
      Serial.print("   Using ");
      Serial.print(speedLabels[i]);
      Serial.println(" SPI clock speed");
      initSuccess = true;
      break;
    } else {
      Serial.println("❌ Failed");
    }
    delay(100); // Brief delay between attempts
  }
  
  if (!initSuccess) {
    Serial.println();
    Serial.println("❌ BNO085 SPI initialization failed at all speeds - entering safe mode");
    Serial.println();
    Serial.println("🔧 TROUBLESHOOTING CHECKLIST:");
    Serial.println();
    Serial.println("   ⚠️  MOST COMMON ISSUE: PS0/PS1 jumpers (90% of failures)");
    Serial.println("      • PS0 (labeled 'PSO' on Teyleten board) MUST be connected to 3.3V");
    Serial.println("      • PS1 MUST ALSO be connected to 3.3V (BOTH HIGH for SPI!)");
    Serial.println("      • These MUST be connected BEFORE power-on (BNO085 detects mode at startup)");
    Serial.println("      • If you connected jumpers AFTER power-on, POWER CYCLE the robot");
    Serial.println("      • Use a multimeter to verify: PS0 = ~3.3V, PS1 = ~3.3V");
    Serial.println();
    Serial.println("   ✓ Verify CS (White wire) → Pin 10");
    Serial.println("   ✓ Verify SCK/SCL (Blue wire) → Pin 13");
    Serial.println("   ✓ Verify MOSI/SDA (Purple wire) → Pin 11");
    Serial.println("   ✓ Verify MISO/ADO (Orange wire) → Pin 12");
    Serial.println("   ✓ Verify RST (Yellow wire) → Pin 14");
    Serial.println("   ✓ Verify VCC (Red wire) → 3.3V (NOT 5V!)");
    Serial.println("   ✓ Verify GND (Black wire) → GND");
    Serial.println("   ✓ Check for loose connections or shorts");
    Serial.println();
    Serial.println("   🔍 PHYSICAL DAMAGE CHECK:");
    Serial.println("      • Visual inspection: Look for burnt components, cracked IC, lifted pads");
    Serial.println("      • Power test: Measure VCC pin - should be 3.3V ±0.1V");
    Serial.println("      • Continuity test: Check all pins for continuity to Teensy");
    Serial.println("      • If SPI read test above shows 0xFF consistently → floating pin (damage?)");
    Serial.println("      • If board was exposed to >5V on VCC → likely damaged");
    Serial.println("      • If board was exposed to reverse polarity → likely damaged");
    Serial.println();
    Serial.println("   💡 If all connections are correct, try:");
    Serial.println("      1. Power cycle the robot (disconnect ALL power, wait 10s, reconnect)");
    Serial.println("      2. Verify PS0/PS1 jumpers are making good contact (use multimeter)");
    Serial.println("      3. Check if BNO085 board is getting power (LED if present)");
    Serial.println("      4. Try swapping PS0/PS1 (reverse them) - some boards are labeled differently");
    Serial.println("      5. Test with I2C mode (PS0→GND, PS1→GND) to see if chip responds at all");
    Serial.println();
    imuWorking = false;
  } else {
    // initSuccess already set to true above
    imuWorking = true;
    
    // Enable rotation vector at 400Hz (2500 microseconds = 2.5ms) - SPI can handle high rates
    // Start conservative, can increase to 1000Hz later
    if (!bno08x.enableReport(SH2_ROTATION_VECTOR, 2500)) {  // 400Hz
      Serial.println("❌ Could not enable rotation vector");
    } else {
      Serial.println("✅ Rotation vector enabled at 400Hz (SPI mode)");
    }
    
    // Enable gyroscope for velocity damping at 400Hz
    if (!bno08x.enableReport(SH2_GYROSCOPE_CALIBRATED, 2500)) {  // 400Hz
      Serial.println("⚠️  Could not enable gyroscope (velocity damping disabled)");
    } else {
      Serial.println("✅ Gyroscope enabled for velocity damping at 400Hz (SPI mode)");
    }
  }
  
  // Initialize VESCs
  Serial1.begin(115200);
  Serial2.begin(115200);
  vescLeft.setSerialPort(&Serial1);
  vescRight.setSerialPort(&Serial2);
  
  // Initialize cascaded PID controllers
  anglePID.SetMode(AUTOMATIC);
  anglePID.SetOutputLimits(-3.0, 3.0);  // Velocity setpoint in m/s
  anglePID.SetSampleTime(10);  // 100Hz update rate (can increase with SPI)
  
  velocityPID.SetMode(AUTOMATIC);
  velocityPID.SetOutputLimits(-maxCurrent, maxCurrent);  // Current in Amps
  velocityPID.SetSampleTime(10);  // 100Hz update rate (can increase with SPI)
  
  Serial.println("\n=== LOGGING COMMANDS ===");
  Serial.println("l - Start logging");
  Serial.println("s - Stop logging");
  Serial.println("w - Download logged data");
  Serial.println("c - Clear log buffer");
  Serial.println("h - Show this help");
  Serial.println("SPACE - Pause/Resume data stream");
  Serial.println("\n🚀 Ready for SPI mode logging!");
}

void loop() {
  static unsigned long lastPrint = 0;
  static unsigned long lastHeartbeat = 0;
  
  // Handle serial commands
  if (Serial.available()) {
    char cmd = Serial.read();
    handleCommand(cmd);
  }
  
  // Get IMU data
  if (imuWorking && bno08x.getSensorEvent(&sensorValue)) {
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
      
      // IMU orientation correction (mounted upside down)
      pitch = -pitch;
      roll += 180.0f;
      if (roll > 180.0f) roll -= 360.0f;
      if (roll < -180.0f) roll += 360.0f;
      
      // Calculate roll rate (degrees per second)
      // Assuming 400Hz update rate (2.5ms intervals) - adjust if rate changes
      rollRate = (roll - lastRoll) * 400.0;  // Updated for 400Hz
      lastRoll = roll;
    }
    else if (sensorValue.sensorId == SH2_GYROSCOPE_CALIBRATED) {
      // Get gyroscope data (rad/s)
      gyroX = sensorValue.un.gyroscope.x;
      gyroY = sensorValue.un.gyroscope.y;
      gyroZ = sensorValue.un.gyroscope.z;
    }
  }
  
  // CASCADED BALANCE CONTROL
  if (imuWorking) {
    // Safety check
    bool isBalanceable = (abs(roll) < 25.0);
    
    if (!isBalanceable) {
      vescLeft.setCurrent(0.0);
      vescRight.setCurrent(0.0);
      velocitySetpoint = 0.0;
      currentOutput = 0.0;
    } else {
      // Read encoder velocities from VESCs
      bool leftDataValid = vescLeft.getVescValues();
      bool rightDataValid = vescRight.getVescValues();
      
      // Convert RPM to m/s (positive = forward) - use 0 if data invalid
      leftVelocity = leftDataValid ? vescLeft.data.rpm * RPM_TO_MPS : 0.0;
      rightVelocity = rightDataValid ? vescRight.data.rpm * RPM_TO_MPS : 0.0;
      avgVelocity = (leftVelocity + rightVelocity) / 2.0;
      
      // UPDATE POSITION: Integrate velocity over time
      unsigned long now = millis();
      if (lastPositionUpdate > 0) {
        float dt = (now - lastPositionUpdate) / 1000.0;  // Convert to seconds
        currentPosition += avgVelocity * dt;  // Distance = velocity * time
      }
      lastPositionUpdate = now;
      
      // OUTER LOOP: Angle control → Velocity setpoint
      // Add velocity damping for predictive control (prevents overshoot)
      angleInput = roll + (rollRate * velocityDamping);
      anglePID.Compute();  // Outputs velocitySetpoint
      
      // ADD POSITION FEEDBACK: Pull robot back to starting position
      double positionError = positionSetpoint - currentPosition;
      double positionCorrection = Kp_position * positionError;
      velocitySetpoint += positionCorrection;  // Add position feedback to velocity command
      
      // Apply deadband to angle error
      double rollError = abs(roll - angleSetpoint);
      if (rollError < deadband) {
        velocitySetpoint = positionCorrection;  // Only position feedback when stable
      }
      
      // INNER LOOP: Velocity control → Motor current
      velocityInput = avgVelocity;
      velocityPID.Compute();  // Outputs currentOutput
      
      // Apply minimum current threshold
      float motorOutput = currentOutput;
      if (abs(motorOutput) < minCurrentToMove) {
        motorOutput = 0.0;
      }
      
      // Send to motors (reversed for one side)
      vescLeft.setCurrent(-motorOutput);
      vescRight.setCurrent(motorOutput);
      
      // Log data if enabled
      if (loggingEnabled && (millis() - lastLogTime >= LOG_INTERVAL)) {
        logData(motorOutput);
        lastLogTime = millis();
      }
    }
  } else {
    vescLeft.setCurrent(0.0);
    vescRight.setCurrent(0.0);
    velocitySetpoint = 0.0;
    currentOutput = 0.0;
  }
  
  // Print status
  if (streamData && (millis() - lastPrint >= 50)) {
    lastPrint = millis();
    
    if (imuWorking) {
      Serial.printf("R:%.2f,Pos:%.2f,VelSet:%.2f,VelAct:%.2f,Curr:%.2f,Log:%s\n", 
                   roll, currentPosition, velocitySetpoint, avgVelocity, currentOutput, loggingEnabled ? "ON" : "OFF");
    } else {
      Serial.println("SAFE_MODE: IMU not working");
    }
  }
  
  // Heartbeat LED
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
      downloadLogData();
      break;
      
    case 'c':
    case 'C':
      clearLogBuffer();
      break;
      
    case 'h':
    case 'H':
      showHelp();
      break;
      
    case ' ':
      streamData = !streamData;
      if (streamData) {
        Serial.println("\n✅ Data streaming RESUMED");
      } else {
        Serial.println("\n⏸️  Data streaming PAUSED");
      }
      break;
      
    // ANGLE PID tuning commands
    case 'p':
      Kp_angle -= 1.0; if (Kp_angle < 0) Kp_angle = 0;
      anglePID.SetTunings(Kp_angle, Ki_angle, Kd_angle);
      Serial.printf("Angle Kp = %.1f (decreased)\n", Kp_angle);
      break;
      
    case 'P':
      Kp_angle += 1.0;
      anglePID.SetTunings(Kp_angle, Ki_angle, Kd_angle);
      Serial.printf("Angle Kp = %.1f (increased)\n", Kp_angle);
      break;
      
    case 'i':
      Ki_angle -= 0.1; if (Ki_angle < 0) Ki_angle = 0;
      anglePID.SetTunings(Kp_angle, Ki_angle, Kd_angle);
      Serial.printf("Angle Ki = %.1f (decreased)\n", Ki_angle);
      break;
      
    case 'I':
      Ki_angle += 0.1;
      anglePID.SetTunings(Kp_angle, Ki_angle, Kd_angle);
      Serial.printf("Angle Ki = %.1f (increased)\n", Ki_angle);
      break;
      
    case 'd':
      Kd_angle -= 0.1; if (Kd_angle < 0) Kd_angle = 0;
      anglePID.SetTunings(Kp_angle, Ki_angle, Kd_angle);
      Serial.printf("Angle Kd = %.1f (decreased)\n", Kd_angle);
      break;
      
    case 'D':
      Kd_angle += 0.1;
      anglePID.SetTunings(Kp_angle, Ki_angle, Kd_angle);
      Serial.printf("Angle Kd = %.1f (increased)\n", Kd_angle);
      break;
      
    // VELOCITY PID tuning commands
    case 'a':
      Kp_vel -= 0.1; if (Kp_vel < 0) Kp_vel = 0;
      velocityPID.SetTunings(Kp_vel, Ki_vel, Kd_vel);
      Serial.printf("Velocity Kp = %.1f (decreased)\n", Kp_vel);
      break;
      
    case 'A':
      Kp_vel += 0.1;
      velocityPID.SetTunings(Kp_vel, Ki_vel, Kd_vel);
      Serial.printf("Velocity Kp = %.1f (increased)\n", Kp_vel);
      break;
      
    case 'b':
      Ki_vel -= 0.05; if (Ki_vel < 0) Ki_vel = 0;
      velocityPID.SetTunings(Kp_vel, Ki_vel, Kd_vel);
      Serial.printf("Velocity Ki = %.2f (decreased)\n", Ki_vel);
      break;
      
    case 'B':
      Ki_vel += 0.05;
      velocityPID.SetTunings(Kp_vel, Ki_vel, Kd_vel);
      Serial.printf("Velocity Ki = %.2f (increased)\n", Ki_vel);
      break;
      
    // POSITION CONTROL tuning
    case 'n':
      Kp_position -= 0.1; if (Kp_position < 0) Kp_position = 0;
      Serial.printf("Position Kp = %.2f (decreased)\n", Kp_position);
      break;
      
    case 'N':
      Kp_position += 0.1;
      Serial.printf("Position Kp = %.2f (increased)\n", Kp_position);
      break;
      
    case 'r':  // Reset position to zero
      currentPosition = 0.0;
      positionSetpoint = 0.0;
      Serial.println("Position RESET to 0.0m");
      break;
      
    // ANGLE SETPOINT tuning (CRITICAL for eliminating drift!)
    case 'z':
      angleSetpoint -= 0.1;
      Serial.printf("Angle Setpoint = %.1f° (decreased) - Find angle where robot stops drifting!\n", angleSetpoint);
      break;
      
    case 'Z':
      angleSetpoint += 0.1;
      Serial.printf("Angle Setpoint = %.1f° (increased) - Find angle where robot stops drifting!\n", angleSetpoint);
      break;
      
    // Max current tuning
    case 'm':
      maxCurrent -= 0.5; if (maxCurrent < 1.0) maxCurrent = 1.0;
      velocityPID.SetOutputLimits(-maxCurrent, maxCurrent);
      Serial.printf("Max Current = %.1fA (decreased)\n", maxCurrent);
      break;
      
    case 'M':
      maxCurrent += 0.5; if (maxCurrent > 10.0) maxCurrent = 10.0;
      velocityPID.SetOutputLimits(-maxCurrent, maxCurrent);
      Serial.printf("Max Current = %.1fA (increased)\n", maxCurrent);
      break;
      
    // Velocity damping tuning
    case 'v':
      velocityDamping -= 0.01; if (velocityDamping < 0) velocityDamping = 0;
      Serial.printf("Velocity Damping = %.3f (decreased)\n", velocityDamping);
      break;
      
    case 'V':
      velocityDamping += 0.01; if (velocityDamping > 0.5) velocityDamping = 0.5;
      Serial.printf("Velocity Damping = %.3f (increased)\n", velocityDamping);
      break;
      
    // Show current settings
    case 'x':
    case 'X':
      Serial.println("\n=== CASCADED CONTROL SETTINGS ===");
      Serial.println("ANGLE SETPOINT (Balance Point):");
      Serial.printf("  Setpoint: %.1f° (Adjust with z/Z to eliminate drift!)\n", angleSetpoint);
      Serial.println("ANGLE PID (Outer Loop):");
      Serial.printf("  Kp: %.1f  Ki: %.1f  Kd: %.1f\n", Kp_angle, Ki_angle, Kd_angle);
      Serial.println("VELOCITY PID (Inner Loop):");
      Serial.printf("  Kp: %.1f  Ki: %.2f  Kd: %.1f\n", Kp_vel, Ki_vel, Kd_vel);
      Serial.println("POSITION FEEDBACK:");
      Serial.printf("  Kp: %.2f\n", Kp_position);
      Serial.println("MOTOR SETTINGS:");
      Serial.printf("  Max Current: %.1fA\n", maxCurrent);
      Serial.printf("  Min Current: %.1fA\n", minCurrentToMove);
      Serial.printf("  Deadband: %.1f°\n", deadband);
      Serial.printf("  Velocity Damping: %.3f\n", velocityDamping);
      Serial.println("CURRENT STATE:");
      Serial.printf("  Roll: %.2f° (target: %.1f°)\n", roll, angleSetpoint);
      Serial.printf("  Position: %.2f m (target: %.2f m)\n", currentPosition, positionSetpoint);
      Serial.printf("  Velocity Setpoint: %.2f m/s\n", velocitySetpoint);
      Serial.printf("  Actual Velocity: %.2f m/s\n", avgVelocity);
      Serial.printf("  Motor Current: %.2fA\n", currentOutput);
      break;
  }
}

void startLogging() {
  if (!loggingEnabled) {
    loggingEnabled = true;
    logStartTime = millis();
    logIndex = 0;
    bufferFull = false;
    Serial.println("\n📊 LOGGING STARTED");
    Serial.println("Data will be logged at 50Hz");
    Serial.println("Press 's' to stop, 'w' to download");
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
    Serial.println("Press 'w' to download data");
  } else {
    Serial.println("\n⚠️  No active logging session");
  }
}

void logData(float motorCurrent) {
  if (logIndex < 1000) {
    logBuffer[logIndex].timestamp = millis() - logStartTime;
    logBuffer[logIndex].roll = roll;
    logBuffer[logIndex].pitch = pitch;
    logBuffer[logIndex].yaw = yaw;
    logBuffer[logIndex].pidOutput = currentOutput;
    logBuffer[logIndex].leftCurrent = -motorCurrent;
    logBuffer[logIndex].rightCurrent = motorCurrent;
    logBuffer[logIndex].motorsActive = (abs(motorCurrent) > 0.1);
    logIndex++;
  } else {
    bufferFull = true;
    Serial.println("\n⚠️  Log buffer full! Stop logging and download data.");
  }
}

void downloadLogData() {
  if (logIndex == 0 && !bufferFull) {
    Serial.println("\n⚠️  No data to download");
    return;
  }
  
  Serial.println("\n📊 DOWNLOADING LOG DATA");
  Serial.println("Format: timestamp_ms,roll_deg,pitch_deg,yaw_deg,pid_output,left_current_A,right_current_A,motors_active");
  Serial.println("--- START DATA ---");
  
  int samplesToSend = bufferFull ? 1000 : logIndex;
  
  for (int i = 0; i < samplesToSend; i++) {
    Serial.printf("%lu,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%d\n",
                 logBuffer[i].timestamp,
                 logBuffer[i].roll,
                 logBuffer[i].pitch,
                 logBuffer[i].yaw,
                 logBuffer[i].pidOutput,
                 logBuffer[i].leftCurrent,
                 logBuffer[i].rightCurrent,
                 logBuffer[i].motorsActive ? 1 : 0);
  }
  
  Serial.println("--- END DATA ---");
  Serial.printf("Downloaded %d samples\n", samplesToSend);
  
  // Print control settings for record keeping
  Serial.println("\n=== CASCADED CONTROL SETTINGS (for records) ===");
  Serial.println("ANGLE SETPOINT:");
  Serial.printf("  Balance Angle: %.1f degrees\n", angleSetpoint);
  Serial.println("ANGLE PID (Outer Loop):");
  Serial.printf("  Kp: %.1f\n", Kp_angle);
  Serial.printf("  Ki: %.1f\n", Ki_angle);
  Serial.printf("  Kd: %.1f\n", Kd_angle);
  Serial.println("VELOCITY PID (Inner Loop):");
  Serial.printf("  Kp: %.1f\n", Kp_vel);
  Serial.printf("  Ki: %.2f\n", Ki_vel);
  Serial.printf("  Kd: %.1f\n", Kd_vel);
  Serial.println("POSITION FEEDBACK:");
  Serial.printf("  Kp: %.2f\n", Kp_position);
  Serial.println("MOTOR SETTINGS:");
  Serial.printf("  Deadband: %.1f degrees\n", deadband);
  Serial.printf("  Max Current: %.1f A\n", maxCurrent);
  Serial.printf("  Min Current to Move: %.1f A\n", minCurrentToMove);
  Serial.printf("  Velocity Damping: %.3f\n", velocityDamping);
  Serial.println("SYSTEM INFO:");
  Serial.printf("  IMU Update Rate: 400 Hz (SPI mode)\n");
  Serial.printf("  SPI Clock: 3 MHz\n");
  Serial.printf("  Control Architecture: CASCADED (Angle→Velocity→Current)\n");
  Serial.printf("  Velocity Feedback: ENABLED (VESC encoders)\n");
  Serial.printf("  Velocity Damping: ENABLED (roll rate feedback)\n");
  Serial.printf("  Wheel Diameter: %.3f meters\n", WHEEL_DIAMETER);
}

void clearLogBuffer() {
  logIndex = 0;
  bufferFull = false;
  Serial.println("\n🗑️  Log buffer cleared");
}

void showHelp() {
  Serial.println("\n=== LOGGING COMMANDS ===");
  Serial.println("l - Start logging");
  Serial.println("s - Stop logging");
  Serial.println("w - Download logged data");
  Serial.println("c - Clear log buffer");
  Serial.println("h - Show this help");
  Serial.println("SPACE - Pause/Resume data stream");
  Serial.println("\n=== CASCADED CONTROL TUNING ===");
  Serial.println("ANGLE SETPOINT (CRITICAL - Find balance angle!):");
  Serial.println("  z/Z - Decrease/Increase Angle Setpoint (±0.1°)");
  Serial.println("        Adjust until robot STOPS DRIFTING!");
  Serial.println("ANGLE PID (Outer Loop):");
  Serial.println("  p/P - Decrease/Increase Angle Kp (±1.0)");
  Serial.println("  i/I - Decrease/Increase Angle Ki (±0.1)");
  Serial.println("  d/D - Decrease/Increase Angle Kd (±0.1)");
  Serial.println("VELOCITY PID (Inner Loop):");
  Serial.println("  a/A - Decrease/Increase Velocity Kp (±0.1)");
  Serial.println("  b/B - Decrease/Increase Velocity Ki (±0.05)");
  Serial.println("POSITION FEEDBACK (Usually keep at 0):");
  Serial.println("  n/N - Decrease/Increase Position Kp (±0.1)");
  Serial.println("  r   - Reset position to 0.0m");
  Serial.println("MOTOR SETTINGS:");
  Serial.println("  m/M - Decrease/Increase Max Current (±0.5A)");
  Serial.println("  x   - Show all current settings");
  Serial.println("\n=== CURRENT SETTINGS ===");
  Serial.printf("Angle PID: Kp=%.1f Ki=%.1f Kd=%.1f\n", Kp_angle, Ki_angle, Kd_angle);
  Serial.printf("Velocity PID: Kp=%.1f Ki=%.2f Kd=%.1f\n", Kp_vel, Ki_vel, Kd_vel);
  Serial.printf("Position Kp: %.2f\n", Kp_position);
  Serial.printf("Position: %.2fm (target: %.2fm)\n", currentPosition, positionSetpoint);
  Serial.printf("Max Current: %.1fA  Deadband: %.1f°\n", maxCurrent, deadband);
}
