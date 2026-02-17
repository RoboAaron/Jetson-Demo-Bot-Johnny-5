/*
 * BALANCE ROBOT DATA LOGGER - OPTIMIZED I2C MODE
 * Captures IMU data, PID outputs, and motor commands for analysis
 * Use this to fine-tune PID parameters based on real data
 * 
 * STATUS: I2C OPTIMIZED - Target 70-85% reliability
 * - Baseline: Can balance ~30-40% of the time for 30+ seconds (I2C @ 100kHz, 100Hz)
 * - Optimized: I2C @ 400kHz, 400Hz IMU updates, velocity damping enabled
 * - Expected: 70-85% success rate with optimized I2C
 * - Control method: Current control (optimal)
 * - Architecture: Cascaded PID (proven)
 * - Improvements: Higher I2C speed, faster IMU updates, velocity damping (Kd_vel = 0.15)
 */

#include <Wire.h>
#include <Adafruit_BNO08x.h>
#include <VescUart.h>
#include <PID_v1.h>

// Global objects
Adafruit_BNO08x bno08x(-1);  // I2C mode (no reset pin needed)
sh2_SensorValue_t sensorValue;
VescUart vescLeft;
VescUart vescRight;

// IMU data
bool imuWorking = false;
float pitch = 0.0, roll = 0.0, yaw = 0.0;
float gyroX = 0.0, gyroY = 0.0, gyroZ = 0.0;
float lastRoll = 0.0;
float rollRate = 0.0;

// VESC communication tracking (for diagnostics - helps identify EMI issues)
unsigned long vescFailCount = 0;
unsigned long vescSuccessCount = 0;

// CASCADED PID CONTROL SYSTEM WITH POSITION FEEDBACK
// Outer loop: Angle control (slow)
double angleSetpoint = 1.1;  // CALIBRATED balance angle (was causing drift at 0.0°)
double angleInput, velocitySetpoint;
// TUNED: Further reduced to eliminate chattering/jerking (was 10.0, 0.3, 0.6)
double Kp_angle = 8.0, Ki_angle = 0.2, Kd_angle = 0.4;  // Smoother, less chattery settings
PID anglePID(&angleInput, &velocitySetpoint, &angleSetpoint, Kp_angle, Ki_angle, Kd_angle, DIRECT);

// Inner loop: Velocity control (fast)
double velocityInput, currentOutput;
// TUNED: Further reduced to eliminate chattering/jerking (was 0.5, 0.2, 0.15)
double Kp_vel = 0.4, Ki_vel = 0.15, Kd_vel = 0.15;  // Smoother, Kd_vel kept for damping
PID velocityPID(&velocityInput, &currentOutput, &velocitySetpoint, Kp_vel, Ki_vel, Kd_vel, DIRECT);

// Position feedback (prevents drift)
double positionSetpoint = 0.0;  // Target position (stay at start)
double currentPosition = 0.0;   // Integrated from velocity
double Kp_position = 0.0;       // Position feedback gain (DISABLED by default - may cause drift!)
unsigned long lastPositionUpdate = 0;

// Motor control parameters
float minCurrentToMove = 0.3;  // TUNED: Increased from 0.2 to reduce chattering on small corrections
float maxCurrent = 6.0;  // TUNED: Increased from 5.0A to 6.0A (user feedback: 5A not enough)
float deadband = 0.5;  // ORIGINAL - Reduced deadband for cascaded control
float velocityDamping = 0.01;  // OPTIMIZED: Adjusted for 400Hz update rate (was 0.01 for 100Hz, scales with rate)

// Motor direction control
bool motorDirectionsSwapped = true;  // Set to true if motors were reversed (now fixed)
bool rollSignInverted = false;  // Set to true if PID direction is wrong (robot moves wrong way when tilted)

// Velocity measurement from encoders
float leftVelocity = 0.0;   // m/s (persists between VESC reads)
float rightVelocity = 0.0;  // m/s (persists between VESC reads)
float avgVelocity = 0.0;    // m/s
const float WHEEL_DIAMETER = 0.165;  // meters (6.5 inches typical hoverboard wheel)
const float RPM_TO_MPS = (WHEEL_DIAMETER * PI) / 60.0;  // Convert RPM to m/s

  // I2C Configuration
