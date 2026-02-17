/*
 * MOTOR CHARACTERIZATION TEST
 * 
 * Diagnostic tool to test VESC motors individually to identify
 * configuration or hardware asymmetry issues.
 * 
 * FEATURES:
 * - Individual motor testing (Left, Right, or Both)
 * - Smooth current ramping with configurable profiles
 * - High-speed CSV data logging (50Hz)
 * - Safety: Immediate stop on any unexpected command
 * 
 * USAGE:
 * - 'L': Test Left motor (ramp to 3.0A, hold 2s, ramp down)
 * - 'R': Test Right motor (ramp to 3.0A, hold 2s, ramp down)
 * - 'B': Test Both motors simultaneously
 * - Any other character: Emergency stop
 */

#include <VescUart.h>

// VESC objects
VescUart vescLeft;
VescUart vescRight;

// Test state
enum TestState {
  STATE_IDLE,
  STATE_RAMP_UP,
  STATE_HOLD,
  STATE_RAMP_DOWN
};

struct MotorTest {
  VescUart* vesc;
  TestState state;
  float targetCurrent;
  float currentCurrent;
  unsigned long testStartTime;
  unsigned long stateStartTime;
  bool active;
};

MotorTest leftTest = {&vescLeft, STATE_IDLE, 0.0, 0.0, 0, 0, false};
MotorTest rightTest = {&vescRight, STATE_IDLE, 0.0, 0.0, 0, 0, false};

// Test parameters
const float TARGET_CURRENT = 3.0;        // Target current (Amps)
const unsigned long RAMP_UP_TIME_MS = 1000;   // 1 second ramp up
const unsigned long HOLD_TIME_MS = 2000;      // 2 second hold
const unsigned long RAMP_DOWN_TIME_MS = 1000; // 1 second ramp down
const unsigned long CSV_INTERVAL_MS = 20;     // 50Hz = 20ms

// Data logging
unsigned long lastCsvPrint = 0;
unsigned long testStartTimestamp = 0;

// VESC data storage
float leftCurrent = 0.0;
float leftRPM = 0.0;
float rightCurrent = 0.0;
float rightRPM = 0.0;

// VESC read rate limiting (prevent serial buffer overflow)
const unsigned long VESC_READ_INTERVAL_MS = 15;  // 67Hz max (safe for VESC UART)
unsigned long lastVescRead = 0;

void setup() {
  Serial.begin(2000000);
  delay(1000);
  
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  
  Serial.println("╔════════════════════════════════════════════════════╗");
  Serial.println("║     MOTOR CHARACTERIZATION TEST                   ║");
  Serial.println("╚════════════════════════════════════════════════════╝");
  Serial.println();
  
  // Initialize VESCs
  Serial.println("⚙️  Initializing VESC motor controllers...");
  Serial1.begin(115200);
  Serial2.begin(115200);
  vescLeft.setSerialPort(&Serial1);
  vescRight.setSerialPort(&Serial2);
  Serial.println("   ✅ VESCs initialized");
  Serial.println();
  
  // Ensure motors are stopped
  vescLeft.setCurrent(0.0);
  vescRight.setCurrent(0.0);
  
  Serial.println("════════════════════════════════════════════════════");
  Serial.println("✅ SYSTEM READY");
  Serial.println("════════════════════════════════════════════════════");
  Serial.println();
  Serial.println("COMMANDS:");
  Serial.println("  L - Test Left motor (ramp to 3.0A, hold 2s, ramp down)");
  Serial.println("  R - Test Right motor (ramp to 3.0A, hold 2s, ramp down)");
  Serial.println("  B - Test Both motors simultaneously");
  Serial.println("  Any other character - Emergency stop");
  Serial.println();
  Serial.println("CSV OUTPUT FORMAT:");
  Serial.println("  R:0.00,P:0.00,Y:0.00,Err:0.00,YawErr:0.00,Vel:0.00,VelSet:targetCurrent,VelPID:0.00,RollOut:0.00,YawOut:0.00,Left:leftCurrent,Right:rightCurrent,Setpt:0.00,Mode:MOTOR_TEST,Yaw:OFF,Log:OFF,LeftRPM:leftRPM,RightRPM:rightRPM,Time:timestamp");
  Serial.println("  (Compatible with existing GUI tool - will display Left/Right motor currents)");
  Serial.println();
  Serial.println("Ready!");
}

