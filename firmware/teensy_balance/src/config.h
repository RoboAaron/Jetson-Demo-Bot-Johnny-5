#pragma once

// =============================================================
// config.h — All tunable parameters and hardware pin definitions
// Jetson Demo Bot (Johnny-5) — Teensy 4.1 Balance Controller
//
// HARDWARE VALIDATION REQUIRED before using these values:
//   - Verify IMU mounting orientation (pitch sign convention)
//   - Verify motor direction (which CAN ID is left/right; + = forward?)
//   - Tune PID gains on physical hardware — these are starting points only
//   - Measure actual balance point offset (PITCH_TRIM_DEG)
// =============================================================

// -------------------------------------------------------------
// Pin Definitions — Teensy 4.1
// -------------------------------------------------------------

// I2C — BNO085 IMU
// Teensy 4.1 default I2C1 bus
#define IMU_I2C_SDA     18   // SDA1
#define IMU_I2C_SCL     19   // SCL1
#define IMU_I2C_ADDR    0x4A // BNO085 default address (SA0 = GND)
                             // Use 0x4B if SA0 = VCC

// CAN — FSESC via VESC protocol
// Teensy 4.1 CAN1 (FlexCAN_T4 CAN1)
// Physical pins: CAN1_TX = pin 22, CAN1_RX = pin 23
// (These are fixed by the Teensy 4.1 hardware — no #define needed for FlexCAN_T4)

// Optional: hardware LED for status indication
#define LED_PIN         13   // Teensy built-in LED


// -------------------------------------------------------------
// VESC CAN Configuration
// See FIRMWARE_DESIGN.md §3 for protocol details.
// -------------------------------------------------------------

#define VESC_CAN_BAUD       500000  // 500 kbps — matches setup_fsesc_config.md

// CAN IDs assigned in VESC Tool during FSESC setup
// TODO: verify left/right assignment against physical robot wiring
#define VESC_LEFT_ID        1
#define VESC_RIGHT_ID       2

// How often to expect a STATUS frame from the VESC (set in VESC Tool)
// If no frame received within this window, isAlive() returns false
#define VESC_HEARTBEAT_TIMEOUT_MS  500


// -------------------------------------------------------------
// IMU Configuration
// -------------------------------------------------------------

// BNO085 report rate for rotation vector (ARVR-stabilized quaternion)
// Max practical rate over I2C is ~200 Hz; increase to 400 Hz if using SPI
#define IMU_REPORT_RATE_US  5000    // 5000 µs = 200 Hz

// Sign conventions — HARDWARE VALIDATION REQUIRED
// After mounting, verify:
//   getPitch() > 0  when robot leans FORWARD
//   getPitch() < 0  when robot leans BACKWARD
// Flip PITCH_INVERT if the sign is wrong.
#define PITCH_INVERT        false   // Set true to flip pitch sign


// -------------------------------------------------------------
// Balance Controller
// -------------------------------------------------------------

#define BALANCE_LOOP_HZ     200     // Control loop target frequency

// Balance setpoint: the pitch angle (degrees) at which the robot is upright.
// 0.0 = perfectly vertical.  Adjust to compensate for CoG offset.
// TODO: measure on physical robot
#define PITCH_TRIM_DEG      0.0f

// PID gains — PLACEHOLDER VALUES — must be tuned on hardware.
// Tuning procedure:
//   1. Start with Kp only (Ki = Kd = 0). Increase until oscillation, back off 50%.
//   2. Add Kd to damp oscillation.
//   3. Add small Ki to eliminate steady-state lean.
// See FIRMWARE_DESIGN.md §4 for approach.
#define PID_KP              5.0f
#define PID_KI              0.5f
#define PID_KD              0.1f

// Output limits in Amps.  FSESC6.7 Pro is rated 50 A; start conservative.
#define MAX_CURRENT_A       20.0f
#define MIN_CURRENT_A       (-MAX_CURRENT_A)

// Safety: disable motors if pitch exceeds this threshold
#define FALL_THRESHOLD_DEG  45.0f

// Integral anti-windup clamp (±Amps equivalent)
#define INTEGRAL_CLAMP_A    10.0f


// -------------------------------------------------------------
// Serial Debug
// -------------------------------------------------------------

#define DEBUG_BAUD          115200
#define DEBUG_PRINT_HZ      20      // How often to print telemetry (Hz)
