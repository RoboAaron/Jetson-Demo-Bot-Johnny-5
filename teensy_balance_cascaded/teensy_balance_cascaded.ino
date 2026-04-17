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
const int8_t BNO085_RST_PIN = 17;  // Teensy pin 17 → BNO085 RST (active-low). Set to -1 if not wired.
Adafruit_BNO08x bno08x(BNO085_RST_PIN);
sh2_SensorValue_t sensorValue;
VescUart vescLeft;
VescUart vescRight;
IntervalTimer anglePIDTimer;

// IMU data
bool imuWorking = false;
bool ignoreNextImuReset = false;
bool firstImuDataReceived = false;
unsigned long firstImuDataTime = 0;
const unsigned long IMU_SETTLE_MS = 500;
float pitch = 0.0;
volatile float yaw = 0.0f;
volatile float roll = 0.0f;
volatile bool newImuData = false;
volatile float gyroPitchRate = 0.0f;  // rad/s, positive = tilting forward
volatile float imuVelocity = 0.0f;    // m/s, pitch-rate integrated estimate (Option A)
unsigned long lastGyroMicros = 0;

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
const float VELOCITY_FILTER_ALPHA = 0.30;  // IMU velocity is cleaner; use lighter filtering for responsiveness
const float VELOCITY_DEADBAND = 0.02;      // IMU standstill signal is near genuine zero
const float VELOCITY_OUTPUT_MAX = 0.5;    // Maximum velocity PID output (±degrees)
const float VELOCITY_SLEW_RATE = 0.05;    // Maximum change per update (degrees)
float velScale = 1.0f;                    // Tunable scale factor for IMU pitch-rate velocity model
const float VEL_DECAY = 0.002f;           // Per-sample decay to bound long-term integration drift
const float VEL_SCALE_STEP_COARSE = 0.10f;
const float VEL_SCALE_STEP_FINE = 0.02f;
float lastAngleSetpointFromVel = 0.0;     // For slew rate limiting
// Moving-average buffer (samples of EMA output) for extra smoothing at standstill
const int VELOCITY_MA_SIZE = 6;
float velocityEmaBuffer[VELOCITY_MA_SIZE];
int velocityEmaIndex = 0;
bool velocityMaFilled = false;

// === CHANGED === Runtime flag: when false, velocity loop is off and angle setpoint = baseSetpoint exactly.
bool useVelocityLoop = false;  // Default false for testing; 'v' toggles. Match single-loop first.

// ANGLE CONTROL LOOP (Inner Loop - from single-loop)
double baseSetpoint = -0.84;  // Field-validated CG lean (robot_log_20260416_232745.txt); EEPROM overrides if saved
volatile float angleSetpoint = 0.0f;   // Active setpoint = baseSetpoint + angleSetpointFromVel (or baseSetpoint when vel loop off)
volatile float angleInput = 0.0f;      // Current roll angle (degrees)
volatile float motorCurrent = 0.0f;    // PID output: motor current (Amps)
volatile float leftMotorCurrent = 0.0f;
volatile float rightMotorCurrent = 0.0f;
volatile bool motorCommandPending = false;
volatile bool safetyCutoffActive = true;
volatile bool pidFrozen = false;

// PID_v1 uses double pointers; keep dedicated PID backing variables and mirror to volatile floats.
double pidAngleSetpoint = 0.0;
double pidAngleInput = 0.0;
double pidMotorCurrent = 0.0;

// Angle PID defaults tuned to match the filtered single-loop controller.
double Kp = 1.5;    // Field default after PID latch fix (232745); tune on your floor
double Ki = 0.0;    // Integral gain (start 0, add small later if needed)
double Kd = 0.06;   // Field default with Filt=0.25; raise for more damping if pushes still ring

// Angle input low-pass filter for derivative-noise suppression.
// Alpha = 0.3 gives roughly a 24 Hz cutoff at the 500 Hz angle PID rate.
float angleFilterAlpha = 0.25f;  // Field default: less lag than 0.15 during pushes (232745)

PID balancePID(&pidAngleInput, &pidMotorCurrent, &pidAngleSetpoint, Kp, Ki, Kd, DIRECT);

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
const float WHEEL_RADIUS = WHEEL_DIAMETER * 0.5f;
const int MOTOR_POLES = 30;          // Total motor poles (from VESC XML: si_motor_poles=30)
const int POLE_PAIRS = 15;           // Pole pairs (MOTOR_POLES / 2 = 15)
const float GEAR_RATIO = 1.0;        // Gear ratio (from VESC XML: gear_ratio=1, direct drive)
// Convert mechanical RPM to m/s: mps = mech_rpm * pi * wheel_diameter / 60
const float RPM_TO_MPS = (WHEEL_DIAMETER * PI) / 60.0;

