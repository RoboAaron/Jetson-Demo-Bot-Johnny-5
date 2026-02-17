# Software Design Document
## Jetson-Demo-Bot-Johnny-5 Self-Balancing Robot

**Version**: 1.0  
**Date**: January 2025  
**Status**: Balance Control Implemented, Control Elements Pending  

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [System Architecture](#system-architecture)
3. [Balance Control Implementation](#balance-control-implementation)
4. [Current Balance Parameters](#current-balance-parameters)
5. [Control Elements Design](#control-elements-design)
6. [Communication Protocols](#communication-protocols)
7. [Safety Systems](#safety-systems)
8. [Development Methodology](#development-methodology)
9. [Testing Strategy](#testing-strategy)
10. [Future Enhancements](#future-enhancements)

---

## Executive Summary

This document outlines the software design for a **carry-on sized, two-wheeled self-balancing robot** capable of ~8 mph operation. The system implements a **cascaded PID control architecture** for balance control, with planned integration of higher-level autonomy through NVIDIA Jetson AGX Orin Dev Kit.

### Key Achievements
- ✅ **Balance Control**: Implemented cascaded PID system with 100Hz IMU updates
- ✅ **Motor Control**: VESC-based current control with encoder feedback
- ✅ **Data Logging**: Real-time telemetry and tuning capabilities
- ✅ **Live Tuning**: Runtime parameter adjustment via serial commands

### Current Status
- **Balance**: Functional with chattering (tuning in progress)
- **Control Elements**: Design phase (PS3 controller, Jetson interface)
- **Autonomy**: Planned (8 core demos defined)

---

## System Architecture

### Hardware Stack
```
┌─────────────────────────────────────────┐
│           Jetson AGX Orin Dev Kit       │  ← High-level autonomy
│         (ROS 2, SLAM, Vision, AI)       │
└─────────────────┬───────────────────────┘
                  │ USB Serial (100Hz+)
┌─────────────────▼───────────────────────┐
│            Teensy 4.1                   │  ← Real-time control
│  ┌─────────────────────────────────────┐│
│  │ Cascaded PID Controller             ││
│  │ • Angle PID (Outer Loop)            ││
│  │ • Velocity PID (Inner Loop)        ││
│  │ • Position Feedback (Optional)     ││
│  └─────────────────────────────────────┘│
│  ┌─────────────────────────────────────┐│
│  │ Sensor Fusion                       ││
│  │ • BNO085 IMU (100Hz)                ││
│  │ • VESC Encoders (RPM feedback)     ││
│  └─────────────────────────────────────┘│
└─────────────────┬───────────────────────┘
                  │ UART (115200 baud)
┌─────────────────▼───────────────────────┐
│        Flipsky Dual Mini FSESC6.7       │  ← Motor control
│  ┌─────────────────┐ ┌─────────────────┐│
│  │ Left Motor      │ │ Right Motor     ││
│  │ (Gyroor 6.5")   │ │ (Gyroor 6.5")   ││
│  └─────────────────┘ └─────────────────┘│
└─────────────────────────────────────────┘
```

### Software Architecture
```
┌─────────────────────────────────────────┐
│              Application Layer          │
│  • ROS 2 Nodes (SLAM, Navigation)      │
│  • Computer Vision (DepthAI/OpenCV)    │
│  • AI/ML (ASR, LLM, Gesture Recognition)│
└─────────────────┬───────────────────────┘
                  │ ROS 2 Topics/Services
┌─────────────────▼───────────────────────┐
│            Communication Layer          │
│  • USB Serial Bridge                    │
│  • Protocol Translation                 │
│  • Command Processing                   │
└─────────────────┬───────────────────────┘
                  │ Custom Protocol
┌─────────────────▼───────────────────────┐
│            Control Layer                │
│  • Cascaded PID Controller              │
│  • Sensor Fusion                        │
│  • Safety Systems                       │
│  • Motor Control                        │
└─────────────────────────────────────────┘
```

---

## Balance Control Implementation

### Control Architecture

The balance control system implements a **cascaded PID architecture** based on literature review of successful self-balancing robots. This approach separates concerns and provides better stability than single-loop control.

#### Control Flow
```
IMU Roll Angle → Angle PID → Velocity Setpoint → Velocity PID → Motor Current
     ↑              ↑              ↑              ↑              ↑
  100Hz         Outer Loop      Inner Loop     Encoder      VESC Motors
  Updates       (Slow)          (Fast)        Feedback      (Current)
```

#### Why Cascaded Control?

**Single-Loop Problems** (Initial Implementation):
- Direct angle-to-current mapping caused oscillation
- High inertia motors required excessive gains
- Bang-bang control behavior
- Poor disturbance rejection

**Cascaded Benefits**:
- **Separation of Concerns**: Angle control (slow) vs Velocity control (fast)
- **Better Stability**: Inner loop handles motor dynamics
- **Encoder Feedback**: Real velocity measurement vs estimation
- **Smoother Control**: Proportional response vs on/off behavior

### Implementation Details

#### 1. Sensor Integration
```cpp
// IMU Configuration
BNO085 IMU at 0x4B (I2C, 100kHz)
- Rotation Vector: 100Hz (10ms intervals)
- Gyroscope: 100Hz (velocity damping)
- Orientation Correction: Mounted upside down
```

#### 2. Control Loops

**Outer Loop - Angle Control**:
```cpp
Input: Roll angle + velocity damping
Setpoint: Calibrated balance angle (1.1°)
Output: Velocity setpoint (-3.0 to +3.0 m/s)
Update Rate: 100Hz
```

**Inner Loop - Velocity Control**:
```cpp
Input: Average wheel velocity (m/s)
Setpoint: From angle PID
Output: Motor current (-8.0 to +8.0 A)
Update Rate: 100Hz
```

#### 3. Velocity Feedback
```cpp
// Encoder-based velocity measurement
Wheel Diameter: 0.165m (6.5 inches)
RPM to m/s: (WHEEL_DIAMETER * PI) / 60.0
Left/Right averaging for differential drive
```

#### 4. Safety Systems
```cpp
// Safety cutoffs
Angle Limit: ±25° (emergency stop)
Current Limit: ±8.0A (motor protection)
IMU Failure: Safe mode (motors off)
```

---

## Current Balance Parameters

### Active Configuration (Working but Chattery)

#### Angle PID (Outer Loop)
```cpp
Kp_angle = 15.0    // Proportional gain (high for responsiveness)
Ki_angle = 0.5     // Integral gain (low to prevent windup)
Kd_angle = 0.8     // Derivative gain (moderate damping)
angleSetpoint = 1.1°  // Calibrated balance point
```

#### Velocity PID (Inner Loop)
```cpp
Kp_vel = 0.8       // Proportional gain (causes chatter)
Ki_vel = 0.3       // Integral gain
Kd_vel = 0.0       // Derivative gain (disabled)
```

#### Motor Control Parameters
```cpp
maxCurrent = 8.0A      // Maximum motor current
minCurrentToMove = 0.2A  // Minimum current threshold
deadband = 0.5°       // Angle deadband for stability
velocityDamping = 0.01  // Roll rate damping factor
```

#### System Parameters
```cpp
IMU Update Rate: 100Hz
I2C Clock: 100kHz
Control Loop: 100Hz
Wheel Diameter: 0.165m
Position Feedback: DISABLED (Kp_position = 0.0)
```

### Tuning Strategy

**Current Issue**: Motor chattering at ~14Hz
**Root Cause**: Velocity PID too aggressive (Kp_vel = 0.8)

**Recommended Tuning Sequence**:
1. **Add Velocity Damping**: Press `B` 2-3 times (Kd_vel = 0.10-0.15)
2. **Reduce Velocity Kp**: Press `a` 1-2 times (Kp_vel = 0.6-0.7)
3. **Fine-tune Deadband**: Increase to 0.8° if needed
4. **Verify Angle Setpoint**: Ensure 1.1° is optimal

---

## Control Elements Design

### 1. PS3 Controller Integration

#### Hardware Interface
```cpp
USB Host Controller: USBHost_t36 library
Bluetooth Dongle: Generic USB Bluetooth adapter
Connection: USB OTG port on Teensy 4.1
```

#### Control Mapping
```cpp
// Primary Controls
Left Stick Y: Forward/Backward velocity setpoint
Right Stick X: Differential steering (left/right bias)
L2/R2: Emergency stop (both pressed)

// Secondary Controls
Triangle: Increase max current
Circle: Decrease max current
Square: Toggle logging
Cross: Reset position

// Safety Features
PS Button: Emergency stop
Disconnect Detection: Auto-stop motors
Deadband: ±0.1 on all axes
```

#### Implementation Plan
```cpp
// Control blending
Manual Mode: PS3 inputs override autonomy
Autonomous Mode: Jetson commands override PS3
Failsafe Mode: Emergency stop, return to balance
```

### 2. Jetson Interface

#### Communication Protocol
```cpp
// Upstream (Teensy → Jetson)
struct TelemetryPacket {
  uint32_t timestamp;
  float roll, pitch, yaw;
  float leftVelocity, rightVelocity;
  float leftCurrent, rightCurrent;
  bool balanceActive;
  bool motorsActive;
};

// Downstream (Jetson → Teensy)
struct CommandPacket {
  uint32_t timestamp;
  float velocitySetpoint;    // m/s
  float steeringBias;        // differential
  bool enableAutonomy;
  uint8_t safetyFlags;
};
```

#### Data Rates
```cpp
Telemetry: 100Hz (10ms intervals)
Commands: 50Hz (20ms intervals)
Packet Size: ~32 bytes each
Total Bandwidth: ~4.8 kbps
```

#### Protocol Features
```cpp
// Reliability
Checksums: CRC16 for packet integrity
Sequence Numbers: Detect dropped packets
Heartbeat: 1Hz keepalive
Timeout: 500ms command timeout

// Safety
Emergency Stop: Immediate motor cutoff
Command Validation: Range checking
Fallback: Return to balance mode
```

### 3. Autonomy Integration

#### ROS 2 Integration
```cpp
// Topics
/robot/telemetry: sensor_msgs/Imu + custom_msgs/MotorStatus
/robot/cmd_vel: geometry_msgs/Twist
/robot/emergency_stop: std_msgs/Bool

// Services
/robot/set_balance_mode: custom_srvs/BalanceMode
/robot/calibrate_imu: std_srvs/Empty
/robot/get_status: custom_srvs/RobotStatus
```

#### Control Modes
```cpp
enum ControlMode {
  BALANCE_ONLY,      // Just balance, no movement
  MANUAL_CONTROL,    // PS3 controller
  AUTONOMOUS,        // Jetson commands
  EMERGENCY_STOP     // All motors off
};
```

---

## Communication Protocols

### 1. USB Serial Protocol

#### Packet Structure
```cpp
// Header (4 bytes)
uint8_t sync1 = 0xAA;
uint8_t sync2 = 0x55;
uint8_t packetType;
uint8_t payloadLength;

// Payload (variable)
uint8_t data[payloadLength];

// Footer (2 bytes)
uint16_t checksum;  // CRC16
```

#### Packet Types
```cpp
TELEMETRY_PACKET = 0x01    // Sensor data upstream
COMMAND_PACKET = 0x02      // Control commands downstream
HEARTBEAT_PACKET = 0x03    // Keepalive
EMERGENCY_STOP = 0xFF      // Immediate stop
```

### 2. Error Handling

#### Timeout Management
```cpp
Command Timeout: 500ms
Heartbeat Timeout: 2 seconds
IMU Timeout: 100ms
VESC Timeout: 50ms
```

#### Fallback Behavior
```cpp
Jetson Disconnect: Switch to PS3 control
PS3 Disconnect: Switch to balance-only
IMU Failure: Emergency stop
VESC Failure: Stop affected motor
```

---

## Safety Systems

### 1. Hardware Safety

#### Power Management
```cpp
Main Cutoff: Physical switch
Battery Protection: Individual cell monitoring
Current Limiting: Hardware + software limits
Anti-Spark: XT90-S connectors
```

#### Motor Protection
```cpp
Current Limits: ±8.0A software, ±140A hardware
Temperature Monitoring: VESC thermal protection
Overvoltage: Battery BMS protection
Undervoltage: Low battery cutoff
```

### 2. Software Safety

#### Balance Safety
```cpp
Angle Limits: ±25° emergency stop
Velocity Limits: ±3.0 m/s maximum
Acceleration Limits: ±2.0 m/s² maximum
IMU Validation: Data range checking
```

#### Communication Safety
```cpp
Command Validation: Range and type checking
Timeout Handling: Automatic fallback
Checksum Verification: Packet integrity
Sequence Validation: Detect dropped packets
```

#### Emergency Procedures
```cpp
Emergency Stop: Immediate motor cutoff
Safe Mode: Balance-only operation
Recovery Mode: Gradual power restoration
Diagnostic Mode: System health checking
```

---

## Development Methodology

### 1. Iterative Development

#### Phase 1: Balance Control ✅
- [x] IMU integration and calibration
- [x] Basic PID implementation
- [x] Cascaded control architecture
- [x] Motor control integration
- [x] Data logging and analysis
- [x] Live tuning capabilities

#### Phase 2: Control Elements (Current)
- [ ] PS3 controller integration
- [ ] USB serial protocol
- [ ] Jetson interface
- [ ] Control mode switching
- [ ] Safety system integration

#### Phase 3: Autonomy (Future)
- [ ] ROS 2 integration
- [ ] Navigation stack
- [ ] Computer vision
- [ ] AI/ML integration
- [ ] 8 core demos

### 2. Testing Strategy

#### Unit Testing
```cpp
// Individual component testing
IMU Calibration: Static and dynamic testing
PID Tuning: Step response analysis
Motor Control: Current and velocity validation
Communication: Packet integrity testing
```

#### Integration Testing
```cpp
// System-level testing
Balance Stability: Disturbance rejection
Control Response: Step and ramp inputs
Safety Systems: Failure mode testing
Communication: End-to-end validation
```

#### Field Testing
```cpp
// Real-world validation
Balance Performance: Various surfaces
Control Responsiveness: User experience
Safety Validation: Emergency scenarios
Durability Testing: Extended operation
```

### 3. Documentation Standards

#### Code Documentation
```cpp
// Function headers
/**
 * @brief Cascaded PID balance controller
 * @param roll Current roll angle in degrees
 * @param velocitySetpoint Output velocity setpoint in m/s
 * @return true if control active, false if safety stop
 */
bool updateBalanceControl(float roll, float& velocitySetpoint);
```

#### Architecture Documentation
- System diagrams (UML, block diagrams)
- Interface specifications
- Protocol definitions
- Safety requirements

---

## Testing Strategy

### 1. Balance Control Testing

#### Static Testing
```cpp
// Calibration validation
IMU Zero Point: Verify 0° readings
Balance Point: Find optimal angleSetpoint
Motor Symmetry: Left/right current matching
Encoder Calibration: RPM to velocity conversion
```

#### Dynamic Testing
```cpp
// Response characterization
Step Response: Angle setpoint changes
Disturbance Rejection: External forces
Oscillation Analysis: Frequency domain
Stability Margins: Gain and phase margins
```

#### Performance Metrics
```cpp
// Quantitative measures
Settling Time: <2 seconds for 5° disturbance
Steady State Error: <0.5° angle error
Overshoot: <10% for step inputs
Control Effort: <6A average current
```

### 2. Control Integration Testing

#### PS3 Controller Testing
```cpp
// Input validation
Axis Calibration: Deadband and scaling
Button Mapping: Function verification
Disconnect Handling: Safety procedures
Latency Testing: Input to motor response
```

#### Jetson Interface Testing
```cpp
// Communication validation
Packet Integrity: Checksum verification
Data Rates: Bandwidth utilization
Latency Testing: Round-trip timing
Error Handling: Timeout and recovery
```

### 3. Safety System Testing

#### Failure Mode Testing
```cpp
// Safety validation
IMU Failure: Safe mode activation
Communication Loss: Fallback behavior
Motor Failure: Single motor operation
Power Failure: Graceful shutdown
```

#### Emergency Procedures
```cpp
// Emergency testing
Emergency Stop: Response time <100ms
Recovery Procedures: System restart
Diagnostic Mode: Health checking
Manual Override: Emergency control
```

---

## Future Enhancements

### 1. Advanced Control

#### Adaptive PID
```cpp
// Self-tuning control
Parameter Estimation: Online system identification
Gain Scheduling: Load-dependent tuning
Fuzzy Logic: Rule-based adaptation
Machine Learning: Neural network control
```

#### Predictive Control
```cpp
// Model predictive control
State Estimation: Kalman filtering
Trajectory Planning: Optimal control
Disturbance Prediction: Feedforward control
Multi-objective: Balance + navigation
```

### 2. Enhanced Sensing

#### Sensor Fusion
```cpp
// Multi-sensor integration
IMU + Encoders: Complementary filtering
Vision Integration: Visual odometry
LIDAR Integration: Obstacle detection
GPS Integration: Global positioning
```

#### Environmental Awareness
```cpp
// Context-aware control
Surface Detection: Terrain adaptation
Load Estimation: Mass compensation
Wind Compensation: External forces
Slope Detection: Incline handling
```

### 3. Autonomy Features

#### Navigation Stack
```cpp
// ROS 2 integration
SLAM: Simultaneous localization and mapping
Path Planning: A* and RRT algorithms
Obstacle Avoidance: Dynamic window approach
Localization: AMCL particle filtering
```

#### AI/ML Integration
```cpp
// Machine learning
Gesture Recognition: Computer vision
Voice Commands: Speech recognition
Behavior Learning: Reinforcement learning
Predictive Maintenance: Anomaly detection
```

### 4. Performance Optimization

#### Real-time Optimization
```cpp
// Performance tuning
Control Frequency: 200Hz+ operation
Latency Reduction: <5ms total delay
Power Efficiency: Optimal current usage
Thermal Management: Heat dissipation
```

#### Scalability
```cpp
// System expansion
Multi-robot: Swarm coordination
Cloud Integration: Remote monitoring
Edge Computing: Onboard processing
Modular Design: Component swapping
```

---

## Conclusion

The Jetson-Demo-Bot-Johnny-5 software design implements a robust, cascaded PID control system for self-balancing operation. The current implementation provides functional balance control with comprehensive data logging and live tuning capabilities.

### Key Strengths
- **Proven Architecture**: Cascaded control based on literature review
- **Real-time Performance**: 100Hz control loop with hardware-optimized libraries
- **Comprehensive Logging**: Data-driven tuning and analysis
- **Safety-First Design**: Multiple layers of protection
- **Modular Architecture**: Clear separation of concerns

### Current Challenges
- **Motor Chattering**: Velocity PID tuning in progress
- **Control Integration**: PS3 and Jetson interfaces pending
- **Autonomy Development**: Higher-level behaviors not yet implemented

### Next Steps
1. **Complete Balance Tuning**: Resolve chattering through damping and gain adjustment
2. **Implement Control Elements**: PS3 controller and Jetson interface
3. **Develop Autonomy**: ROS 2 integration and 8 core demos
4. **Enhance Safety**: Advanced failure detection and recovery

This design provides a solid foundation for a sophisticated self-balancing robot capable of both manual control and autonomous operation, with clear pathways for future enhancement and expansion.

---

**Document Control**
- **Version**: 1.0
- **Last Updated**: January 2025
- **Next Review**: February 2025
- **Approved By**: Development Team
- **Distribution**: Internal Development Team, External Reviewers
