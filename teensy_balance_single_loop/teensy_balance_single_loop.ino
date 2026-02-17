/*
 * BALANCE ROBOT - CLEAN MINIMAL CONTROL
 * 
 * ARCHITECTURE: Single-Loop PID (Angle → Motor Current)
 * - Simple, testable, industry-standard approach
 * - No cascaded control complexity
 * - No deadband logic (PID handles it)
 * - No position feedback (can add later if needed)
 * 
 * DESIGN PHILOSOPHY:
 * - Minimal, cohesive, testable units
 * - Start simple, add complexity only if needed
 * - Verify basic causality before tuning
 * 
 * CONTROL FLOW:
 * 1. Read IMU roll angle
 * 2. Calculate angle error (roll - setpoint)
 * 3. PID computes motor current
 * 4. Send current to motors
 * 
 * MODES:
 * - 'd' = Diagnostic mode (direct angle → current mapping, no PID)
 * - Normal mode = PID control
 */

#include <Wire.h>
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

// SINGLE-LOOP PID CONTROL: Angle → Motor Current
// This is the industry-standard approach for self-balancing robots
double baseSetpoint = 0.0;   // User-defined balance angle (degrees)
double driveOffset = 0.0;    // Dynamic tilt for driving (degrees)
double angleSetpoint = 0.0;  // Active setpoint = baseSetpoint + driveOffset
double angleInput;           // Current roll angle (degrees)
double motorCurrent;         // PID output: motor current (Amps)

// PID Gains - Start conservative, tune up
double Kp = 5.0;   // Proportional gain
double Ki = 0.1;   // Integral gain (small to prevent windup)
double Kd = 0.3;   // Derivative gain (damping)

PID balancePID(&angleInput, &motorCurrent, &angleSetpoint, Kp, Ki, Kd, DIRECT);

// YAW CONTROL: Prevents unwanted rotation
// Separate PID loop to maintain heading (yaw angle)
double yawSetpoint = 0.0;    // Target yaw angle (degrees) - initialized when balancing starts
double yawInput;             // Current yaw angle (degrees)
double yawOutput;            // Yaw PID output: differential current (Amps)

// Yaw PID Gains - DISABLED BY DEFAULT (was interfering with balance)
// Enable only after balance is stable and you want to prevent rotation
double Kp_yaw = 0.0;   // Proportional gain for yaw (0 = disabled)
double Ki_yaw = 0.0;   // Integral gain for yaw (0 = disabled)
double Kd_yaw = 0.0;   // Derivative gain for yaw (0 = disabled)

PID yawPID(&yawInput, &yawOutput, &yawSetpoint, Kp_yaw, Ki_yaw, Kd_yaw, DIRECT);

bool yawControlEnabled = false;  // DISABLED BY DEFAULT - was causing balance instability

// Motor control parameters
float maxCurrent = 6.0;  // Maximum motor current (Amps)
float minCurrent = 0.3;  // Minimum current to overcome friction (Amps)

// Drive control limits (tilt offset in degrees)
const float DRIVE_OFFSET_STEP = 0.1;
const float DRIVE_OFFSET_MAX = 5.0;

// Fine adjust mode (for smaller tuning steps)
bool fineAdjust = false;
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

// Persistent settings (EEPROM)
struct SavedSettings {
  uint32_t magic;
  float kp;
  float ki;
  float kd;
  float baseSetpoint;
  float maxCurrent;
  float minCurrent;
  float kp_yaw;
  float ki_yaw;
  float kd_yaw;
  bool yawControlEnabled;
};
const uint32_t SETTINGS_MAGIC = 0xB105E5A1;

bool loadSettings();
void saveSettings();

// Control mode
enum ControlMode {
  MODE_DIAGNOSTIC,  // Direct angle → current mapping (no PID) - for testing
  MODE_PID          // Normal PID control
};
ControlMode controlMode = MODE_PID;

// Motor direction configuration
// DETERMINE ONCE: When robot tilts forward (positive roll), should wheels spin forward (positive current)?
// If robot moves wrong direction, change MOTOR_DIRECTION_SIGN
const float MOTOR_DIRECTION_SIGN = 1.0;  // -1.0 if motors need to be inverted, 1.0 if correct

// I2C Configuration
const uint32_t I2C_CLOCK_SPEED = 400000;  // 400kHz Fast Mode
const uint32_t IMU_UPDATE_RATE_HZ = 400;  // 400Hz update rate
const uint32_t IMU_REPORT_INTERVAL_US = 2500;  // 2500 microseconds = 2.5ms = 400Hz