void loop() {
  // Handle serial commands
  while (Serial.available()) {
    char cmd = Serial.read();
    // Skip whitespace (newlines, carriage returns, spaces, tabs)
    if (cmd == '\n' || cmd == '\r' || cmd == ' ' || cmd == '\t') {
      continue;
    }
    handleCommand(cmd);
  }
  
  // Update motor tests (this updates currentCurrent and applies to motors)
  updateMotorTest(leftTest);
  updateMotorTest(rightTest);
  
  // Update VESC readings (rate limited to prevent serial buffer overflow)
  // This updates RPM and current display values
  if (millis() - lastVescRead >= VESC_READ_INTERVAL_MS) {
    lastVescRead = millis();
    updateVescReadings();
  }
  
  // Print CSV data at 50Hz
  if (millis() - lastCsvPrint >= CSV_INTERVAL_MS) {
    lastCsvPrint = millis();
    printCsvData();
  }
  
  // Debug: Print test state every 2 seconds if any test is active
  static unsigned long lastDebugPrint = 0;
  if ((leftTest.active || rightTest.active) && (millis() - lastDebugPrint >= 2000)) {
    lastDebugPrint = millis();
    Serial.printf("🔍 DEBUG: Left active=%s, current=%.3fA, state=%d | Right active=%s, current=%.3fA, state=%d\n",
                 leftTest.active ? "YES" : "NO", leftTest.currentCurrent, leftTest.state,
                 rightTest.active ? "YES" : "NO", rightTest.currentCurrent, rightTest.state);
  }
  
  // Heartbeat LED
  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat >= 500) {
    lastHeartbeat = millis();
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }
}

void handleCommand(char cmd) {
  Serial.printf("\n📥 Command received: '%c' (0x%02X)\n", cmd, (unsigned char)cmd);
  
  // Emergency stop on any unexpected command
  if (cmd != 'L' && cmd != 'R' && cmd != 'B' && cmd != 'l' && cmd != 'r' && cmd != 'b') {
    Serial.println("⚠️  Invalid command - emergency stop");
    emergencyStop();
    return;
  }
  
  // Stop any active tests before starting new one
  emergencyStop();
  delay(100);
  
  switch (cmd) {
    case 'L':
    case 'l':
      Serial.println("\n🔧 Starting LEFT motor test...");
      startMotorTest(leftTest, TARGET_CURRENT);
      Serial.printf("   Left test active: %s, currentCurrent: %.3f\n", 
                   leftTest.active ? "YES" : "NO", leftTest.currentCurrent);
      break;
      
    case 'R':
    case 'r':
      Serial.println("\n🔧 Starting RIGHT motor test...");
      startMotorTest(rightTest, TARGET_CURRENT);
      Serial.printf("   Right test active: %s, currentCurrent: %.3f\n", 
                   rightTest.active ? "YES" : "NO", rightTest.currentCurrent);
      break;
      
    case 'B':
    case 'b':
      Serial.println("\n🔧 Starting BOTH motors test...");
      startMotorTest(leftTest, TARGET_CURRENT);
      startMotorTest(rightTest, TARGET_CURRENT);
      Serial.printf("   Left test active: %s, Right test active: %s\n", 
                   leftTest.active ? "YES" : "NO", rightTest.active ? "YES" : "NO");
      break;
  }
  
  testStartTimestamp = millis();
  Serial.println("CSV data stream starting...");
  Serial.println("(Format: R:P:Y:...Left:...Right:...LeftRPM:...RightRPM:...Time:...)");
}

void startMotorTest(MotorTest& test, float targetCurrent) {
  test.targetCurrent = targetCurrent;
  test.currentCurrent = 0.0;
  test.state = STATE_RAMP_UP;
  test.stateStartTime = millis();
  test.testStartTime = millis();
  test.active = true;
  Serial.printf("   Test started: target=%.2fA, state=RAMP_UP\n", targetCurrent);
}

