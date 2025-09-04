# Jetson Self-Balancing Robot

A carry-on sized, two-wheeled self-balancing robot capable of ~8 mph, powered by NVIDIA Jetson AGX Orin for advanced perception, planning, and interaction capabilities.

## Project Overview

This robot demonstrates 8 core capabilities:
1. **Vision-Driven Autonomy** - Visual SLAM + Nav2 navigation
2. **Human Following + Gesture Control** - Person tracking and gesture recognition
3. **Conversational Companion** - Local ASR + LLM integration
4. **Object Recognition + Manipulation** - Computer vision and manipulation
5. **Remote Teleoperation** - Web/VR control interfaces
6. **Multi-Sensor Fusion** - IMU + camera + lidar integration
7. **Voice Command + Wake Word** - Speech recognition and wake word detection
8. **Safety + Recovery** - Fall detection and auto-balance restoration

## Hardware Specifications

### Core Computing
- **Main Controller**: NVIDIA Jetson AGX Orin Dev Kit
- **Microcontroller**: PJRC Teensy 4.1 (ARM Cortex-M7, 600 MHz)
- **IMU**: BNO085 9DOF AHRS sensor

### Motors & Control
- **Motors**: 2× Gyroor 6.5" hoverboard hub motors (36V)
- **Motor Controller**: Flipsky Dual Mini FSESC6.7 Pro (50A)

### Power System
- **Batteries**: 2× HRB 5S 5000 mAh LiPo (wired in series, 10S ~37V)
- **Power Distribution**: Bus bars with 60A ESC fuse, 10A Jetson fuse
- **Voltage Regulation**: IPS-DTD24S198 buck converter (24→19V @ 8A for Jetson)
- **Safety**: Nilight battery switch, XT90-S anti-spark connectors

### Sensors
- **Vision**: Luxonis OAK-D Pro (stereo/depth camera)
- **Audio**: ReSpeaker microphone array (for ASR)
- **Optional**: RPLidar / ToF rangefinder

### Mechanical
- **Chassis**: 3mm aluminum base plate + 6mm Lexan/aluminum top plate
- **Mast**: LS2015 carbon fiber survey pole (1.8m, detachable)
- **Ground Clearance**: 2-3 inches with hoverboard hubs

## Software Requirements

### Operating System
- **Base OS**: Ubuntu 22.04 LTS (Jetson)
- **JetPack**: Latest version for Jetson AGX Orin

### Core Frameworks
- **ROS 2**: Humble (control, Nav2, SLAM)
- **DepthAI**: For OAK-D Pro camera integration
- **OpenCV**: Computer vision processing

### AI/ML Libraries
- **Whisper**: Local speech recognition
- **Local LLM**: For conversational capabilities
- **PyTorch/TensorRT**: For neural network inference

### Development Tools
- **Arduino IDE/PlatformIO**: Teensy firmware development
- **micro-ROS**: Optional ROS integration for Teensy
- **Git**: Version control

## Installation Guide

### 1. Jetson Setup
```bash
# Flash Jetson with Ubuntu 22.04 + JetPack
# Follow NVIDIA's official JetPack installation guide

# Update system
sudo apt update && sudo apt upgrade -y

# Install essential tools
sudo apt install -y git curl wget vim build-essential
```

### 2. ROS 2 Installation
```bash
# Install ROS 2 Humble
sudo apt install software-properties-common
sudo add-apt-repository universe
sudo apt update && sudo apt install curl -y
sudo curl -sSL https://raw.githubusercontent.com/ros/rosdistro/master/ros.key -o /usr/share/keyrings/ros-archive-keyring.gpg
echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/ros-archive-keyring.gpg] http://packages.ros.org/ros2/ubuntu $(. /etc/os-release && echo $UBUNTU_CODENAME) main" | sudo tee /etc/apt/sources.list.d/ros2.list > /dev/null
sudo apt update
sudo apt install ros-humble-desktop python3-argcomplete python3-colcon-common-extensions python3-rosdep python3-vcstool -y

# Initialize rosdep
sudo rosdep init
rosdep update
```

### 3. DepthAI Installation
```bash
# Install DepthAI SDK
pip3 install depthai
pip3 install opencv-python

# Install DepthAI ROS 2 package
mkdir -p ~/ros2_ws/src
cd ~/ros2_ws/src
git clone https://github.com/luxonis/depthai-ros.git
cd ~/ros2_ws
colcon build --symlink-install
```