// PID Update Rates
const uint32_t PID_SAMPLE_TIME_MS = 2;  // 2ms = 500Hz

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
  
  Serial.println("╔════════════════════════════════════════════════════╗");
  Serial.println("║     BALANCE ROBOT - CLEAN MINIMAL CONTROL          ║");
  Serial.println("╚════════════════════════════════════════════════════╝");
  Serial.println();
  
  Serial.println("🚀 SYSTEM CONFIGURATION:");
  Serial.printf("   • I2C Clock Speed: %d kHz (Fast Mode)\n", I2C_CLOCK_SPEED / 1000);
  Serial.printf("   • IMU Update Rate: %d Hz\n", IMU_UPDATE_RATE_HZ);
  Serial.printf("   • PID Sample Time: %d ms (%d Hz)\n", PID_SAMPLE_TIME_MS, 1000 / PID_SAMPLE_TIME_MS);
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
  angleSetpoint = baseSetpoint + driveOffset;
  balancePID.SetTunings(Kp, Ki, Kd);
  
  // Validate yaw PID gains and reset if corrupted (nan/inf values from EEPROM)
  if (isnan(Kp_yaw) || isinf(Kp_yaw) || Kp_yaw < 0 || Kp_yaw > 10.0) {
    Kp_yaw = 0.5;
    Serial.println("⚠️  Yaw Kp was corrupted, reset to 0.5");
  }
  if (isnan(Ki_yaw) || isinf(Ki_yaw) || Ki_yaw < 0 || Ki_yaw > 5.0) {
    Ki_yaw = 0.0;
    Serial.println("⚠️  Yaw Ki was corrupted, reset to 0.0");
  }
  if (isnan(Kd_yaw) || isinf(Kd_yaw) || Kd_yaw < 0 || Kd_yaw > 5.0) {
    Kd_yaw = 0.1;
    Serial.println("⚠️  Yaw Kd was corrupted, reset to 0.1");
  }
  
  yawPID.SetTunings(Kp_yaw, Ki_yaw, Kd_yaw);
  Serial.println();
  
  // Initialize single-loop PID controller
  Serial.println("🎛️  Initializing PID controllers...");
  balancePID.SetMode(AUTOMATIC);
  balancePID.SetOutputLimits(-maxCurrent, maxCurrent);  // Motor current in Amps
  balancePID.SetSampleTime(PID_SAMPLE_TIME_MS);  // 500Hz update rate
  
  // Initialize yaw PID controller
  yawPID.SetMode(AUTOMATIC);
  yawPID.SetOutputLimits(-maxCurrent, maxCurrent);  // Differential current limit
  yawPID.SetSampleTime(PID_SAMPLE_TIME_MS);  // 500Hz update rate
  
  Serial.printf("   ✅ Balance PID: %d Hz update rate\n", 1000 / PID_SAMPLE_TIME_MS);
  Serial.printf("   ✅ Roll Gains: Kp=%.2f, Ki=%.2f, Kd=%.2f\n", Kp, Ki, Kd);
  Serial.printf("   ✅ Max Current: %.1fA\n", maxCurrent);
  Serial.println();
  
  Serial.println("════════════════════════════════════════════════════");
  Serial.println("✅ SYSTEM READY - Clean Minimal Control");
  Serial.println("════════════════════════════════════════════════════");
  Serial.println();
  Serial.println("📊 CONTROL ARCHITECTURE:");
  Serial.println("   • Single-Loop PID: Angle → Motor Current");
  Serial.println("   • IMU: 400Hz @ 400kHz I2C");
  Serial.println("   • PID: 500Hz update rate");
  Serial.println();
  Serial.println("=== CONTROL MODES ===");
  Serial.println("d - Toggle Diagnostic Mode (direct angle→current, no PID)");
  Serial.println();
  Serial.println("=== TUNING COMMANDS ===");
  Serial.println("p/P - Decrease/Increase Kp");
  Serial.println("i/I - Decrease/Increase Ki");
  Serial.println("j - Decrease Kd");
  Serial.println("D - Increase Kd (NOTE: 'd' toggles diagnostic mode)");
  Serial.println("z/Z - Decrease/Increase Angle Setpoint");
  Serial.println("m/M - Decrease/Increase Max Current");
  Serial.println("x - Show current tuning values");
  Serial.println("V - Save tuning values to EEPROM");
  Serial.println("v - Load tuning values from EEPROM");
  Serial.println("t - Toggle fine adjust (smaller step sizes)");
  Serial.println();
  Serial.println("=== DRIVE COMMANDS ===");
  Serial.println("f - Drive forward (increase tilt)");
  Serial.println("b - Drive backward (decrease tilt)");
  Serial.println("0 - Stop drive (clear tilt offset)");
  Serial.println();
  Serial.println("=== LOGGING COMMANDS ===");
  Serial.println("l - Start logging");
  Serial.println("s - Stop logging");
  Serial.println("w - Download logged data");
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
          printTuningValues();  // Show current tuning values
          lastIMUWarning = millis();
          
          // Attempt I2C bus recovery (release bus)
          static unsigned long lastRecoveryAttempt = 0;
          if (millis() - lastRecoveryAttempt > 5000) {  // Try recovery every 5 seconds
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
      
      // Also show VESC communication stats if available
      unsigned long totalVesc = vescSuccessCount + vescFailCount;
      if (totalVesc > 0) {
        float vescRate = 100.0f * vescSuccessCount / totalVesc;
        Serial.printf("📊 VESC Stats: Success=%lu (%.1f%%), Fail=%lu\n", 
                      vescSuccessCount, vescRate, vescFailCount);
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
      
      // Note: PID library handles derivative internally from angleInput changes
    }
  }
  
  // CLEAN SINGLE-LOOP BALANCE CONTROL
  // Declare motor currents at function scope so they're accessible for logging
  float leftMotorCurrent = 0.0;
  float rightMotorCurrent = 0.0;
  float outputCurrent = 0.0;
  float yawOutput = 0.0;  // Initialize yaw output
  
  if (imuWorking) {
    // Safety check - disable motors if robot is too far tilted
    bool isBalanceable = (abs(roll) < 25.0);
    
    if (!isBalanceable) {
      // Safety: Robot too far tilted, disable motors
      vescLeft.setCurrent(0.0);
      vescRight.setCurrent(0.0);
      motorCurrent = 0.0;
      leftMotorCurrent = 0.0;
      rightMotorCurrent = 0.0;
      
      static unsigned long lastSafetyDebug = 0;
      if (millis() - lastSafetyDebug > 1000) {
        Serial.printf("⚠️  SAFETY: Roll=%.2f° exceeds limit (25°) - Motors DISABLED\n", roll);
        lastSafetyDebug = millis();
      }
    } else {
      // NORMAL CONTROL: Single-loop PID
      
      // Update active setpoint (base + drive offset)
      angleSetpoint = baseSetpoint + driveOffset;

      // Set PID input to current roll angle
      // Note: Roll is already corrected for IMU mounting orientation
      angleInput = roll;
      
      outputCurrent = 0.0;
      
      if (controlMode == MODE_DIAGNOSTIC) {
        // DIAGNOSTIC MODE: Direct angle → current mapping (no PID)
        // This verifies basic causality: tilt forward → wheels forward
        float angleError = roll - angleSetpoint;
        
        // Add small deadzone so robot stops when upright (within 0.2°)
        if (abs(angleError) < 0.2) {
          outputCurrent = 0.0;  // Stop when upright
        } else {
          outputCurrent = angleError * 2.0;  // Simple gain: 2A per degree
          outputCurrent = constrain(outputCurrent, -maxCurrent, maxCurrent);
        }
      } else {
        // PID MODE: Normal control
        balancePID.Compute();  // Computes motorCurrent
        outputCurrent = motorCurrent;
        
        // Apply minimum current threshold (dead zone for friction)
        if (abs(outputCurrent) < minCurrent) {
          outputCurrent = 0.0;
        }
      }
      
      // Apply motor direction sign (determined once during hardware setup)
      outputCurrent *= MOTOR_DIRECTION_SIGN;
      
      // YAW CONTROL: Compute yaw correction to prevent unwanted rotation
      
      if (yawControlEnabled) {
        // Initialize yaw setpoint to current yaw on first IMU data (immediate initialization)
        static bool yawSetpointInitialized = false;
        if (!yawSetpointInitialized) {
          yawSetpoint = yaw;
          yawSetpointInitialized = true;
          Serial.printf("🎯 Yaw setpoint initialized to %.2f°\n", yawSetpoint);
        }
        
        // Update yaw PID
        yawInput = yaw;
        yawPID.Compute();  // Computes yawOutput (differential current)
        
        // Check for invalid yaw output (nan or inf)
        if (isnan(yawOutput) || isinf(yawOutput)) {
          yawOutput = 0.0;
        }
        
        // Limit yaw output to prevent it from overwhelming balance control
        // Max yaw correction should be small compared to balance current
        float maxYawOutput = maxCurrent * 0.3;  // Limit to 30% of max current
        yawOutput = constrain(yawOutput, -maxYawOutput, maxYawOutput);
        
        // Combine roll PID output with yaw PID output
        // Roll output: both motors same direction (forward/backward)
        // Yaw output: opposite directions (rotation)
        // leftMotor = rollOutput - yawOutput  (but left is already inverted, so:)
        // rightMotor = rollOutput + yawOutput
        leftMotorCurrent = -outputCurrent - yawOutput;   // Left motor (inverted for forward motion)
        rightMotorCurrent = outputCurrent + yawOutput;   // Right motor
      } else {
        // Yaw control disabled - use original differential drive
        leftMotorCurrent = -outputCurrent;
        rightMotorCurrent = outputCurrent;
      }
      
      // Send to motors
      vescLeft.setCurrent(leftMotorCurrent);
      vescRight.setCurrent(rightMotorCurrent);
      
      // Debug output (every 500ms) to verify motor commands
      static unsigned long lastDebug = 0;
      if (millis() - lastDebug > 500) {
        Serial.printf("🔧 DEBUG: Roll=%.2f°, Output=%.2fA, Left=%.2fA, Right=%.2fA, YawOut=%.2fA\n", 
                     roll, outputCurrent, leftMotorCurrent, rightMotorCurrent, yawOutput);
        lastDebug = millis();
      }
      
      // Log data if enabled
      if (loggingEnabled && (millis() - lastLogTime >= LOG_INTERVAL)) {
        logData(outputCurrent, leftMotorCurrent, rightMotorCurrent, yawOutput);
        lastLogTime = millis();
      }
    }
  } else {
    // IMU not working - disable motors
    vescLeft.setCurrent(0.0);
    vescRight.setCurrent(0.0);
    motorCurrent = 0.0;
    leftMotorCurrent = 0.0;
    rightMotorCurrent = 0.0;
    yawOutput = 0.0;
  }
  
  // Print status
  if (streamData && (millis() - lastPrint >= 50)) {
    lastPrint = millis();
    
    if (imuWorking) {
      // Calculate angle error for display
      float angleError = roll - angleSetpoint;
      float yawError = yaw - yawSetpoint;
      const char* modeStr = (controlMode == MODE_DIAGNOSTIC) ? "DIAG" : "PID";
      // Log: Roll, Pitch, Yaw, RollError, YawError, RollPID_Out, YawPID_Out, LeftMotor, RightMotor, Setpoint, DriveOffset, Mode, YawCtrl, Logging
      Serial.printf("R:%.2f,P:%.2f,Y:%.2f,Err:%.2f,YawErr:%.2f,RollOut:%.2f,YawOut:%.2f,Left:%.2f,Right:%.2f,Setpt:%.2f,Drive:%.2f,Mode:%s,Yaw:%s,Log:%s\n", 
                   roll, pitch, yaw, angleError, yawError, motorCurrent, yawOutput, leftMotorCurrent, rightMotorCurrent, angleSetpoint, driveOffset,
                   modeStr, yawControlEnabled ? "ON" : "OFF", loggingEnabled ? "ON" : "OFF");
    } else {
      // IMU not working - show diagnostic
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
      downloadLogData();
      break;
      
    case 'c':
    case 'C':
      clearLogBuffer();
      break;
      
    case ' ':
      streamData = !streamData;
      if (streamData) {
        Serial.println("\n✅ Data streaming RESUMED");
      } else {
        Serial.println("\n⏸️  Data streaming PAUSED");
      }
      break;
    
    // DIAGNOSTIC MODE: Toggle direct angle→current mapping (no PID)
    case 'd':
      controlMode = (controlMode == MODE_DIAGNOSTIC) ? MODE_PID : MODE_DIAGNOSTIC;
      if (controlMode == MODE_DIAGNOSTIC) {
        Serial.println("🔧 DIAGNOSTIC MODE: Direct angle→current mapping (no PID)");
        Serial.println("   Tilt forward → positive current → wheels forward");
        Serial.println("   Use this to verify basic causality before tuning PID");
      } else {
        Serial.println("✅ PID MODE: Normal control enabled");
      }
      break;
    
    // ROLL PID TUNING: Proportional gain
    case 'p':
      Kp -= (fineAdjust ? KP_STEP_FINE : KP_STEP_COARSE); if (Kp < 0) Kp = 0;
      balancePID.SetTunings(Kp, Ki, Kd);
      Serial.printf("Roll Kp = %.2f (decreased)\n", Kp);
      break;
      
    case 'P':
      Kp += (fineAdjust ? KP_STEP_FINE : KP_STEP_COARSE);
      balancePID.SetTunings(Kp, Ki, Kd);
      Serial.printf("Roll Kp = %.2f (increased)\n", Kp);
      break;
    
    // ROLL PID TUNING: Integral gain
    case 'i':
      Ki -= (fineAdjust ? KI_STEP_FINE : KI_STEP_COARSE); if (Ki < 0) Ki = 0;
      balancePID.SetTunings(Kp, Ki, Kd);
      Serial.printf("Roll Ki = %.2f (decreased)\n", Ki);
      break;
      
    case 'I':
      Ki += (fineAdjust ? KI_STEP_FINE : KI_STEP_COARSE);
      balancePID.SetTunings(Kp, Ki, Kd);
      Serial.printf("Roll Ki = %.2f (increased)\n", Ki);
      break;
    
    // ROLL PID TUNING: Derivative gain (NOTE: 'd' toggles diagnostic mode, 'j' decreases Kd, 'D' increases Kd)
    case 'j':
    case 'J':
      // Use 'j' for decrease Kd (since 'd' is diagnostic mode, 'k' might conflict)
      Kd -= (fineAdjust ? KD_STEP_FINE : KD_STEP_COARSE); if (Kd < 0) Kd = 0;
      balancePID.SetTunings(Kp, Ki, Kd);
      Serial.printf("Roll Kd = %.2f (decreased)\n", Kd);
      break;
      
    case 'D':
      Kd += (fineAdjust ? KD_STEP_FINE : KD_STEP_COARSE);
      balancePID.SetTunings(Kp, Ki, Kd);
      Serial.printf("Roll Kd = %.2f (increased)\n", Kd);
      break;
    
    // ANGLE SETPOINT tuning
    case 'z':
      baseSetpoint -= (fineAdjust ? SETPOINT_STEP_FINE : SETPOINT_STEP_COARSE);
      Serial.printf("Angle Setpoint = %.1f° (decreased)\n", baseSetpoint);
      Serial.printf("Current roll: %.2f°, Error: %.2f°\n", roll, roll - (baseSetpoint + driveOffset));
      break;
      
    case 'Z':
      baseSetpoint += (fineAdjust ? SETPOINT_STEP_FINE : SETPOINT_STEP_COARSE);
      Serial.printf("Angle Setpoint = %.1f° (increased)\n", baseSetpoint);
      Serial.printf("Current roll: %.2f°, Error: %.2f°\n", roll, roll - (baseSetpoint + driveOffset));
      break;
    
    // MAX CURRENT tuning
    case 'm':
      maxCurrent -= (fineAdjust ? MAXCURRENT_STEP_FINE : MAXCURRENT_STEP_COARSE); if (maxCurrent < 1.0) maxCurrent = 1.0;
      balancePID.SetOutputLimits(-maxCurrent, maxCurrent);
      Serial.printf("Max Current = %.1fA (decreased)\n", maxCurrent);
      break;
      
    case 'M':
      maxCurrent += (fineAdjust ? MAXCURRENT_STEP_FINE : MAXCURRENT_STEP_COARSE); if (maxCurrent > 10.0) maxCurrent = 10.0;
      balancePID.SetOutputLimits(-maxCurrent, maxCurrent);
      Serial.printf("Max Current = %.1fA (increased)\n", maxCurrent);
      break;

    // DRIVE CONTROL: Adjust tilt offset for movement
    case 'f':
    case 'F':
      driveOffset += DRIVE_OFFSET_STEP;
      if (driveOffset > DRIVE_OFFSET_MAX) driveOffset = DRIVE_OFFSET_MAX;
      Serial.printf("Drive Offset = %.2f° (forward)\n", driveOffset);
      break;
      
    case 'b':
    case 'B':
      driveOffset -= DRIVE_OFFSET_STEP;
      if (driveOffset < -DRIVE_OFFSET_MAX) driveOffset = -DRIVE_OFFSET_MAX;
      Serial.printf("Drive Offset = %.2f° (backward)\n", driveOffset);
      break;
      
    case '0':
      driveOffset = 0.0;
      Serial.println("Drive Offset = 0.00° (stop)");
      break;
    
    // I2C speed reduction (if seeing failures)
    case 'q':
      {
        uint32_t newSpeed = 200000;  // 200kHz
        Wire.setClock(newSpeed);
        Serial.printf("I2C speed reduced to %d kHz\n", newSpeed / 1000);
      }
      break;
    
    case 'Q':
      {
        uint32_t newSpeed = 100000;  // 100kHz
        Wire.setClock(newSpeed);
        Serial.printf("I2C speed reduced to %d kHz\n", newSpeed / 1000);
      }
      break;

    // SAVE/LOAD SETTINGS
    case 'V':
      saveSettings();
      Serial.println("✅ Settings saved to EEPROM");
      break;
      
    case 'v':
      if (loadSettings()) {
        balancePID.SetTunings(Kp, Ki, Kd);
        balancePID.SetOutputLimits(-maxCurrent, maxCurrent);
        yawPID.SetTunings(Kp_yaw, Ki_yaw, Kd_yaw);
        yawPID.SetOutputLimits(-maxCurrent, maxCurrent);
        Serial.println("✅ Settings loaded from EEPROM");
      } else {
        Serial.println("⚠️  No saved settings found");
      }
      break;
    
    case 't':
    case 'T':
      fineAdjust = !fineAdjust;
      Serial.printf("Fine Adjust: %s\n", fineAdjust ? "ON" : "OFF");
      break;
    
    // YAW PID TUNING: Proportional gain
    case 'y':
      // Reset if corrupted
      if (isnan(Kp_yaw) || isinf(Kp_yaw)) Kp_yaw = 0.5;
      Kp_yaw -= (fineAdjust ? KP_STEP_FINE : KP_STEP_COARSE); if (Kp_yaw < 0) Kp_yaw = 0;
      yawPID.SetTunings(Kp_yaw, Ki_yaw, Kd_yaw);
      Serial.printf("Yaw Kp = %.2f (decreased)\n", Kp_yaw);
      break;
      
    case 'Y':
      // Reset if corrupted
      if (isnan(Kp_yaw) || isinf(Kp_yaw)) Kp_yaw = 0.5;
      Kp_yaw += (fineAdjust ? KP_STEP_FINE : KP_STEP_COARSE);
      yawPID.SetTunings(Kp_yaw, Ki_yaw, Kd_yaw);
      Serial.printf("Yaw Kp = %.2f (increased)\n", Kp_yaw);
      break;
    
    // YAW PID TUNING: Integral gain
    case 'u':
      // Reset if corrupted
      if (isnan(Ki_yaw) || isinf(Ki_yaw)) Ki_yaw = 0.0;
      Ki_yaw -= (fineAdjust ? KI_STEP_FINE : KI_STEP_COARSE); if (Ki_yaw < 0) Ki_yaw = 0;
      // Also check other gains
      if (isnan(Kp_yaw) || isinf(Kp_yaw)) Kp_yaw = 0.5;
      if (isnan(Kd_yaw) || isinf(Kd_yaw)) Kd_yaw = 0.1;
      yawPID.SetTunings(Kp_yaw, Ki_yaw, Kd_yaw);
      Serial.printf("Yaw Ki = %.2f (decreased)\n", Ki_yaw);
      break;
      
    case 'U':
      // Reset if corrupted
      if (isnan(Ki_yaw) || isinf(Ki_yaw)) Ki_yaw = 0.0;
      Ki_yaw += (fineAdjust ? KI_STEP_FINE : KI_STEP_COARSE);
      // Also check other gains
      if (isnan(Kp_yaw) || isinf(Kp_yaw)) Kp_yaw = 0.5;
      if (isnan(Kd_yaw) || isinf(Kd_yaw)) Kd_yaw = 0.1;
      yawPID.SetTunings(Kp_yaw, Ki_yaw, Kd_yaw);
      Serial.printf("Yaw Ki = %.2f (increased)\n", Ki_yaw);
      break;
    
    // YAW PID TUNING: Derivative gain
    case 'h':
      // Reset if corrupted
      if (isnan(Kd_yaw) || isinf(Kd_yaw)) Kd_yaw = 0.1;
      Kd_yaw -= (fineAdjust ? KD_STEP_FINE : KD_STEP_COARSE); if (Kd_yaw < 0) Kd_yaw = 0;
      // Also check other gains
      if (isnan(Kp_yaw) || isinf(Kp_yaw)) Kp_yaw = 0.5;
      if (isnan(Ki_yaw) || isinf(Ki_yaw)) Ki_yaw = 0.0;
      yawPID.SetTunings(Kp_yaw, Ki_yaw, Kd_yaw);
      Serial.printf("Yaw Kd = %.2f (decreased)\n", Kd_yaw);
      break;
      
    case 'H':
      // Reset if corrupted
      if (isnan(Kd_yaw) || isinf(Kd_yaw)) Kd_yaw = 0.1;
      Kd_yaw += (fineAdjust ? KD_STEP_FINE : KD_STEP_COARSE);
      // Also check other gains
      if (isnan(Kp_yaw) || isinf(Kp_yaw)) Kp_yaw = 0.5;
      if (isnan(Ki_yaw) || isinf(Ki_yaw)) Ki_yaw = 0.0;
      yawPID.SetTunings(Kp_yaw, Ki_yaw, Kd_yaw);
      Serial.printf("Yaw Kd = %.2f (increased)\n", Kd_yaw);
      break;
    
    // YAW CONTROL: Toggle enable/disable (using 'n' for "no rotation")
    case 'n':
    case 'N':
      yawControlEnabled = !yawControlEnabled;
      Serial.printf("Yaw Control: %s\n", yawControlEnabled ? "ENABLED" : "DISABLED");
      if (yawControlEnabled) {
        Serial.println("   Yaw PID will prevent unwanted rotation");
      } else {
        Serial.println("   Yaw PID disabled - robot may rotate freely");
      }
      break;
    
    // YAW SETPOINT: Reset to current yaw
    case 'r':
    case 'R':
      yawSetpoint = yaw;
      Serial.printf("Yaw Setpoint reset to current yaw: %.2f°\n", yawSetpoint);
      break;
    
    // Show current settings
    case 'x':
    case 'X':
      printTuningValues();
      Serial.println("CURRENT STATE:");
      Serial.printf("  Roll: %.2f° (target: %.1f°, error: %.2f°)\n", roll, angleSetpoint, roll - angleSetpoint);
      Serial.printf("  Yaw: %.2f° (target: %.1f°, error: %.2f°)\n", yaw, yawSetpoint, yaw - yawSetpoint);
      Serial.printf("  Base Setpoint: %.2f°  Drive Offset: %.2f°\n", baseSetpoint, driveOffset);
      Serial.printf("  Motor Current: %.2fA  Yaw Output: %.2fA\n", motorCurrent, yawOutput);
      Serial.printf("  Control Mode: %s  Yaw Control: %s\n", 
                    (controlMode == MODE_DIAGNOSTIC) ? "DIAGNOSTIC" : "PID",
                    yawControlEnabled ? "ENABLED" : "DISABLED");
      Serial.println("════════════════════════════════════════════════════\n");
      break;
  }
}

