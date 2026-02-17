# Jetson Demo Bot - Johnny 5

A self-balancing robot built with NVIDIA Jetson and Teensy 4.1, featuring autonomous navigation, manual control, and real-time balance control.

## 🤖 System Overview

### Hardware Architecture
- **Jetson Nano/Orin**: High-level control, navigation, computer vision
- **Teensy 4.1**: Real-time balance control, motor control, sensor fusion
- **BNO085 IMU**: 9-DOF orientation sensing for balance control
- **Dual FSESC Motor Controllers**: Hoverboard hub motor control via UART
- **LiDAR**: LDROBOT STL-19P for SLAM and navigation
- **PS3 Controller**: Manual remote control capability

### Communication Architecture
- **Jetson ↔ Teensy**: USB Serial (2Mbaud) - Power + Data in one cable
- **Teensy ↔ ESCs**: UART (115200 baud) - Motor current commands
- **Teensy ↔ IMU**: I2C - Real-time orientation data
- **Teensy ↔ PS3**: USB Host - Wireless gamepad input

## 🚀 Quick Start

### Development Mode (Laptop Connected)
```bash
# Program Teensy balance controller
cd teensy_balance_controller
arduino-cli compile --fqbn teensy:avr:teensy41 .
teensy_loader_cli --mcu=TEENSY41 -w -v teensy_balance_controller.ino.hex

# Monitor balance controller
minicom -b 2000000 -D /dev/ttyACM0
```

### Production Mode (Jetson Connected)
```bash
# Test Jetson-Teensy interface
python3 test_jetson_interface.py

# Run robot control
python3 robot_main.py
```

**⚠️ Important**: Only ONE device can connect to Teensy USB at a time (laptop OR Jetson, not both).

## 📁 Project Structure

```
├── teensy_balance_controller/          # Core balance control firmware
├── ldrobot_lidar_ros2/                # LiDAR integration and SLAM
├── docs/
│   ├── delivery/                      # Project management and tasks
│   ├── deployment/                    # Deployment guides
│   └── setup/                         # Development environment setup
├── test_jetson_interface.py           # Jetson-Teensy communication test
└── robotics_*.md                      # Design documentation
```

## 🔧 Development Setup

### Prerequisites
- **Arduino IDE 2.3.3+** with **Teensyduino 1.59+**
- **Python 3.8+** with `pyserial`
- **ROS2 Humble** (for navigation stack)

### Quick Setup
```bash
# Ubuntu/Jetson setup
./setup_ubuntu_dev.md

# Windows setup  
./setup_windows_dev.bat

# Teensy development environment
./docs/setup/setup_teensy_dev.md
```

## 🎯 Current Status

### ✅ Completed Features
- **Balance Control**: BNO085 IMU + PID → ESC current control
- **Jetson Interface**: USB serial protocol for bidirectional communication  
- **Real-time Control**: 100Hz balance loop with sensor fusion
- **Development Tools**: CLI compilation, upload, and testing workflows

### 🚧 In Progress
- PS3 remote control integration
- Jetson velocity/steering command processing
- Mode blending (balance + navigation + manual)

### 📋 Planned Features
- ROS2 navigation stack integration
- Computer vision for obstacle avoidance
- Web interface for monitoring and control
- Autonomous waypoint navigation

## 🔌 Hardware Connections

### Power Distribution
- **Main Power**: 24V battery → FSESC controllers
- **Teensy Power**: 5V from Jetson USB connection
- **Jetson Power**: 5V/4A barrel jack or USB-C

### Signal Connections
- **Teensy Pin 18 (SDA)** → BNO085 SDA
- **Teensy Pin 19 (SCL)** → BNO085 SCL  
- **Teensy Serial1** → Left ESC UART
- **Teensy Serial2** → Right ESC UART
- **Teensy USB** → Jetson USB (power + data)

## 📊 Performance Specifications
- **Balance Control**: 100Hz PID loop, ±15A current range
- **IMU Data Rate**: 100Hz rotation vector, <10ms latency
- **Jetson Communication**: 2Mbaud, 20Hz IMU data, 10Hz status
- **Motor Control**: UART at 115200 baud to dual ESCs

## 📖 Documentation

- **[Setup Guide](docs/setup/setup_teensy_dev.md)**: Development environment
- **[Deployment Guide](docs/deployment/teensy_jetson_deployment.md)**: Production deployment
- **[Design Methodology](robotics_design_methodology.md)**: System architecture
- **[Project Overview](robotics_project_overview.md)**: Hardware and software overview

## 🤝 Contributing

This project follows a structured development methodology with:
- **Product Backlog Items (PBIs)** for feature planning
- **Task-based development** with clear acceptance criteria
- **Git workflow** with task-specific commits and PRs

See `docs/delivery/backlog.md` for current development status.

## 📄 License

Open source hardware and software project. See individual component licenses for details.

---

**Johnny 5 is alive!** 🤖⚡