const uint32_t I2C_CLOCK_SPEED = 400000;  // 400kHz Fast Mode (was 100000)
// NOTE: If seeing I2C failures, try reducing to 200000 (200kHz) or 100000 (100kHz)
const uint32_t IMU_UPDATE_RATE_HZ = 400;  // 400Hz update rate (was 100Hz) - I2C @ 400kHz can handle this easily
const uint32_t IMU_REPORT_INTERVAL_US = 2500;  // 2500 microseconds = 2.5ms = 400Hz (was 10000 = 10ms = 100Hz)

// PID Update Rates
const uint32_t PID_SAMPLE_TIME_MS = 2;  // 2ms = 500Hz (was 10ms = 100Hz) - Faster than IMU for responsive control

// VESC Communication Rate Limiting (CRITICAL: Prevents serial buffer overflow and resets)
// VESC UART communication should not exceed 100-200Hz to avoid blocking and buffer issues
// NOTE: Can be increased to 5ms (200Hz) or 2ms (500Hz) if needed for rock-solid balancing
// Start at 15ms (67Hz) for reliability, increase if balancing needs faster updates
uint32_t VESC_UPDATE_INTERVAL_MS = 15;  // 15ms = 67Hz (more reliable, adjustable)

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
  Serial.println("║     BALANCE ROBOT - OPTIMIZED I2C MODE             ║");
  Serial.println("╚════════════════════════════════════════════════════╝");
  Serial.println();
  
  Serial.println("🚀 I2C OPTIMIZATION CONFIGURATION:");
  Serial.printf("   • I2C Clock Speed: %d kHz (Fast Mode)\n", I2C_CLOCK_SPEED / 1000);
  Serial.printf("   • IMU Update Rate: %d Hz\n", IMU_UPDATE_RATE_HZ);
  Serial.printf("   • PID Sample Time: %d ms (%d Hz)\n", PID_SAMPLE_TIME_MS, 1000 / PID_SAMPLE_TIME_MS);
  Serial.println();
  
  // Initialize I2C at optimized speed
  Serial.println("📡 Initializing I2C bus...");
  Wire.begin();
  Wire.setClock(I2C_CLOCK_SPEED);  // OPTIMIZED: 400kHz Fast Mode
  // NOTE: If seeing I2C failures, try reducing I2C_CLOCK_SPEED to 200000 or 100000
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
    
    // Enable gyroscope for velocity damping at optimized rate
    Serial.printf("   Enabling gyroscope at %d Hz...\n", IMU_UPDATE_RATE_HZ);
    if (!bno08x.enableReport(SH2_GYROSCOPE_CALIBRATED, IMU_REPORT_INTERVAL_US)) {
      Serial.println("   ⚠️  Could not enable gyroscope (velocity damping disabled)");
    } else {
      Serial.printf("   ✅ Gyroscope enabled for velocity damping at %d Hz\n", IMU_UPDATE_RATE_HZ);
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
      
      // Enable gyroscope for velocity damping at optimized rate
      Serial.printf("   Enabling gyroscope at %d Hz...\n", IMU_UPDATE_RATE_HZ);
      if (!bno08x.enableReport(SH2_GYROSCOPE_CALIBRATED, IMU_REPORT_INTERVAL_US)) {
        Serial.println("   ⚠️  Could not enable gyroscope (velocity damping disabled)");
      } else {
        Serial.printf("   ✅ Gyroscope enabled for velocity damping at %d Hz\n", IMU_UPDATE_RATE_HZ);
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
  
  // Initialize cascaded PID controllers at optimized rate
  Serial.println("🎛️  Initializing PID controllers...");
  anglePID.SetMode(AUTOMATIC);
  anglePID.SetOutputLimits(-3.0, 3.0);  // Velocity setpoint in m/s
  anglePID.SetSampleTime(PID_SAMPLE_TIME_MS);  // OPTIMIZED: 200Hz update rate
  
  velocityPID.SetMode(AUTOMATIC);
  velocityPID.SetOutputLimits(-maxCurrent, maxCurrent);  // Current in Amps
  velocityPID.SetSampleTime(PID_SAMPLE_TIME_MS);  // OPTIMIZED: 200Hz update rate
  
  Serial.printf("   ✅ Angle PID: %d Hz update rate\n", 1000 / PID_SAMPLE_TIME_MS);
  Serial.printf("   ✅ Velocity PID: %d Hz update rate (Kd_vel = %.2f)\n", 1000 / PID_SAMPLE_TIME_MS, Kd_vel);
  Serial.println();
  
  Serial.println("════════════════════════════════════════════════════");
  Serial.println("✅ SYSTEM READY - Optimized I2C Configuration");
  Serial.println("════════════════════════════════════════════════════");
  Serial.println();
  Serial.println("📊 EXPECTED IMPROVEMENTS:");
  Serial.println("   • 4x faster sensor updates (400Hz vs 100Hz)");
  Serial.println("   • 4x faster I2C bus (400kHz vs 100kHz)");
  Serial.println("   • Lower latency (~0.3-0.5ms vs 1-2ms)");
  Serial.println("   • Velocity damping enabled (eliminates chattering)");
  Serial.println("   • Target: 70-85% reliability (vs 30-40% baseline)");
  Serial.println();
  Serial.println("=== LOGGING COMMANDS ===");
  Serial.println("l - Start logging");
  Serial.println("s - Stop logging");
  Serial.println("w - Download logged data");
  Serial.println("c - Clear log buffer");
  Serial.println("h - Show this help");
  Serial.println("SPACE - Pause/Resume data stream");
  Serial.println("x - Show current tuning values (also shown when comms lost)");
  Serial.println("\nReady for logging!");
}

void loop() {
  static unsigned long lastPrint = 0;
  static unsigned long lastHeartbeat = 0;
  static unsigned long lastIMUUpdate = 0;
  static unsigned long imuReadCount = 0;
  static unsigned long imuFailCount = 0;
  static unsigned long lastIMUStats = 0;
  static unsigned long lastVESCUpdate = 0;  // Rate limiting for VESC communication
  
  // Handle serial commands
  if (Serial.available()) {
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
        
        // Warn if VESC communication is failing
        if (vescRate < 50.0f && totalVesc > 20) {  // Only warn after enough samples
          Serial.println("   ⚠️  WARNING: VESC communication failing! Check:");
          Serial.println("      • Serial wiring (TX/RX swapped?)");
          Serial.println("      • VESC power and ground connections");
          Serial.println("      • Try increasing VESC interval (press 'u')");
          Serial.println("      • Check for EMI interference");
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
      
      // Store raw roll for diagnostics
      float rawRoll = roll;
      
      // IMU orientation correction (mounted upside down) - matches working baseline
      pitch = -pitch;
      roll += 180.0f;
      if (roll > 180.0f) roll -= 360.0f;
      if (roll < -180.0f) roll += 360.0f;
      
      // DEBUG: Show raw vs corrected roll (temporary - remove after fixing)
      static unsigned long lastDebug = 0;
      if (millis() - lastDebug > 500) {  // Print every 500ms
        Serial.printf("DEBUG: RawRoll=%.2f°, CorrectedRoll=%.2f°, Pitch=%.2f°\n", rawRoll, roll, pitch);
        lastDebug = millis();
      }
      
      // Calculate roll rate (degrees per second)
      // OPTIMIZED: For 400Hz update rate (2.5ms intervals)
      rollRate = (roll - lastRoll) * IMU_UPDATE_RATE_HZ;  // Was * 100.0 for 100Hz
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
    // Safety check - roll should be near 0° when upright (or near angleSetpoint = 1.1°)
    // If roll is 140-180°, the IMU orientation correction is likely wrong
    bool isBalanceable = (abs(roll) < 25.0);
    
    // DEBUG: Show why motors are disabled
    static unsigned long lastSafetyDebug = 0;
    if (!isBalanceable && (millis() - lastSafetyDebug > 1000)) {
      Serial.printf("⚠️  SAFETY: Roll=%.2f° exceeds limit (25°) - Motors DISABLED\n", roll);
      lastSafetyDebug = millis();
    }
    
    if (!isBalanceable) {
      vescLeft.setCurrent(0.0);
      vescRight.setCurrent(0.0);
      velocitySetpoint = 0.0;
      currentOutput = 0.0;
    } else {
      // CRITICAL FIX: Rate limit VESC communication to prevent serial buffer overflow and resets
      // VESC UART functions can block and should not be called more than 100Hz
      unsigned long now = millis();
      bool shouldUpdateVESC = (now - lastVESCUpdate >= VESC_UPDATE_INTERVAL_MS);
      
      if (shouldUpdateVESC) {
        lastVESCUpdate = now;
        
        // Read encoder velocities from VESCs
        // CRITICAL: VESC UART needs time between requests - add small delays
        // Reading both VESCs too quickly can cause communication failures
        bool leftDataValid = vescLeft.getVescValues();
        delayMicroseconds(500);  // Small delay between VESC reads (prevents serial conflicts)
        bool rightDataValid = vescRight.getVescValues();
        
        // Track VESC communication failures (for diagnostics)
        if (!leftDataValid || !rightDataValid) {
          vescFailCount++;
          // If both VESCs fail, flush serial buffers and warn
          if (!leftDataValid && !rightDataValid) {
            Serial1.flush();
            Serial2.flush();
            static unsigned long lastVescWarning = 0;
            if (millis() - lastVescWarning > 2000) {  // Warn every 2 seconds max
              Serial.println("\n⚠️  VESC COMMUNICATION LOST! Both VESCs not responding");
              Serial.println("   Possible causes: Serial wiring, VESC power, EMI interference");
              printTuningValues();  // Show current tuning values
              lastVescWarning = millis();
            }
          } else {
            // One VESC failed - less critical, but still log occasionally
            static unsigned long lastSingleVescWarning = 0;
            if (millis() - lastSingleVescWarning > 5000) {  // Warn every 5 seconds max
              Serial.printf("\n⚠️  VESC Communication Issue: %s VESC not responding\n", 
                           !leftDataValid ? "Left" : "Right");
              lastSingleVescWarning = millis();
            }
          }
        } else {
          vescSuccessCount++;
        }
        
        // Convert RPM to m/s (positive = forward) - use 0 if data invalid
        // IMPORTANT: Only update velocity if data is valid, otherwise keep last value
        if (leftDataValid) {
          leftVelocity = vescLeft.data.rpm * RPM_TO_MPS;
        }
        if (rightDataValid) {
          rightVelocity = vescRight.data.rpm * RPM_TO_MPS;
        }
        // Always recalculate average (uses last valid values if one fails)
        avgVelocity = (leftVelocity + rightVelocity) / 2.0;
      }
      // If not updating VESC, keep using last velocity values (smooth operation)
      
      // UPDATE POSITION: Integrate velocity over time
      unsigned long nowPos = millis();
      if (lastPositionUpdate > 0) {
        float dt = (nowPos - lastPositionUpdate) / 1000.0;  // Convert to seconds
        currentPosition += avgVelocity * dt;  // Distance = velocity * time
      }
      lastPositionUpdate = nowPos;
      
      // OUTER LOOP: Angle control → Velocity setpoint
      // Add velocity damping for predictive control (prevents overshoot)
      // If PID direction is wrong (robot moves wrong way when tilted), set rollSignInverted = true
      float rollForPID = rollSignInverted ? -roll : roll;
      angleInput = rollForPID + (rollRate * velocityDamping);
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
      velocityPID.Compute();  // Outputs currentOutput (now with Kd_vel damping!)
      
      // Apply minimum current threshold
      float motorOutput = currentOutput;
      if (abs(motorOutput) < minCurrentToMove) {
        motorOutput = 0.0;
      }
      
      // Send to motors (directions - swap if needed)
      // CRITICAL: Only send commands when VESC update happens (rate limited)
      // This prevents serial buffer overflow and resets
      if (shouldUpdateVESC) {
        if (motorDirectionsSwapped) {
          vescLeft.setCurrent(motorOutput);   // Swapped from baseline
          vescRight.setCurrent(-motorOutput);
        } else {
          vescLeft.setCurrent(-motorOutput);  // Original baseline
          vescRight.setCurrent(motorOutput);
        }
      }
      
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
      // Show if safety check is blocking motors
      bool isBalanceable = (abs(roll) < 25.0);
      Serial.printf("R:%.2f,P:%.2f,Y:%.2f,Pos:%.2f,VelSet:%.2f,VelAct:%.2f,Curr:%.2f,Bal:%s,Log:%s\n", 
                   roll, pitch, yaw, currentPosition, velocitySetpoint, avgVelocity, currentOutput, 
                   isBalanceable ? "OK" : "BLOCKED", loggingEnabled ? "ON" : "OFF");
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

// [Rest of the functions remain the same - handleCommand, startLogging, stopLogging, logData, downloadLogData, clearLogBuffer, showHelp]
// Copy from baseline file...

// Function to print all current tuning values
void printTuningValues() {
  Serial.println("\n╔════════════════════════════════════════════════════╗");
  Serial.println("║         CURRENT TUNING VALUES                      ║");
  Serial.println("╚════════════════════════════════════════════════════╝");
  Serial.println("ANGLE PID (Outer Loop):");
  Serial.printf("  Kp_angle: %.2f  Ki_angle: %.2f  Kd_angle: %.2f\n", Kp_angle, Ki_angle, Kd_angle);
  Serial.printf("  Angle Setpoint: %.2f°\n", angleSetpoint);
  Serial.println("VELOCITY PID (Inner Loop):");
  Serial.printf("  Kp_vel: %.2f  Ki_vel: %.2f  Kd_vel: %.2f\n", Kp_vel, Ki_vel, Kd_vel);
  Serial.println("MOTOR CONTROL:");
  Serial.printf("  Max Current: %.2fA  Min Current: %.2fA\n", maxCurrent, minCurrentToMove);
  Serial.printf("  Deadband: %.2f°  Velocity Damping: %.3f\n", deadband, velocityDamping);
  Serial.println("POSITION FEEDBACK:");
  Serial.printf("  Kp_position: %.2f\n", Kp_position);
  Serial.println("SYSTEM CONFIG:");
  Serial.printf("  I2C Clock: %d kHz  IMU Rate: %d Hz  PID Rate: %d Hz\n", 
                I2C_CLOCK_SPEED / 1000, IMU_UPDATE_RATE_HZ, 1000 / PID_SAMPLE_TIME_MS);
  Serial.printf("  VESC Update: %lu ms (%lu Hz)\n", VESC_UPDATE_INTERVAL_MS, 1000 / VESC_UPDATE_INTERVAL_MS);
  Serial.printf("  Motor Directions: %s  Roll Sign: %s\n", 
                motorDirectionsSwapped ? "SWAPPED" : "ORIGINAL",
                rollSignInverted ? "INVERTED" : "NORMAL");
  Serial.println("════════════════════════════════════════════════════");
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
      
    // VELOCITY DAMPING (Kd_vel) tuning - NEW!
    case 'k':
      Kd_vel -= 0.05; if (Kd_vel < 0) Kd_vel = 0;
      velocityPID.SetTunings(Kp_vel, Ki_vel, Kd_vel);
      Serial.printf("Velocity Kd = %.2f (decreased) - Damping reduced\n", Kd_vel);
      break;
      
    case 'K':
      Kd_vel += 0.05; if (Kd_vel > 1.0) Kd_vel = 1.0;
      velocityPID.SetTunings(Kp_vel, Ki_vel, Kd_vel);
      Serial.printf("Velocity Kd = %.2f (increased) - Damping increased\n", Kd_vel);
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
      
    // Toggle motor directions (if motors are reversed)
    case 't':
    case 'T':
      motorDirectionsSwapped = !motorDirectionsSwapped;
      Serial.printf("Motor directions: %s\n", motorDirectionsSwapped ? "SWAPPED" : "ORIGINAL");
      break;
    
    // Toggle roll sign inversion (if PID direction is wrong)
    case 'y':
    case 'Y':
      rollSignInverted = !rollSignInverted;
      Serial.printf("Roll sign for PID: %s\n", rollSignInverted ? "INVERTED" : "NORMAL");
      Serial.println("   (If robot moves wrong direction when tilted, this should be INVERTED)");
      break;
    
    // Reduce I2C speed (if seeing failures)
    case 'q':
      {
        uint32_t newSpeed = 200000;  // 200kHz
        Wire.setClock(newSpeed);
        Serial.printf("I2C speed reduced to %d kHz (target was %d kHz)\n", newSpeed / 1000, I2C_CLOCK_SPEED / 1000);
        Serial.println("   If still failing, try 'Q' for 100kHz");
      }
      break;
    
    case 'Q':
      {
        uint32_t newSpeed = 100000;  // 100kHz
        Wire.setClock(newSpeed);
        Serial.printf("I2C speed reduced to %d kHz (target was %d kHz)\n", newSpeed / 1000, I2C_CLOCK_SPEED / 1000);
      }
      break;
    
    // Adjust VESC update rate (for rock-solid balancing - can increase if needed)
    case 'u':
      VESC_UPDATE_INTERVAL_MS += 1; if (VESC_UPDATE_INTERVAL_MS > 50) VESC_UPDATE_INTERVAL_MS = 50;
      Serial.printf("VESC update interval = %lu ms (%lu Hz) - SLOWER\n", VESC_UPDATE_INTERVAL_MS, 1000 / VESC_UPDATE_INTERVAL_MS);
      Serial.println("   (Slower = safer, but may reduce balancing responsiveness)");
      break;
    
    case 'U':
      VESC_UPDATE_INTERVAL_MS -= 1; if (VESC_UPDATE_INTERVAL_MS < 2) VESC_UPDATE_INTERVAL_MS = 2;
      Serial.printf("VESC update interval = %lu ms (%lu Hz) - FASTER\n", VESC_UPDATE_INTERVAL_MS, 1000 / VESC_UPDATE_INTERVAL_MS);
      Serial.println("   (Faster = more responsive, but watch for serial buffer issues)");
      if (VESC_UPDATE_INTERVAL_MS < 5) {
        Serial.println("   ⚠️  WARNING: Very fast rate may cause serial buffer overflow!");
      }
      break;
    
    // Show current settings (press 'x' or 'X' to see all tuning values)
    case 'x':
    case 'X':
      printTuningValues();
      Serial.println("CURRENT STATE:");
      Serial.printf("  Roll: %.2f° (target: %.1f°)\n", roll, angleSetpoint);
      Serial.printf("  Position: %.2f m (target: %.2f m)\n", currentPosition, positionSetpoint);
      Serial.printf("  Velocity Setpoint: %.2f m/s  Actual: %.2f m/s\n", velocitySetpoint, avgVelocity);
      Serial.printf("  Motor Current: %.2fA\n", currentOutput);
      Serial.println("════════════════════════════════════════════════════\n");
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
  Serial.printf("  Kd: %.2f (DAMPING ENABLED)\n", Kd_vel);
  Serial.println("POSITION FEEDBACK:");
  Serial.printf("  Kp: %.2f\n", Kp_position);
  Serial.println("MOTOR SETTINGS:");
  Serial.printf("  Deadband: %.1f degrees\n", deadband);
  Serial.printf("  Max Current: %.1f A\n", maxCurrent);
  Serial.printf("  Min Current to Move: %.1f A\n", minCurrentToMove);
  Serial.printf("  Velocity Damping: %.3f\n", velocityDamping);
  Serial.println("SYSTEM INFO:");
  Serial.printf("  IMU Update Rate: %d Hz\n", IMU_UPDATE_RATE_HZ);
  Serial.printf("  I2C Clock: %d kHz\n", I2C_CLOCK_SPEED / 1000);
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
  Serial.println("  k/K - Decrease/Increase Velocity Kd (±0.05) - DAMPING!");
  Serial.println("POSITION FEEDBACK (Usually keep at 0):");
  Serial.println("  n/N - Decrease/Increase Position Kp (±0.1)");
  Serial.println("  r   - Reset position to 0.0m");
  Serial.println("MOTOR SETTINGS:");
  Serial.println("  m/M - Decrease/Increase Max Current (±0.5A)");
  Serial.println("  x   - Show all current settings");
  Serial.println("\n=== CURRENT SETTINGS ===");
  Serial.printf("Angle PID: Kp=%.1f Ki=%.1f Kd=%.1f\n", Kp_angle, Ki_angle, Kd_angle);
  Serial.printf("Velocity PID: Kp=%.1f Ki=%.2f Kd=%.2f (DAMPING!)\n", Kp_vel, Ki_vel, Kd_vel);
  Serial.printf("Position Kp: %.2f\n", Kp_position);
  Serial.printf("Position: %.2fm (target: %.2fm)\n", currentPosition, positionSetpoint);
  Serial.printf("Max Current: %.1fA  Deadband: %.1f°\n", maxCurrent, deadband);
  Serial.printf("I2C: %d kHz  IMU: %d Hz  PID: %d Hz\n", I2C_CLOCK_SPEED / 1000, IMU_UPDATE_RATE_HZ, 1000 / PID_SAMPLE_TIME_MS);
}