// Function to print all current tuning values
void printTuningValues() {
  Serial.println("\n╔════════════════════════════════════════════════════╗");
  Serial.println("║         CURRENT TUNING VALUES                      ║");
  Serial.println("╚════════════════════════════════════════════════════╝");
  Serial.println("ROLL PID CONTROL (Balance):");
  Serial.printf("  Roll Kp: %.2f  Roll Ki: %.2f  Roll Kd: %.2f\n", Kp, Ki, Kd);
  Serial.printf("  Base Angle Setpoint: %.2f°\n", baseSetpoint);
  Serial.printf("  Drive Offset: %.2f°  Active Setpoint: %.2f°\n", driveOffset, angleSetpoint);
  Serial.println("YAW PID CONTROL (Rotation):");
  Serial.printf("  Kp_yaw: %.2f  Ki_yaw: %.2f  Kd_yaw: %.2f\n", Kp_yaw, Ki_yaw, Kd_yaw);
  Serial.printf("  Yaw Setpoint: %.2f°  Yaw Control: %s\n", yawSetpoint, yawControlEnabled ? "ENABLED" : "DISABLED");
  Serial.println("MOTOR CONTROL:");
  Serial.printf("  Max Current: %.2fA  Min Current: %.2fA\n", maxCurrent, minCurrent);
  Serial.printf("  Motor Direction Sign: %.1f\n", MOTOR_DIRECTION_SIGN);
  Serial.println("SYSTEM CONFIG:");
  Serial.printf("  I2C Clock: %d kHz  IMU Rate: %d Hz  PID Rate: %d Hz\n", 
                I2C_CLOCK_SPEED / 1000, IMU_UPDATE_RATE_HZ, 1000 / PID_SAMPLE_TIME_MS);
  Serial.printf("  Control Mode: %s\n", (controlMode == MODE_DIAGNOSTIC) ? "DIAGNOSTIC" : "PID");
  Serial.printf("  Fine Adjust: %s\n", fineAdjust ? "ON" : "OFF");
  Serial.println("════════════════════════════════════════════════════");
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

void logData(float rollPIDOutput, float leftMotorCurrent, float rightMotorCurrent, float yawPIDOutput) {
  if (logIndex < 1000) {
    logBuffer[logIndex].timestamp = millis() - logStartTime;
    logBuffer[logIndex].roll = roll;
    logBuffer[logIndex].pitch = pitch;
    logBuffer[logIndex].yaw = yaw;
    logBuffer[logIndex].pidOutput = rollPIDOutput;  // Roll PID output (before yaw correction)
    // Log ACTUAL motor currents sent to motors (includes yaw correction)
    logBuffer[logIndex].leftCurrent = leftMotorCurrent;
    logBuffer[logIndex].rightCurrent = rightMotorCurrent;
    logBuffer[logIndex].motorsActive = (abs(leftMotorCurrent) > 0.1 || abs(rightMotorCurrent) > 0.1);
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
  Serial.println("\n=== CONTROL SETTINGS (for records) ===");
  Serial.println("SINGLE-LOOP PID CONTROL:");
  Serial.printf("  Roll Kp: %.2f\n", Kp);
  Serial.printf("  Roll Ki: %.2f\n", Ki);
  Serial.printf("  Roll Kd: %.2f\n", Kd);
  Serial.printf("  Base Angle Setpoint: %.1f degrees\n", baseSetpoint);
  Serial.printf("  Drive Offset: %.2f degrees\n", driveOffset);
  Serial.println("MOTOR SETTINGS:");
  Serial.printf("  Max Current: %.1f A\n", maxCurrent);
  Serial.printf("  Min Current: %.1f A\n", minCurrent);
  Serial.printf("  Motor Direction Sign: %.1f\n", MOTOR_DIRECTION_SIGN);
  Serial.println("SYSTEM INFO:");
  Serial.printf("  IMU Update Rate: %d Hz\n", IMU_UPDATE_RATE_HZ);
  Serial.printf("  I2C Clock: %d kHz\n", I2C_CLOCK_SPEED / 1000);
  Serial.printf("  Control Architecture: SINGLE-LOOP (Angle→Current)\n");
  Serial.printf("  Control Mode: %s\n", (controlMode == MODE_DIAGNOSTIC) ? "DIAGNOSTIC" : "PID");
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
  
  // Load yaw PID settings (new fields, may not exist in old saved settings)
  // Validate values to prevent nan/inf corruption
  Kp_yaw = settings.kp_yaw;
  Ki_yaw = settings.ki_yaw;
  Kd_yaw = settings.kd_yaw;
  yawControlEnabled = settings.yawControlEnabled;
  
  // Validate and reset if corrupted (nan/inf values)
  if (isnan(Kp_yaw) || isinf(Kp_yaw) || Kp_yaw < 0 || Kp_yaw > 10.0) {
    Kp_yaw = 0.5;  // Reset to default
  }
  if (isnan(Ki_yaw) || isinf(Ki_yaw) || Ki_yaw < 0 || Ki_yaw > 5.0) {
    Ki_yaw = 0.0;  // Reset to default
  }
  if (isnan(Kd_yaw) || isinf(Kd_yaw) || Kd_yaw < 0 || Kd_yaw > 5.0) {
    Kd_yaw = 0.1;  // Reset to default
  }
  
  yawPID.SetTunings(Kp_yaw, Ki_yaw, Kd_yaw);
  
  driveOffset = 0.0;  // Always start with no drive offset
  angleSetpoint = baseSetpoint + driveOffset;
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
  settings.kp_yaw = Kp_yaw;
  settings.ki_yaw = Ki_yaw;
  settings.kd_yaw = Kd_yaw;
  settings.yawControlEnabled = yawControlEnabled;
  EEPROM.put(0, settings);
}

void clearLogBuffer() {
  logIndex = 0;
  bufferFull = false;
  Serial.println("\n🗑️  Log buffer cleared");
}