// Motor control parameters
float maxCurrent = 2.0f;  // Field default for low-traction / cardboard (232745); raise on grippy floor
float minCurrent = 0.0;  // DEPRECATED: Use stiction compensation instead (kept for EEPROM compatibility)

// Stiction (static friction) compensation
// Motors need ~0.55A to overcome static friction and start moving.
// Without this, small PID corrections produce no motion, destabilizing velocity control.
const float MIN_DRIVE_CURRENT = 0.55f;  // Stiction breakaway current (Amps)
const float DRIVE_ZERO_EPS = 0.01f;     // Treat commands below this as true zero (Amps)

// Single-loop parity mode for algorithm A/B isolation:
// when velocity+yaw are OFF, emulate single-loop actuator behavior.
const bool SINGLE_LOOP_PARITY_WHEN_VEL_OFF = false;
const float PARITY_MIN_CURRENT = 0.1f;
// True = do not read VESC feedback when velocity loop is OFF (clean angle-only A/B mode).
const bool BYPASS_VESC_FEEDBACK_WHEN_VEL_OFF = true;

// Fine adjust mode (for smaller tuning steps)
bool fineAdjust = false;
// Dry-run mode: keep computing/logging commanded currents but suppress motor commands.
bool motorOutputEnabled = false;
const float KP_STEP_COARSE = 0.5;
const float KP_STEP_FINE = 0.1;
const float KI_STEP_COARSE = 0.05;
const float KI_STEP_FINE = 0.01;
const float KD_STEP_COARSE = 0.01;   // Kd is sensitive at 500 Hz; coarse 0.01, fine 0.002
const float KD_STEP_FINE = 0.002;
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
const uint32_t SETTINGS_MAGIC = 0xC45C4DF0;  // Bump forces proven defaults: Kp=1.5,Kd=0.06,base=-0.84,Filt=0.25,maxI=2.0 + 4-8 rearm/dither/GAINS line

bool loadSettings();
void saveSettings();
void runAngleControlISR();

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
const uint32_t I2C_CLOCK_SPEED = 400000;  // 400kHz Fast Mode — required for 400Hz rotation vector bandwidth
const uint8_t BNO085_PRIMARY_ADDRESS = 0x4B;
const uint8_t BNO085_ALTERNATE_ADDRESS = 0x4A;
const uint32_t IMU_UPDATE_RATE_HZ = 400;  // 400Hz rotation vector
const uint32_t IMU_REPORT_INTERVAL_US = 2500;  // 2500 µs = 400Hz
const uint32_t GYRO_REPORT_INTERVAL_US = 10000; // 10000 µs = 100Hz (gyro not used in control loop yet)

// PID Update Rates
const uint32_t PID_SAMPLE_TIME_MS = 2;  // 2ms = 500Hz (angle loop - fast)
const uint32_t VELOCITY_PID_SAMPLE_TIME_MS = 50;  // 50ms = 20Hz (velocity loop - slower, stable)

// VESC Communication Rate Limiting (CRITICAL: Prevents serial buffer overflow)
const uint32_t VESC_UPDATE_INTERVAL_MS = 15;  // 15ms = 67Hz (prevents buffer overflow)
const uint32_t MOTOR_COMMAND_UPDATE_INTERVAL_MS = 5;  // 5ms = 200Hz motor command dispatch from loop()

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
  float velocity;
  float velocitySetpoint;
};

LogData logBuffer[1000]; // 20 seconds at 50Hz
int logIndex = 0;
bool bufferFull = false;
bool logBufferFullNotified = false;

// Anti-stiction dither: add a small zero-mean sinusoidal current so the motor
// never sits at exactly zero. Replaces the old piecewise stiction comp which
// created a 0.55 A step at zero-crossing and produced a limit cycle.
// Zero DC bias, no discontinuity, no reliance on knowing the exact stiction
// breakaway current (which varies with motor temperature and wear).
static const float DITHER_AMPLITUDE = 0.08f;   // Peak dither current (Amps)
static const float DITHER_FREQ_HZ   = 40.0f;   // Dither frequency (Hz); no gearbox, so resonance is not a concern

