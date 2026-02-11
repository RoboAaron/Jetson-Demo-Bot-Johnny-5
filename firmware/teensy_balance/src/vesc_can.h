#pragma once

// =============================================================
// vesc_can.h — VESC open CAN protocol interface
// Jetson Demo Bot (Johnny-5) — Teensy 4.1 Balance Controller
//
// Implements the minimal VESC CAN subset needed for:
//   - Sending current commands to both motors (SET_CURRENT)
//   - Receiving motor telemetry (STATUS: RPM, current, duty)
//
// Uses FlexCAN_T4 for Teensy 4.1's built-in CAN1 controller.
// See FIRMWARE_DESIGN.md §3 for protocol decisions.
// =============================================================

#include <Arduino.h>
#include <FlexCAN_T4.h>
#include "config.h"

// ------------------------------------------------------------
// VESC CAN packet IDs (VESC open source protocol)
// CAN frame extended ID = (COMM_PACKET_ID << 8) | VESC_CAN_ID
// ------------------------------------------------------------
enum VescPacketId : uint8_t {
    CAN_PACKET_SET_DUTY             = 0x00,
    CAN_PACKET_SET_CURRENT          = 0x01,  // Used for balance commands
    CAN_PACKET_SET_CURRENT_BRAKE    = 0x02,  // Active braking
    CAN_PACKET_SET_RPM              = 0x03,
    CAN_PACKET_SET_POS              = 0x04,
    CAN_PACKET_STATUS               = 0x09,  // RPM, current, duty  (VESC→us)
    CAN_PACKET_STATUS_2             = 0x0E,  // Ah used, Ah charged (VESC→us)
    CAN_PACKET_STATUS_4             = 0x10,  // Temps, input current (VESC→us)
};

// Telemetry received from a single VESC via CAN STATUS frames
struct VescStatus {
    float    rpm         = 0.0f;
    float    current_a   = 0.0f;   // Motor phase current (A)
    float    duty        = 0.0f;   // Duty cycle (0.0–1.0)
    float    input_a     = 0.0f;   // Battery input current (A)
    float    temp_fet_c  = 0.0f;   // FET temperature (°C)
    float    temp_mot_c  = 0.0f;   // Motor temperature (°C)
    uint32_t last_rx_ms  = 0;      // millis() of last received STATUS frame
};

class VescCan {
public:
    // --------------------------------------------------------
    // Initialise CAN1 at VESC_CAN_BAUD (500 kbps).
    // Call once in setup().
    // --------------------------------------------------------
    void begin();

    // --------------------------------------------------------
    // Process all pending incoming CAN frames (non-blocking).
    // Populates leftStatus() / rightStatus() from VESC STATUS frames.
    // Call every loop iteration.
    // --------------------------------------------------------
    void update();

    // --------------------------------------------------------
    // Motor commands — current in Amps.
    // Positive = forward (verify direction on hardware).
    // --------------------------------------------------------

    // Send the same current to both motors (balance command)
    void setCurrentBoth(float current_a);

    // Send current to a specific VESC by CAN ID
    void setCurrent(uint8_t vesc_id, float current_a);

    // Active brake: apply braking current to both motors
    void setBrakeBoth(float brake_a);

    // Coast: set 0 current to both motors
    void coast();

    // --------------------------------------------------------
    // Telemetry accessors
    // --------------------------------------------------------
    const VescStatus& leftStatus()  const { return _left; }
    const VescStatus& rightStatus() const { return _right; }

    // Returns true if both VESCs sent a STATUS frame recently
    bool isAlive(uint32_t timeout_ms = VESC_HEARTBEAT_TIMEOUT_MS) const;

    // Returns true if a specific VESC is alive
    bool isAlive(uint8_t vesc_id, uint32_t timeout_ms = VESC_HEARTBEAT_TIMEOUT_MS) const;

private:
    // FlexCAN_T4 instance — CAN1 on Teensy 4.1 (pins 22/23)
    FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> _can;

    VescStatus _left;   // CAN ID = VESC_LEFT_ID
    VescStatus _right;  // CAN ID = VESC_RIGHT_ID

    // Send a 4-byte int32 command (big-endian, scaled)
    void _sendInt32(uint8_t vesc_id, VescPacketId cmd, int32_t value);

    // Parse a received STATUS or STATUS_4 frame into the appropriate VescStatus
    void _parseFrame(const CAN_message_t& msg);
};
