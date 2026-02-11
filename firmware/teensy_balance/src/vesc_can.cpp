#include "vesc_can.h"

// =============================================================
// vesc_can.cpp — VESC CAN protocol implementation
//
// VESC protocol reference: https://github.com/vedderb/bldc
// CAN frame format: extended 29-bit ID
//   id = (COMM_PACKET_ID << 8) | VESC_CAN_ID
//   data = big-endian payload
// =============================================================

void VescCan::begin() {
    _can.begin();
    _can.setBaudRate(VESC_CAN_BAUD);

    // Accept all extended frames (VESC uses extended 29-bit IDs)
    _can.setMaxMB(16);
    _can.enableFIFO();
    _can.enableFIFOInterrupt(false);  // Polling mode (called from update())

    Serial.print("[VESC] CAN1 initialized @ ");
    Serial.print(VESC_CAN_BAUD / 1000);
    Serial.println(" kbps");
    Serial.print("[VESC] Left motor CAN ID:  "); Serial.println(VESC_LEFT_ID);
    Serial.print("[VESC] Right motor CAN ID: "); Serial.println(VESC_RIGHT_ID);
}

void VescCan::update() {
    CAN_message_t msg;
    // Drain all queued frames
    while (_can.read(msg)) {
        _parseFrame(msg);
    }
}

// ------------------------------------------------------------
// Motor commands
// ------------------------------------------------------------

void VescCan::setCurrentBoth(float current_a) {
    setCurrent(VESC_LEFT_ID,  current_a);
    setCurrent(VESC_RIGHT_ID, current_a);
}

void VescCan::setCurrent(uint8_t vesc_id, float current_a) {
    // VESC expects current in milliamps as a signed 32-bit integer
    int32_t ma = (int32_t)(current_a * 1000.0f);
    _sendInt32(vesc_id, CAN_PACKET_SET_CURRENT, ma);
}

void VescCan::setBrakeBoth(float brake_a) {
    int32_t ma = (int32_t)(fabsf(brake_a) * 1000.0f);
    _sendInt32(VESC_LEFT_ID,  CAN_PACKET_SET_CURRENT_BRAKE, ma);
    _sendInt32(VESC_RIGHT_ID, CAN_PACKET_SET_CURRENT_BRAKE, ma);
}

void VescCan::coast() {
    setCurrent(VESC_LEFT_ID,  0.0f);
    setCurrent(VESC_RIGHT_ID, 0.0f);
}

// ------------------------------------------------------------
// Liveness checks
// ------------------------------------------------------------

bool VescCan::isAlive(uint32_t timeout_ms) const {
    return isAlive(VESC_LEFT_ID, timeout_ms) && isAlive(VESC_RIGHT_ID, timeout_ms);
}

bool VescCan::isAlive(uint8_t vesc_id, uint32_t timeout_ms) const {
    const VescStatus& s = (vesc_id == VESC_LEFT_ID) ? _left : _right;
    return (millis() - s.last_rx_ms) < timeout_ms;
}

// ------------------------------------------------------------
// Private: Send a 4-byte big-endian int32 command
// ------------------------------------------------------------
void VescCan::_sendInt32(uint8_t vesc_id, VescPacketId cmd, int32_t value) {
    CAN_message_t msg;
    msg.flags.extended = 1;                     // VESC uses extended 29-bit IDs
    msg.id = ((uint32_t)cmd << 8) | vesc_id;
    msg.len = 4;

    // Big-endian encoding
    msg.buf[0] = (uint8_t)((value >> 24) & 0xFF);
    msg.buf[1] = (uint8_t)((value >> 16) & 0xFF);
    msg.buf[2] = (uint8_t)((value >>  8) & 0xFF);
    msg.buf[3] = (uint8_t)((value      ) & 0xFF);

    _can.write(msg);
}

// ------------------------------------------------------------
// Private: Parse incoming CAN frame from VESC
//
// STATUS (0x09):  8 bytes
//   [0-3] RPM       int32_t (raw)
//   [4-5] Current   int16_t / 10  → Amps
//   [6-7] Duty      int16_t / 1000 → fraction (0.0–1.0)
//
// STATUS_4 (0x10): 8 bytes
//   [0-1] temp_fet   int16_t / 10 → °C
//   [2-3] temp_motor int16_t / 10 → °C
//   [4-5] current_in int16_t / 10 → Amps
//   [6-7] PID pos    int16_t (unused for balance)
// ------------------------------------------------------------
void VescCan::_parseFrame(const CAN_message_t& msg) {
    if (!msg.flags.extended) return;    // VESC always uses extended IDs

    uint8_t  packet_id = (msg.id >> 8) & 0xFF;
    uint8_t  vesc_id   = msg.id & 0xFF;

    // Route to the correct status struct
    VescStatus* s = nullptr;
    if      (vesc_id == VESC_LEFT_ID)  s = &_left;
    else if (vesc_id == VESC_RIGHT_ID) s = &_right;
    else return;  // Unknown VESC ID — ignore

    switch (packet_id) {
        case CAN_PACKET_STATUS: {
            if (msg.len < 8) return;
            int32_t rpm_raw = ((int32_t)msg.buf[0] << 24)
                            | ((int32_t)msg.buf[1] << 16)
                            | ((int32_t)msg.buf[2] <<  8)
                            | ((int32_t)msg.buf[3]);
            int16_t cur_raw = ((int16_t)msg.buf[4] << 8)
                            | ((int16_t)msg.buf[5]);
            int16_t dut_raw = ((int16_t)msg.buf[6] << 8)
                            | ((int16_t)msg.buf[7]);

            s->rpm       = (float)rpm_raw;
            s->current_a = (float)cur_raw / 10.0f;
            s->duty      = (float)dut_raw / 1000.0f;
            s->last_rx_ms = millis();
            break;
        }

        case CAN_PACKET_STATUS_4: {
            if (msg.len < 6) return;
            int16_t tfet_raw = ((int16_t)msg.buf[0] << 8) | msg.buf[1];
            int16_t tmot_raw = ((int16_t)msg.buf[2] << 8) | msg.buf[3];
            int16_t iin_raw  = ((int16_t)msg.buf[4] << 8) | msg.buf[5];

            s->temp_fet_c = (float)tfet_raw / 10.0f;
            s->temp_mot_c = (float)tmot_raw / 10.0f;
            s->input_a    = (float)iin_raw  / 10.0f;
            break;
        }

        default:
            break;  // Other STATUS frames not needed for Phase 2
    }
}