float applyDither(float cmdA) {
  float t_sec = micros() * 1.0e-6f;
  float dither = DITHER_AMPLITUDE * sinf(2.0f * PI * DITHER_FREQ_HZ * t_sec);
  return cmdA + dither;
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

// === PID state-reset helpers (4-8) ===
// PID_v1.Initialize() snapshots *myOutput into outputSum on every
// MANUAL -> AUTOMATIC transition. With Ki=0 and P_ON_E, outputSum is
// never updated inside Compute(), so any non-zero value latched at
// Initialize() becomes a permanent DC bias on the output until another
// mode transition. These helpers zero the output double *before*
// SetMode(AUTOMATIC) so Initialize() always snapshots a clean zero.
// They also snap the input pointer to the current live value so the
// first post-rearm Compute() sees dInput == 0 (no spurious D kick).
static inline void rearmBalancePID() {
  pidMotorCurrent = 0.0;
  pidAngleInput = angleInput;
  pidAngleSetpoint = angleSetpoint;
  balancePID.SetMode(AUTOMATIC);
}

static inline void rearmVelocityPID() {
  angleSetpointFromVel = 0.0;
  lastAngleSetpointFromVel = 0.0f;
  velocityPID.SetMode(AUTOMATIC);
}

static inline void rearmYawPID() {
  yawOutput = 0.0;
  yawSetpoint = yaw;  // hold current yaw as new target after rearm
  yawPID.SetMode(AUTOMATIC);
}

void runAngleControlISR() {
  if (!imuWorking || safetyCutoffActive) {
    if (!pidFrozen) {
      balancePID.SetMode(MANUAL);
      pidFrozen = true;
    }
    motorCurrent = 0.0f;
    pidMotorCurrent = 0.0;  // belt-and-braces: keep PID output double clean
    leftMotorCurrent = 0.0f;
    rightMotorCurrent = 0.0f;
    motorCommandPending = true;
    return;
  }

  if (pidFrozen) {
    rearmBalancePID();
    pidFrozen = false;
  }

  float outputCurrent = 0.0f;
  if (controlMode == MODE_DIAGNOSTIC) {
    float angleError = roll - angleSetpoint;
    if (fabs(angleError) >= 0.2f) {
      outputCurrent = constrain(angleError * 2.0f, -maxCurrent, maxCurrent);
    }
    motorCurrent = outputCurrent;
  } else {
    pidAngleInput = angleInput;
    pidAngleSetpoint = angleSetpoint;
    balancePID.Compute();
    motorCurrent = (float)pidMotorCurrent;
    outputCurrent = motorCurrent;
  }

  float yawCorr = 0.0f;
  if (yawControlEnabled) {
    static bool yawSetpointInitialized = false;
    if (!yawSetpointInitialized) yawSetpoint = yaw;
    yawSetpointInitialized = true;

    yawInput = yaw;
    yawPID.Compute();
    yawCorr = (float)yawOutput;
    if (isnan(yawCorr) || isinf(yawCorr)) yawCorr = 0.0f;
    float maxYawOutput = maxCurrent * 0.3f;
    yawCorr = constrain(yawCorr, -maxYawOutput, maxYawOutput);
    yawOutput = yawCorr;
  } else {
    yawOutput = 0.0;
  }

  float leftCmd = -outputCurrent - yawCorr;
  float rightCmd = outputCurrent + yawCorr;

  leftCmd *= LEFT_MOTOR_DIRECTION_SIGN;
  rightCmd *= RIGHT_MOTOR_DIRECTION_SIGN;

  bool velocityLoopActive = useVelocityLoop && !FORCE_SINGLE_LOOP_MODE;
  bool parityModeActive = SINGLE_LOOP_PARITY_WHEN_VEL_OFF && !velocityLoopActive && !yawControlEnabled;
  if (parityModeActive) {
    if (fabs(leftCmd) < PARITY_MIN_CURRENT) leftCmd = 0.0f;
    if (fabs(rightCmd) < PARITY_MIN_CURRENT) rightCmd = 0.0f;
  } else {
    leftCmd = applyDither(leftCmd);
    rightCmd = applyDither(rightCmd);
  }

  leftMotorCurrent = constrain(leftCmd, -maxCurrent, maxCurrent);
  rightMotorCurrent = constrain(rightCmd, -maxCurrent, maxCurrent);
  motorCommandPending = true;
  newImuData = false;
}

bool enableIMUReports() {
  bool ok = true;
  if (!bno08x.enableReport(SH2_ROTATION_VECTOR, IMU_REPORT_INTERVAL_US)) {
    Serial.println("   ❌ Could not enable rotation vector");
    ok = false;
  } else {
    Serial.printf("   ✅ Rotation vector enabled at %d Hz\n", IMU_UPDATE_RATE_HZ);
  }
  if (!bno08x.enableReport(SH2_GYROSCOPE_CALIBRATED, GYRO_REPORT_INTERVAL_US)) {
    Serial.println("   ❌ Could not enable gyroscope calibrated report");
  } else {
    Serial.printf("   ✅ Gyroscope calibrated report enabled at %d Hz\n", 1000000 / GYRO_REPORT_INTERVAL_US);
  }
  return ok;
}

// SH2_RESET clears active report subscriptions. Re-send sensor config after any wasReset(),
// with the same settle + second enable timing used at startup (without begin_I2C here:
// the SHTP session must stay open; begin_I2C in loop() can strand I2C after a bad sequence).
bool recoverIMUReportsAfterSh2Reset() {
  delay(500);
  if (!enableIMUReports()) return false;
  delay(500);
  return enableIMUReports();
}

bool initializeIMUAtAddress(uint8_t address) {
  if (!bno08x.begin_I2C(address)) return false;

  Serial.printf("   ✅ IMU initialized at 0x%02X\n", address);
  if (!enableIMUReports()) return false;

  imuWorking = true;
  // BNO085 internal sensor engine needs extra time after SHTP init.
  // Perform a second reset + re-enable after 500ms to ensure reports start.
  Serial.println("   Waiting for BNO085 sensor engine...");
  delay(500);
  digitalWrite(BNO085_RST_PIN, LOW);
  delay(10);
  digitalWrite(BNO085_RST_PIN, HIGH);
  delay(500);
  if (!enableIMUReports()) {
    Serial.println("   ❌ Second enable failed");
    imuWorking = false;
    return false;
  }

  ignoreNextImuReset = true;
  Serial.println("   ✅ BNO085 sensor engine ready");
  return true;
}

void setup() {
  // Assert BNO085 reset IMMEDIATELY — before oscillator can start in noisy environment
  pinMode(BNO085_RST_PIN, OUTPUT);
  digitalWrite(BNO085_RST_PIN, LOW);  // Oscillator cannot start while RST is LOW

  // Pull up unused pin near I2C lines to reduce floating wire antenna effect
  pinMode(15, INPUT_PULLUP);

  Serial.begin(2000000);
  delay(2000);

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

  // Initialize I2C
  Serial.println("📡 Initializing I2C bus...");
  Wire.begin();
  Wire.setClock(I2C_CLOCK_SPEED);
  Serial.printf("   ✓ I2C initialized at %d kHz\n", I2C_CLOCK_SPEED / 1000);
  Serial.printf("   ✓ BNO085 RST pin: %d (held LOW since power-on)\n", BNO085_RST_PIN);
  Serial.println();

  // Initialize VESCs (while BNO085 is still held in reset)
  Serial.println("⚙️  Initializing VESC motor controllers...");
  Serial1.begin(115200);
  Serial2.begin(115200);
  vescLeft.setSerialPort(&Serial1);
  vescRight.setSerialPort(&Serial2);
  Serial.println("   ✅ VESCs initialized");
  delay(200);

  // Release BNO085 reset — oscillator starts NOW in a settled environment
  Serial.println("   Releasing BNO085 RST...");
  digitalWrite(BNO085_RST_PIN, HIGH);
  delay(500);  // Full oscillator startup + SHTP boot time
  Serial.println("   ✅ BNO085 released from reset");
  Serial.println();

  // Initialize IMU — try 0x4B first (matches working baseline), then 0x4A
  Serial.println("🔧 Initializing IMU...");
  for (int attempt = 1; attempt <= 3; attempt++) {
    Serial.printf("   IMU init attempt %d/3...\n", attempt);
    if (initializeIMUAtAddress(BNO085_PRIMARY_ADDRESS)) break;

    Serial.println("   Trying alternate address 0x4A...");
    if (initializeIMUAtAddress(BNO085_ALTERNATE_ADDRESS)) break;

    if (attempt < 3) {
      Serial.println("   Retrying in 500ms...");
      delay(500);
      // Re-pulse RST before retry
      digitalWrite(BNO085_RST_PIN, LOW);
      delay(10);
      digitalWrite(BNO085_RST_PIN, HIGH);
      delay(500);
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

  // Load saved settings (if available)
  if (loadSettings()) {
    Serial.println("💾 Loaded saved settings from EEPROM");
  } else {
    Serial.println("💾 No saved settings found (using defaults)");
    Serial.println("⚠️  EEPROM INVALID — running from SOURCE DEFAULTS. Press 'k' to save once tuned.");
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
  balancePID.SetOutputLimits(-maxCurrent, maxCurrent);
  balancePID.SetSampleTime(PID_SAMPLE_TIME_MS);  // 500Hz update rate
  rearmBalancePID();  // Clean initial Initialize() (see 4-8 state-reset helpers)

  // === CHANGED === Velocity PID: start in MANUAL when velocity loop disabled (useVelocityLoop false).
  velocityPID.SetOutputLimits(-VELOCITY_OUTPUT_MAX, VELOCITY_OUTPUT_MAX);
  velocityPID.SetSampleTime(VELOCITY_PID_SAMPLE_TIME_MS);
  if (useVelocityLoop && !FORCE_SINGLE_LOOP_MODE) {
    rearmVelocityPID();
  } else {
    velocityPID.SetMode(MANUAL);
    angleSetpointFromVel = 0.0;
    lastAngleSetpointFromVel = 0.0;
  }

  // Initialize yaw PID controller
  yawPID.SetOutputLimits(-maxCurrent, maxCurrent);
  yawPID.SetSampleTime(PID_SAMPLE_TIME_MS);  // 500Hz update rate
  rearmYawPID();

  Serial.printf("   ✅ Angle PID: %d Hz update rate\n", 1000 / PID_SAMPLE_TIME_MS);
  Serial.printf("   ✅ Velocity PID: %d Hz update rate\n", 1000 / VELOCITY_PID_SAMPLE_TIME_MS);
  Serial.printf("   ✅ Angle Gains: Kp=%.2f, Ki=%.2f, Kd=%.3f\n", Kp, Ki, Kd);
  Serial.printf("   ✅ Velocity Gains: Kp=%.2f, Ki=%.2f, Kd=%.2f\n", Kp_vel, Ki_vel, Kd_vel);
  Serial.printf("   ✅ Max Current: %.1fA\n", maxCurrent);
  anglePIDTimer.begin(runAngleControlISR, 2000);  // 500 Hz hardware timer ISR
  Serial.println("   ✅ Angle PID ISR timer: 500 Hz (2000 us)");
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
  Serial.println("n/N - Decrease/Increase IMU velocity scale (VEL_SCALE)");
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
  Serial.println("Ready! Motors DISARMED — press 'o' to arm when robot is upright.");
}

void loop() {
  static unsigned long lastPrint = 0;
  static unsigned long lastIMUUpdate = 0;
  static unsigned long imuReadCount = 0;
  static unsigned long imuFailCount = 0;
  static unsigned long lastIMUStats = 0;
  static unsigned long prevIMUReadCount = 0;
  static unsigned long prevIMUStatsTime = 0;
  
  // Handle serial commands (process ALL available commands to prevent buffer overflow)
  while (Serial.available()) {
    char cmd = Serial.read();
    handleCommand(cmd);
  }
  bool velocityLoopActive = useVelocityLoop && !FORCE_SINGLE_LOOP_MODE;
  
  // Get IMU data with failure tracking
  bool imuDataReceived = false;
  if (imuWorking) {
    if (bno08x.wasReset()) {
      if (ignoreNextImuReset) {
        Serial.println("\nℹ️  BNO085 startup reset acknowledged — re-enabling reports...");
        ignoreNextImuReset = false;
      } else {
        Serial.println("\n⚠️  BNO085 SPONTANEOUS RESET DETECTED — re-enabling reports...");
      }
      if (recoverIMUReportsAfterSh2Reset()) {
        Serial.println("   ✅ Reports re-enabled after reset");
      } else {
        Serial.println("   ❌ Failed to re-enable reports after reset");
        imuWorking = false;
      }
      firstImuDataReceived = false;
      firstImuDataTime = 0;
      imuVelocity = 0.0f;
      lastGyroMicros = 0;
    }

    while (bno08x.getSensorEvent(&sensorValue)) {
      if (sensorValue.sensorId == SH2_ROTATION_VECTOR) {
        imuDataReceived = true;
        imuReadCount++;
        lastIMUUpdate = millis();
        if (!firstImuDataReceived) {
          firstImuDataReceived = true;
          firstImuDataTime = millis();
        }

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

        // Low-pass filter remains in loop; angle ISR consumes angleInput at fixed 500 Hz.
        static float angleFiltered = 0.0f;
        static bool angleFilterInitialized = false;
        if (!angleFilterInitialized) {
          angleFiltered = roll;
          angleFilterInitialized = true;
        }
        angleFiltered = angleFilterAlpha * roll + (1.0f - angleFilterAlpha) * angleFiltered;
        angleInput = angleFiltered;
        newImuData = true;
      } else if (sensorValue.sensorId == SH2_GYROSCOPE_CALIBRATED) {
        // Gyro Y axis = pitch rate. Sign: positive = tilting forward.
        // IMU is mounted upside down, so negate to match roll orientation correction.
        gyroPitchRate = -sensorValue.un.gyroscope.y;

        unsigned long nowGyroMicros = micros();
        if (lastGyroMicros > 0) {
          float dt = (nowGyroMicros - lastGyroMicros) / 1000000.0f;
          if (dt > 0.0f && dt < 0.03f) {
            float rollRad = roll * (PI / 180.0f);
            imuVelocity += gyroPitchRate * WHEEL_RADIUS * velScale * cosf(rollRad) * dt;
            imuVelocity *= (1.0f - VEL_DECAY);
            imuVelocity = constrain(imuVelocity, -VELOCITY_MAX, VELOCITY_MAX);
          }
        }
        lastGyroMicros = nowGyroMicros;
      }
    }

    if (!imuDataReceived) {
      imuFailCount++;

      bool dataLost  = (lastIMUUpdate > 0) && (millis() - lastIMUUpdate > 100);
      bool neverSent = (lastIMUUpdate == 0) && (millis() > 3000);

      if (dataLost || neverSent) {
        static unsigned long lastIMUWarning = 0;
        if (millis() - lastIMUWarning > 2000) {
          if (neverSent) {
            Serial.printf("\n⚠️  IMU NEVER SENT DATA! Init looked OK but no rotation vectors after %lu ms\n", millis());
            Serial.println("   Likely cause: I2C bus marginal — init ACKs but sensor can't sustain transfer");
          } else {
            Serial.printf("\n⚠️  IMU COMMUNICATION LOST! Last update: %lu ms ago\n", millis() - lastIMUUpdate);
          }
          Serial.printf("   Rotation-vector events: %lu, Loop passes without new event: %lu\n", imuReadCount, imuFailCount);
          Serial.println("   Note: 'without new event' is a loop-level metric, not direct I2C transaction failures.");
          Serial.println("   Possible causes: Loose wiring, I2C speed too high, weak pull-ups");
          printTuningValues();
          lastIMUWarning = millis();

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
      unsigned long nowMs = millis();
      unsigned long totalLoops = imuReadCount + imuFailCount;
      float loopHitRate = (totalLoops > 0) ? (100.0f * imuReadCount / totalLoops) : 0.0f;
      unsigned long deltaEvents = imuReadCount - prevIMUReadCount;
      unsigned long deltaMs = (prevIMUStatsTime > 0) ? (nowMs - prevIMUStatsTime) : 0;
      float eventRateHz = (deltaMs > 0) ? (1000.0f * deltaEvents / deltaMs) : 0.0f;

      if (lastIMUUpdate > 0) {
        Serial.printf("📊 IMU Stats: RV events=%lu (+%lu, %.1f Hz), Last update=%lu ms ago\n",
                      imuReadCount, deltaEvents, eventRateHz, nowMs - lastIMUUpdate);
      } else {
        Serial.printf("📊 IMU Stats: RV events=%lu (+%lu, %.1f Hz), Last update=never\n",
                      imuReadCount, deltaEvents, eventRateHz);
      }
      Serial.printf("📊 IMU Loop Metric: no-new-event loops=%lu (hit=%.1f%%, diagnostic only)\n",
                    imuFailCount, loopHitRate);
      
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
      
      prevIMUReadCount = imuReadCount;
      prevIMUStatsTime = nowMs;
      lastIMUStats = millis();
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
  float velocityFeedback = imuWorking ? constrain((float)imuVelocity, -2.0f, 2.0f) : 0.0f;
  bool inDeadband = (fabs(velocitySetpoint) < 0.01f) && (fabs(velocityFeedback) < VELOCITY_DEADBAND);
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
        rearmVelocityPID();
      }
      velocityInput = velocityFeedback;
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
      float velocityError = velocitySetpoint - velocityFeedback;
      Serial.printf("🔍 VEL: imu=%.3f vescRaw=%.3f vescFilt=%.3f err=%.3f angleFromVel=%.4f° setpt=%.3f totalSetpt=%.2f° scale=%.2f\n",
                   velocityFeedback, avgVelocity, filteredVelocity, velocityError, angleSetpointFromVel,
                   velocitySetpoint, baseSetpoint + angleSetpointFromVel, velScale);
    }
    static int signVerificationCounter = 0;
    signVerificationCounter++;
    if (signVerificationCounter >= 20) {
      signVerificationCounter = 0;
      Serial.printf("✅ SIGN CHECK: setpt=%.3f m/s, imuVel=%.3f m/s, angleFromVel=%.4f° | Expected: +setpt with -vel → -angleFromVel (forward tilt)\n",
                   velocitySetpoint, velocityFeedback, angleSetpointFromVel);
    }
  }

  if (imuWorking) {
    bool imuSettled = firstImuDataReceived && (millis() - firstImuDataTime >= IMU_SETTLE_MS);
    bool isBalanceable = imuSettled && (abs(roll) < 25.0f);
    safetyCutoffActive = !isBalanceable;

    if (!isBalanceable) {
      velocityPID.SetMode(MANUAL);
      yawPID.SetMode(MANUAL);
      motorCurrent = 0.0f;
      pidMotorCurrent = 0.0;  // keep PID output double clean for next rearm (4-8)
      leftMotorCurrent = 0.0f;
      rightMotorCurrent = 0.0f;
      angleSetpointFromVel = 0.0;  // Reset velocity PID output
      yawOutput = 0.0;             // keep yaw PID output double clean for next rearm (4-8)

      static unsigned long lastSafetyDebug = 0;
      if (millis() - lastSafetyDebug > 1000) {
        if (!imuSettled) {
          Serial.printf("⏳ IMU settling... firstData=%s, elapsed=%lu ms (need %lu ms)\n",
                       firstImuDataReceived ? "yes" : "no",
                       firstImuDataReceived ? (millis() - firstImuDataTime) : 0UL,
                       IMU_SETTLE_MS);
        } else {
          Serial.printf("⚠️  SAFETY: Roll=%.2f° exceeds limit (25°) - Motors DISABLED\n", roll);
        }
        lastSafetyDebug = millis();
      }
    } else {
      if (velocityLoopActive && !FORCE_SINGLE_LOOP_MODE) rearmVelocityPID();
      rearmYawPID();

      // === CHANGED === Rich debug every 100 ms when logging enabled (angleInput, setpoint, vel, deadband, useVelocityLoop, current).
      static unsigned long lastDebug = 0;
      if (loggingEnabled && (millis() - lastDebug >= 100)) {
        lastDebug = millis();
        Serial.printf("DBG100: angleIn=%.3f setpt=%.3f fromVel=%.3f filtVel=%.3f inDB=%d useVel=%d motor=%.3f\n",
                     angleInput, angleSetpoint, angleSetpointFromVel, filteredVelocity,
                     inDeadband ? 1 : 0, useVelocityLoop ? 1 : 0, motorCurrent);
      } else if (!loggingEnabled && (millis() - lastDebug > 500)) {
        lastDebug = millis();
        Serial.printf("🔧 DEBUG: Roll=%.2f°, Vel=%.3f/%.3f, VelPID=%.3f°, Out=%.4fA, Left=%.4fA, Right=%.4fA\n",
                     roll, filteredVelocity, velocitySetpoint, angleSetpointFromVel, motorCurrent,
                     leftMotorCurrent, rightMotorCurrent);
      }
      
      // Log data if enabled
      if (loggingEnabled && (millis() - lastLogTime >= LOG_INTERVAL)) {
        logData(motorCurrent, leftMotorCurrent, rightMotorCurrent, yawOutput);
        lastLogTime = millis();
      }
    }
  } else {
    // IMU not working - ISR will hold motors at zero.
    safetyCutoffActive = true;
    motorCurrent = 0.0f;
    pidMotorCurrent = 0.0;  // keep PID output double clean for next rearm (4-8)
    leftMotorCurrent = 0.0f;
    rightMotorCurrent = 0.0f;
    yawOutput = 0.0;
    angleSetpointFromVel = 0.0;
  }

  // Dispatch motor commands from loop() only (never from ISR).
  // This prevents serial/UART congestion and stream lockups when safety trips.
  static unsigned long lastMotorCommandWrite = 0;
  static bool forcedZeroSent = false;
  bool pendingMotorCommand = false;
  float pendingLeftCurrent = 0.0f;
  float pendingRightCurrent = 0.0f;
  noInterrupts();
  if (motorCommandPending) {
    pendingMotorCommand = true;
    pendingLeftCurrent = leftMotorCurrent;
    pendingRightCurrent = rightMotorCurrent;
  }
  interrupts();
  if (pendingMotorCommand && (millis() - lastMotorCommandWrite >= MOTOR_COMMAND_UPDATE_INTERVAL_MS)) {
    bool allowMotorOutput = imuWorking && !safetyCutoffActive && motorOutputEnabled;
    if (allowMotorOutput) {
      sendMotorCurrents(pendingLeftCurrent, pendingRightCurrent);
      forcedZeroSent = false;
    } else if (!forcedZeroSent) {
      sendMotorCurrents(0.0f, 0.0f);
      forcedZeroSent = true;
    }
    lastMotorCommandWrite = millis();
    noInterrupts();
    motorCommandPending = false;
    interrupts();
  }
  
  // Print status
  if (streamData && (millis() - lastPrint >= 50)) {
    lastPrint = millis();
    
    if (imuWorking) {
      float angleError = roll - angleSetpoint;
      float yawError = yaw - yawSetpoint;
      const char* modeStr = (controlMode == MODE_DIAGNOSTIC) ? "DIAG" : "PID";
      // Log: Roll, Pitch, Yaw, AngleInput (EMA-filtered roll fed to balancePID), RollError, YawError, Vel (filtered), RawVel (avg before filter), VelSetpt, VelPID_Out, RollPID_Out, YawPID_Out, LeftMotor, RightMotor, Setpoint, Mode, YawCtrl, Logging, GyroRate
      // AI: added in 4-8 so filter-vs-raw divergence is visible without a reflash.
      Serial.printf("R:%.2f,P:%.2f,Y:%.2f,AI:%.2f,Err:%.2f,YawErr:%.2f,Vel:%.3f,RawVel:%.3f,VelSet:%.3f,VelPID:%.3f,RollOut:%.2f,YawOut:%.2f,Left:%.2f,Right:%.2f,Setpt:%.2f,Mode:%s,Yaw:%s,Log:%s,GyroRate:%.4f\n",
                   roll, pitch, yaw, angleInput, angleError, yawError, filteredVelocity, avgVelocity, velocitySetpoint, angleSetpointFromVel,
                   motorCurrent, yawOutput, leftMotorCurrent, rightMotorCurrent, angleSetpoint,
                   modeStr, yawControlEnabled ? "ON" : "OFF", loggingEnabled ? "ON" : "OFF", gyroPitchRate);

      // Effective gains line once per second so every captured log is self-documenting.
      static unsigned long lastGainsPrint = 0;
      if (millis() - lastGainsPrint >= 1000) {
        lastGainsPrint = millis();
        Serial.printf("GAINS:Kp=%.2f,Ki=%.2f,Kd=%.3f,Setpt=%.2f,MaxI=%.1f,Filt=%.2f,VelLoop=%d,Dither=%.2f@%.0fHz\n",
                      Kp, Ki, Kd, baseSetpoint, maxCurrent, angleFilterAlpha,
                      useVelocityLoop ? 1 : 0, DITHER_AMPLITUDE, DITHER_FREQ_HZ);
      }
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
          rearmVelocityPID();
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
        Serial.println("Motor Output: DISABLED");
      } else {
        Serial.println("Motor Output: ENABLED");
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

    case 'n':
    case 'N':
      if (cmd == 'n') {
        velScale -= (fineAdjust ? VEL_SCALE_STEP_FINE : VEL_SCALE_STEP_COARSE);
      } else {
        velScale += (fineAdjust ? VEL_SCALE_STEP_FINE : VEL_SCALE_STEP_COARSE);
      }
      if (velScale < 0.1f) velScale = 0.1f;
      if (velScale > 5.0f) velScale = 5.0f;
      Serial.printf("VEL_SCALE = %.2f (%s)\n", velScale, (cmd == 'n') ? "decreased" : "increased");
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
      Serial.printf("Angle Kd = %.3f (decreased)\n", Kd);
      break;
      
    case 'D':
      Kd += (fineAdjust ? KD_STEP_FINE : KD_STEP_COARSE);
      balancePID.SetTunings(Kp, Ki, Kd);
      Serial.printf("Angle Kd = %.3f (increased)\n", Kd);
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
      Serial.printf("Yaw Kd = %.3f (decreased)\n", Kd_yaw);
      break;
      
    case 'H':
      if (isnan(Kd_yaw) || isinf(Kd_yaw)) Kd_yaw = 0.1;
      Kd_yaw += (fineAdjust ? KD_STEP_FINE : KD_STEP_COARSE);
      if (isnan(Kp_yaw) || isinf(Kp_yaw)) Kp_yaw = 0.5;
      if (isnan(Ki_yaw) || isinf(Ki_yaw)) Ki_yaw = 0.0;
      yawPID.SetTunings(Kp_yaw, Ki_yaw, Kd_yaw);
      Serial.printf("Yaw Kd = %.3f (increased)\n", Kd_yaw);
      break;
    
    case 'f':
    case 'F':
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
  Serial.printf("  Velocity Setpoint: %.3f m/s  IMU Velocity: %.3f m/s  VESC Filt: %.3f m/s  VESC Raw: %.3f m/s\n",
                velocitySetpoint, imuVelocity, filteredVelocity, avgVelocity);
  Serial.printf("  Velocity PID Output: %.3f° (angle offset, clamped to ±%.1f°, slew limited)\n", angleSetpointFromVel, VELOCITY_OUTPUT_MAX);
  Serial.printf("  Velocity Deadband: ±%.3f m/s (IMU signal)  Filter alpha: %.2f  VEL_SCALE: %.2f\n",
                VELOCITY_DEADBAND, VELOCITY_FILTER_ALPHA, velScale);
  Serial.println("ANGLE PID CONTROL (Inner Loop - Balance):");
  Serial.printf("  Angle Kp: %.2f  Angle Ki: %.2f  Angle Kd: %.3f\n", Kp, Ki, Kd);
  Serial.printf("  Angle Filter Alpha: %.2f (~%.0f Hz cutoff)\n", angleFilterAlpha, -logf(1.0f - angleFilterAlpha) / (2.0f * PI * (PID_SAMPLE_TIME_MS / 1000.0f)));
  Serial.printf("  Base Angle Setpoint: %.2f°\n", baseSetpoint);
  Serial.printf("  Active Setpoint: %.2f° (base + velocity offset)\n", angleSetpoint);
  Serial.println("YAW PID CONTROL (Rotation):");
  Serial.printf("  Kp_yaw: %.2f  Ki_yaw: %.2f  Kd_yaw: %.3f\n", Kp_yaw, Ki_yaw, Kd_yaw);
  Serial.printf("  Yaw Setpoint: %.2f°  Yaw Control: %s\n", yawSetpoint, yawControlEnabled ? "ENABLED" : "DISABLED");
  Serial.println("MOTOR CONTROL:");
  Serial.printf("  Max Current: %.2fA\n", maxCurrent);
  Serial.printf("  Dither: ±%.2fA @ %.0f Hz (anti-stiction, zero-mean)\n", DITHER_AMPLITUDE, DITHER_FREQ_HZ);
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
  Serial.printf("  Angle Kd: %.3f\n", Kd);
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
    angleFilterAlpha = 0.25f;
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
