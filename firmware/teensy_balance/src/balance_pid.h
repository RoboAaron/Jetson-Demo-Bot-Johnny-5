#pragma once

// =============================================================
// balance_pid.h — PID controller for balance loop
// Jetson Demo Bot (Johnny-5) — Teensy 4.1 Balance Controller
//
// Standard discrete PID with:
//   - Anti-windup integral clamp
//   - Derivative on measurement (not error) to avoid derivative kick
//   - Output clamping
// =============================================================

#include <Arduino.h>

class BalancePID {
public:
    // Construct with gains and output limits.
    // These can be changed at runtime via setGains() / setLimits().
    BalancePID(float kp, float ki, float kd, float out_min, float out_max);

    // Update gains at runtime (e.g., via serial CLI during tuning)
    void setGains(float kp, float ki, float kd);

    // Update the balance setpoint (target pitch angle in degrees)
    void setSetpoint(float setpoint_deg);

    // Update output limits (Amps)
    void setLimits(float out_min, float out_max);

    // Reset integrator (call when enabling balance after a pause)
    void reset();

    // --------------------------------------------------------
    // Compute one PID step.
    //   measurement: current pitch angle (degrees)
    //   dt_s:        time since last call (seconds)
    // Returns: current command in Amps (clamped to [out_min, out_max])
    // --------------------------------------------------------
    float compute(float measurement, float dt_s);

    // --------------------------------------------------------
    // Diagnostics — read individual terms (for serial debug)
    // --------------------------------------------------------
    float getKp()       const { return _kp; }
    float getKi()       const { return _ki; }
    float getKd()       const { return _kd; }
    float getSetpoint() const { return _setpoint; }
    float getError()    const { return _error; }
    float getPTerm()    const { return _p_term; }
    float getITerm()    const { return _i_term; }
    float getDTerm()    const { return _d_term; }
    float getOutput()   const { return _output; }

private:
    float _kp, _ki, _kd;
    float _setpoint = 0.0f;
    float _out_min, _out_max;

    float _integral     = 0.0f;
    float _prev_meas    = 0.0f;  // Previous measurement (for D-on-measurement)
    bool  _first_call   = true;

    // Last computed terms (for diagnostics)
    float _error  = 0.0f;
    float _p_term = 0.0f;
    float _i_term = 0.0f;
    float _d_term = 0.0f;
    float _output = 0.0f;
};
