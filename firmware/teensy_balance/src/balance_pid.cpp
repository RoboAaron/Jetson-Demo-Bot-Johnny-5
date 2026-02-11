#include "balance_pid.h"
#include "config.h"
#include <math.h>

// =============================================================
// balance_pid.cpp — PID implementation
// =============================================================

BalancePID::BalancePID(float kp, float ki, float kd, float out_min, float out_max)
    : _kp(kp), _ki(ki), _kd(kd), _out_min(out_min), _out_max(out_max) {}

void BalancePID::setGains(float kp, float ki, float kd) {
    _kp = kp;
    _ki = ki;
    _kd = kd;
}

void BalancePID::setSetpoint(float setpoint_deg) {
    _setpoint = setpoint_deg;
}

void BalancePID::setLimits(float out_min, float out_max) {
    _out_min = out_min;
    _out_max = out_max;
}

void BalancePID::reset() {
    _integral   = 0.0f;
    _prev_meas  = 0.0f;
    _first_call = true;
    _error  = 0.0f;
    _p_term = 0.0f;
    _i_term = 0.0f;
    _d_term = 0.0f;
    _output = 0.0f;
}

float BalancePID::compute(float measurement, float dt_s) {
    if (dt_s <= 0.0f) return _output;

    _error = _setpoint - measurement;

    // --- Proportional ---
    _p_term = _kp * _error;

    // --- Integral with anti-windup clamp ---
    _integral += _error * dt_s;
    // Clamp integral to prevent windup (see INTEGRAL_CLAMP_A in config.h)
    float iclamp = INTEGRAL_CLAMP_A;
    if (_ki > 1e-6f) iclamp = INTEGRAL_CLAMP_A / _ki;
    _integral = constrain(_integral, -iclamp, iclamp);
    _i_term = _ki * _integral;

    // --- Derivative on measurement (avoids derivative kick on setpoint change) ---
    if (_first_call) {
        _d_term    = 0.0f;
        _prev_meas = measurement;
        _first_call = false;
    } else {
        float dmeas = (measurement - _prev_meas) / dt_s;
        _d_term = -_kd * dmeas;   // Negative because D-on-measurement
    }
    _prev_meas = measurement;

    // --- Sum and clamp ---
    _output = constrain(_p_term + _i_term + _d_term, _out_min, _out_max);

    return _output;
}