### 4. Navigation Stack (Nav2)
```bash
# Install Nav2
sudo apt install ros-humble-navigation2 ros-humble-nav2-bringup ros-humble-nav2-lifecycle-manager -y
sudo apt install ros-humble-slam-toolbox ros-humble-robot-localization -y
```

### 5. Audio Processing
```bash
# Install audio dependencies
sudo apt install -y portaudio19-dev python3-pyaudio
pip3 install whisper openai-whisper
pip3 install speechrecognition pyaudio

# For ReSpeaker microphone array
pip3 install respeaker
```

### 6. Teensy Development Environment
```bash
# Install Arduino IDE or PlatformIO
# Arduino IDE: Download from arduino.cc
# PlatformIO: pip3 install platformio

# Install Teensyduino add-on for Arduino IDE
# Download from pjrc.com/teensy/td_download.html
```

## Software Architecture

### ROS 2 Node Structure
```
/jetson_robot/
├── balance_controller/          # Balance control algorithms
├── sensor_fusion/              # IMU + camera + lidar fusion
├── navigation/                 # Nav2 + SLAM integration
├── vision_processing/          # OAK-D + OpenCV processing
├── audio_processing/           # Whisper ASR + wake word
├── human_interaction/          # Gesture recognition + following
├── safety_monitor/             # Fall detection + recovery
├── teleoperation/              # Web/VR control interfaces
└── conversational_ai/          # Local LLM integration
```

### Communication Protocols
- **CAN Bus**: Jetson ↔ Teensy ↔ FSESC
- **ROS 2 Topics**: Inter-node communication
- **Serial/UART**: Teensy ↔ Jetson (backup communication)
- **I2C/SPI**: Sensor communication to Teensy

## Configuration Files

### Default Passwords and Security
- **Jetson Default**: `nvidia/nvidia` (change immediately)
- **SSH**: Enable and configure key-based authentication
- **ROS 2**: Configure DDS security for production use

### Network Configuration
- **WiFi**: Configure for robot connectivity
- **ROS 2 Discovery**: Set ROS_DOMAIN_ID for multi-robot environments
- **Camera Streaming**: Configure OAK-D for optimal performance

## Development Workflow

### Phase 1: Basic Balance Control
1. Develop balance algorithm on Teensy + FSESC
2. Test with minimal hardware (motors + IMU only)
3. Validate stability and control response

### Phase 2: Jetson Integration
1. Add Jetson for vision and SLAM
2. Integrate OAK-D camera with DepthAI
3. Implement basic navigation capabilities

### Phase 3: Advanced Features
1. Add voice control and conversational AI
2. Implement human following and gesture control
3. Develop teleoperation interfaces

### Phase 4: Safety and Polish
1. Add fall detection and recovery
2. Implement comprehensive safety systems
3. Performance optimization and testing

## Safety Considerations

### Electrical Safety
- **Fusing**: 60A main ESC fuse, 10A Jetson fuse
- **Connectors**: XT90-S anti-spark connectors
- **Isolation**: Separate power and logic grounds
- **Emergency Stop**: Main battery cutoff switch

### Software Safety
- **Watchdog Timers**: Prevent runaway conditions
- **Fall Detection**: IMU-based tilt monitoring
- **Emergency Recovery**: Auto-balance restoration
- **Battery Monitoring**: Low voltage protection

## Troubleshooting

### Common Issues
1. **Balance Instability**: Check IMU calibration and control gains
2. **Camera Issues**: Verify DepthAI installation and USB connection
3. **Power Problems**: Check fuse ratings and connector integrity
4. **ROS 2 Discovery**: Ensure proper network configuration

### Debug Tools
- **ROS 2**: `rqt`, `ros2 topic echo`, `ros2 node list`
- **DepthAI**: `depthai_demo.py` for camera testing
- **Teensy**: Serial monitor for firmware debugging

## Contributing

This project follows a modular development approach. Each demo capability should be developed as a separate ROS 2 package with clear interfaces and documentation.

## License

[Specify your preferred license]

## Acknowledgments

- NVIDIA for Jetson platform and JetPack
- Luxonis for OAK-D camera and DepthAI
- ROS 2 community for navigation and SLAM tools
- PJRC for Teensy microcontroller platform

---

*Last updated: 2025-01-27*