void updateMotorTest(MotorTest& test) {
  if (!test.active) {
    return;
  }
  
  unsigned long elapsed = millis() - test.stateStartTime;
  
  switch (test.state) {
    case STATE_RAMP_UP:
      if (elapsed >= RAMP_UP_TIME_MS) {
        // Ramp complete, enter hold state
        test.currentCurrent = test.targetCurrent;
        test.state = STATE_HOLD;
        test.stateStartTime = millis();
      } else {
        // Linear ramp: current = target * (elapsed / ramp_time)
        test.currentCurrent = test.targetCurrent * ((float)elapsed / (float)RAMP_UP_TIME_MS);
      }
      break;
      
    case STATE_HOLD:
      test.currentCurrent = test.targetCurrent;
      if (elapsed >= HOLD_TIME_MS) {
        // Hold complete, enter ramp down
        test.state = STATE_RAMP_DOWN;
        test.stateStartTime = millis();
      }
      break;
      
    case STATE_RAMP_DOWN:
      if (elapsed >= RAMP_DOWN_TIME_MS) {
        // Ramp down complete, stop motor
        test.currentCurrent = 0.0;
        test.state = STATE_IDLE;
        test.active = false;
        Serial.printf("\n✅ Motor test complete (duration: %.1f s)\n", 
                     (millis() - test.testStartTime) / 1000.0);
      } else {
        // Linear ramp down: current = target * (1 - elapsed / ramp_time)
        test.currentCurrent = test.targetCurrent * (1.0 - ((float)elapsed / (float)RAMP_DOWN_TIME_MS));
      }
      break;
      
    case STATE_IDLE:
      test.currentCurrent = 0.0;
      test.active = false;
      break;
  }
  
  // Apply current to motor
  test.vesc->setCurrent(test.currentCurrent);
}

void updateVescReadings() {
  // Read left VESC for RPM data
  if (vescLeft.getVescValues()) {
    leftRPM = vescLeft.data.rpm;
    // NOTE: VESC library may provide actual motor current in data structure
    // Common field names: avgMotorCurrent, avgInputCurrent, inCurrent, motorCurrent
    // If your VESC library provides actual current, uncomment and use the appropriate field:
    // leftCurrent = vescLeft.data.avgMotorCurrent;  // This would override commanded current with actual
  } else {
    // If VESC read fails, keep last RPM value (don't reset to 0)
    // leftRPM remains unchanged
  }
  
  // Small delay between VESC reads to prevent serial buffer overflow
  delayMicroseconds(500);
  
  // Read right VESC for RPM data
  if (vescRight.getVescValues()) {
    rightRPM = vescRight.data.rpm;
    // NOTE: See comment above for left VESC - adjust if your library provides actual current
    // rightCurrent = vescRight.data.avgMotorCurrent;  // This would override commanded current with actual
  } else {
    // If VESC read fails, keep last RPM value (don't reset to 0)
    // rightRPM remains unchanged
  }
}

void printCsvData() {
  // Always update current values from test state before printing
  leftCurrent = leftTest.currentCurrent;
  rightCurrent = rightTest.currentCurrent;
  
  // Calculate target current (average of active tests)
  float targetCurrent = 0.0;
  if (leftTest.active) targetCurrent = leftTest.currentCurrent;
  if (rightTest.active) {
    if (leftTest.active) {
      targetCurrent = (leftTest.currentCurrent + rightTest.currentCurrent) / 2.0;
    } else {
      targetCurrent = rightTest.currentCurrent;
    }
  }
  
  // Format compatible with existing GUI tool
  // Uses R:P:Y: format for compatibility, but this is MOTOR TEST data
  // GUI will parse Left: and Right: for motor currents
  // Format: R:0.00,P:0.00,Y:0.00,Err:0.00,YawErr:0.00,Vel:0.00,VelSet:targetCurrent,VelPID:0.00,RollOut:0.00,YawOut:0.00,Left:leftCurrent,Right:rightCurrent,Setpt:0.00,Mode:MOTOR_TEST,Yaw:OFF,Log:OFF,LeftRPM:leftRPM,RightRPM:rightRPM,Time:timestamp
  unsigned long timestamp = (testStartTimestamp > 0) ? (millis() - testStartTimestamp) : 0;
  
  // Primary format for GUI compatibility (GUI will parse Left: and Right:)
  Serial.printf("R:0.00,P:0.00,Y:0.00,Err:0.00,YawErr:0.00,Vel:0.00,VelSet:%.3f,VelPID:0.00,RollOut:0.00,YawOut:0.00,Left:%.3f,Right:%.3f,Setpt:0.00,Mode:MOTOR_TEST,Yaw:OFF,Log:OFF,LeftRPM:%.1f,RightRPM:%.1f,Time:%lu\n",
               targetCurrent,
               leftCurrent,
               rightCurrent,
               leftRPM,
               rightRPM,
               timestamp);
}

void emergencyStop() {
  // Stop all motors immediately
  vescLeft.setCurrent(0.0);
  vescRight.setCurrent(0.0);
  
  // Reset test states
  leftTest.active = false;
  leftTest.state = STATE_IDLE;
  leftTest.currentCurrent = 0.0;
  
  rightTest.active = false;
  rightTest.state = STATE_IDLE;
  rightTest.currentCurrent = 0.0;
  
  testStartTimestamp = 0;
  
  Serial.println("\n🛑 EMERGENCY STOP - All motors disabled");
}
