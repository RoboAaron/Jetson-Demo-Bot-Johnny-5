#pragma once

// =============================================================
// imu_bno085.h — BNO085 IMU wrapper
// Jetson Demo Bot (Johnny-5) — Teensy 4.1 Balance Controller
//
// Uses SparkFun BNO08x Arduino Library (NOT Adafruit_BNO055).
// BNO085 returns a fused ARVR-stabilized rotation vector as a
// quaternion; this class converts it to Euler angles.
//
// See FIRMWARE_DESIGN.md §2 for the BNO085 vs BNO055 decision.
// =============================================================

#include <Arduino.h>
#include <Wire.h>
#include <SparkFun_BNO08x_Arduino_Library.h>
#include "config.h"

class IMU_BNO085 {
public:
    // --------------------------------------------------------
    // Initialise I2C and start the BNO085 rotation vector report.
    // Returns true on success.  Call once in setup().
    // --------------------------------------------------------
    bool begin();

    // --------------------------------------------------------
    // Poll the BNO085 for new data.
    // Returns true when a fresh rotation vector report is ready.
    // Call every loop iteration; integrate into the main loop as:
    //   if (!imu.update()) return;   // skip if no new data
    // --------------------------------------------------------
    bool update();

    // --------------------------------------------------------
    // Euler angles derived from the ARVR-stabilized quaternion.
    // All values in degrees.
    //
    // Coordinate convention (IMU mounted flat, connector toward rear):
    //   pitch > 0 → robot leans forward
    //   roll  > 0 → robot leans right
    //   yaw   = heading (0 = North / initial orientation)
    //
    // TODO: HARDWARE VALIDATION — verify sign convention after mounting.
    //       If pitch sign is wrong, set PITCH_INVERT = true in config.h.
    // --------------------------------------------------------
    float getPitch() const;
    float getRoll()  const;
    float getYaw()   const;

    // Raw quaternion components (use if you need them directly)
    float getQr() const { return _qr; }
    float getQi() const { return _qi; }
    float getQj() const { return _qj; }
    float getQk() const { return _qk; }

    // True if begin() succeeded and data is flowing
    bool isOk() const { return _ok; }

    // Milliseconds since last successful update()
    uint32_t msSinceLastUpdate() const;

private:
    BNO08x  _imu;
    bool    _ok = false;

    // Last quaternion from rotation vector report
    float _qr = 1.0f, _qi = 0.0f, _qj = 0.0f, _qk = 0.0f;

    // Derived Euler angles (degrees)
    float _pitch = 0.0f;
    float _roll  = 0.0f;
    float _yaw   = 0.0f;

    uint32_t _lastUpdateMs = 0;

    // Convert quaternion (qr, qi, qj, qk) to Euler angles and store them.
    // Convention: ZYX (yaw-pitch-roll), aerospace-style.
    void _updateEuler();
};
