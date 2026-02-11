#include "imu_bno085.h"
#include <math.h>

// =============================================================
// imu_bno085.cpp — BNO085 IMU implementation
// =============================================================

bool IMU_BNO085::begin() {
    Wire.begin();

    if (!_imu.begin(IMU_I2C_ADDR, Wire)) {
        Serial.println("[IMU] ERROR: BNO085 not found at address 0x"
                       + String(IMU_I2C_ADDR, HEX));
        Serial.println("[IMU]   Check: I2C wiring, pull-up resistors, SA0 pin level");
        _ok = false;
        return false;
    }

    // Request ARVR-stabilized rotation vector report.
    // This is the highest-quality fused orientation output from the BNO085.
    // Report rate: IMU_REPORT_RATE_US microseconds (default 5000 = 200 Hz).
    if (!_imu.enableRotationVector(IMU_REPORT_RATE_US)) {
        Serial.println("[IMU] ERROR: Could not enable rotation vector report");
        _ok = false;
        return false;
    }

    _ok = true;
    _lastUpdateMs = millis();
    Serial.print("[IMU] BNO085 OK @ I2C 0x");
    Serial.print(IMU_I2C_ADDR, HEX);
    Serial.print(" — rotation vector @ ");
    Serial.print(1000000UL / IMU_REPORT_RATE_US);
    Serial.println(" Hz");
    return true;
}

bool IMU_BNO085::update() {
    if (!_ok) return false;

    if (_imu.wasReset()) {
        Serial.println("[IMU] BNO085 reset detected — re-enabling rotation vector");
        _imu.enableRotationVector(IMU_REPORT_RATE_US);
    }

    if (!_imu.getSensorEvent()) return false;

    // Only process rotation vector reports (SENSOR_REPORTID_ROTATION_VECTOR)
    if (_imu.getSensorEventID() != SENSOR_REPORTID_ROTATION_VECTOR) return false;

    _qr = _imu.getQuatReal();
    _qi = _imu.getQuatI();
    _qj = _imu.getQuatJ();
    _qk = _imu.getQuatK();

    _updateEuler();
    _lastUpdateMs = millis();
    return true;
}

float IMU_BNO085::getPitch() const {
    float p = _pitch;
#if PITCH_INVERT
    p = -p;
#endif
    return p;
}

float IMU_BNO085::getRoll()  const { return _roll; }
float IMU_BNO085::getYaw()   const { return _yaw; }

uint32_t IMU_BNO085::msSinceLastUpdate() const {
    return millis() - _lastUpdateMs;
}

// ------------------------------------------------------------
// Quaternion → Euler (ZYX / yaw-pitch-roll convention)
//
// For a self-balancing robot mounted with:
//   Z-axis pointing up
//   X-axis pointing forward
//   Y-axis pointing left
//
// Pitch = rotation about Y-axis (forward/back lean)
// Roll  = rotation about X-axis (left/right lean)
// Yaw   = rotation about Z-axis (heading)
//
// TODO: HARDWARE VALIDATION — verify these match the physical mount.
// ------------------------------------------------------------
void IMU_BNO085::_updateEuler() {
    // Normalise quaternion
    float norm = sqrtf(_qr*_qr + _qi*_qi + _qj*_qj + _qk*_qk);
    if (norm < 1e-6f) return;
    float r = _qr / norm;
    float i = _qi / norm;
    float j = _qj / norm;
    float k = _qk / norm;

    // ZYX Euler angles
    // Roll (rotation about X)
    float sinr_cosp = 2.0f * (r*i + j*k);
    float cosr_cosp = 1.0f - 2.0f * (i*i + j*j);
    _roll = atan2f(sinr_cosp, cosr_cosp) * (180.0f / M_PI);

    // Pitch (rotation about Y)
    float sinp = 2.0f * (r*j - k*i);
    if (fabsf(sinp) >= 1.0f)
        _pitch = copysignf(90.0f, sinp);    // clamp at ±90°
    else
        _pitch = asinf(sinp) * (180.0f / M_PI);

    // Yaw (rotation about Z)
    float siny_cosp = 2.0f * (r*k + i*j);
    float cosy_cosp = 1.0f - 2.0f * (j*j + k*k);
    _yaw = atan2f(siny_cosp, cosy_cosp) * (180.0f / M_PI);
}
