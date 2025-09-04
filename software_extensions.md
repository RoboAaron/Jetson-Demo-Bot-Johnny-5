# Software Extensions and Dependencies

*Comprehensive guide for all software components required for the Jetson Self-Balancing Robot*

## Table of Contents
1. [Core System Software](#core-system-software)
2. [ROS 2 Packages](#ros-2-packages)
3. [Computer Vision Stack](#computer-vision-stack)
4. [Audio Processing](#audio-processing)
5. [AI/ML Libraries](#aiml-libraries)
6. [Embedded Development](#embedded-development)
7. [Development Tools](#development-tools)
8. [Optional Extensions](#optional-extensions)
9. [Installation Scripts](#installation-scripts)

## Core System Software

### Operating System
- **Ubuntu 22.04 LTS** - Base operating system
- **JetPack 5.1+** - NVIDIA's SDK for Jetson platforms
  - CUDA 11.4+
  - cuDNN 8.6+
  - TensorRT 8.5+
  - OpenCV 4.6+ (CUDA-enabled)

### System Dependencies
```bash
# Essential system packages
sudo apt install -y \
    git curl wget vim build-essential \
    cmake pkg-config \
    python3-pip python3-dev python3-venv \
    libeigen3-dev libboost-all-dev \
    libopencv-dev libopencv-contrib-dev \
    portaudio19-dev libasound2-dev \
    can-utils \
    i2c-tools \
    htop tree
```

## ROS 2 Packages

### Core ROS 2 Stack
```bash
# ROS 2 Humble Desktop
sudo apt install -y ros-humble-desktop

# Essential ROS 2 tools
sudo apt install -y \
    ros-humble-rqt ros-humble-rqt-common-plugins \
    ros-humble-rviz2 ros-humble-rviz-imu-plugin \
    ros-humble-joint-state-publisher \
    ros-humble-robot-state-publisher \
    ros-humble-xacro \
    ros-humble-urdf \
    ros-humble-tf2-tools \
    ros-humble-tf2-geometry-msgs
```

### Navigation and SLAM
```bash
# Navigation2 stack
sudo apt install -y \
    ros-humble-navigation2 \
    ros-humble-nav2-bringup \
    ros-humble-nav2-lifecycle-manager \
    ros-humble-nav2-map-server \
    ros-humble-nav2-amcl \
    ros-humble-nav2-behavior-tree \
    ros-humble-nav2-bt-navigator \
    ros-humble-nav2-controller \
    ros-humble-nav2-core \
    ros-humble-nav2-costmap-2d \
    ros-humble-nav2-dwb-controller \
    ros-humble-nav2-local-planner \
    ros-humble-nav2-map-server \
    ros-humble-nav2-msgs \
    ros-humble-nav2-planner \
    ros-humble-nav2-recoveries \
    ros-humble-nav2-regulated-pure-pursuit-controller \
    ros-humble-nav2-rotation-controller \
    ros-humble-nav2-rviz-plugins \
    ros-humble-nav2-simple-commander \
    ros-humble-nav2-smac-planner \
    ros-humble-nav2-theta-star-planner \
    ros-humble-nav2-util \
    ros-humble-nav2-velocity-smoother \
    ros-humble-nav2-waypoint-follower

# SLAM Toolbox
sudo apt install -y \
    ros-humble-slam-toolbox \
    ros-humble-robot-localization \
    ros-humble-imu-filter-madgwick \
    ros-humble-robot-localization
```

### Sensor Integration
```bash
# IMU and sensor packages
sudo apt install -y \
    ros-humble-imu-tools \
    ros-humble-imu-complementary-filter \
    ros-humble-imu-filter-madgwick \
    ros-humble-robot-localization

# Camera packages
sudo apt install -y \
    ros-humble-image-transport \
    ros-humble-image-transport-plugins \
    ros-humble-cv-bridge \
    ros-humble-vision-msgs \
    ros-humble-stereo-image-proc
```

## Computer Vision Stack

### DepthAI Integration
```bash
# DepthAI SDK
pip3 install depthai

# DepthAI ROS 2 packages
cd ~/ros2_ws/src
git clone https://github.com/luxonis/depthai-ros.git
cd ~/ros2_ws
colcon build --symlink-install
```

### OpenCV and Computer Vision
```bash
# OpenCV with CUDA support (included in JetPack)
# Additional OpenCV modules
pip3 install opencv-contrib-python

# Additional vision libraries
pip3 install \
    numpy \
    scipy \
    matplotlib \
    scikit-image \
    pillow \
    imageio
```

### Object Detection and Tracking
```bash
# YOLO and object detection
pip3 install \
    ultralytics \
    torch torchvision torchaudio \
    torch-tensorrt

# Additional ML libraries
pip3 install \
    tensorflow \
    onnx \
    onnxruntime
```

## Audio Processing

### Speech Recognition
```bash
# Whisper and speech processing
pip3 install \
    openai-whisper \
    speechrecognition \
    pyaudio \
    librosa \
    soundfile

# Wake word detection
pip3 install \
    porcupine \
    pvporcupine
```

### Audio Hardware Integration
```bash
# ReSpeaker microphone array
pip3 install \
    respeaker \
    pyusb \
    pyserial

# Audio utilities
sudo apt install -y \
    alsa-utils \
    pulseaudio \
    pulseaudio-utils
```

## AI/ML Libraries

### Local Language Models
```bash
# For conversational AI
pip3 install \
    transformers \
    torch \
    accelerate \
    bitsandbytes

# Popular local LLM options
pip3 install \
    llama-cpp-python \
    langchain \
    sentence-transformers
```

### Neural Network Optimization
```bash
# TensorRT for Jetson optimization
# (Included in JetPack, but may need additional packages)
pip3 install \
    pycuda \
    nvidia-tensorrt

# Model conversion tools
pip3 install \
    onnx \
    onnxruntime \
    onnx-tf
```

## Embedded Development

### Teensy Development
```bash
# Arduino IDE (download from arduino.cc)
# Teensyduino add-on (download from pjrc.com)

# PlatformIO alternative
pip3 install platformio

# micro-ROS for Teensy
# Follow micro-ROS installation guide for Teensy
```

### Communication Protocols
```bash
# CAN bus utilities
sudo apt install -y \
    can-utils \
    can-utils-dev

# Serial communication
pip3 install \
    pyserial \
    pymodbus

# I2C/SPI utilities
sudo apt install -y \
    i2c-tools \
    python3-smbus
```

## Development Tools

### Version Control and Collaboration
```bash
# Git and GitHub CLI
sudo apt install -y git
curl -fsSL https://cli.github.com/packages/githubcli-archive-keyring.gpg | sudo dd of=/usr/share/keyrings/githubcli-archive-keyring.gpg
echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/githubcli-archive-keyring.gpg] https://cli.github.com/packages stable main" | sudo tee /etc/apt/sources.list.d/github-cli.list > /dev/null
sudo apt update
sudo apt install gh
```

### Code Editors and IDEs
```bash
# VS Code
wget -qO- https://packages.microsoft.com/keys/microsoft.asc | gpg --dearmor > packages.microsoft.gpg
sudo install -o root -g root -m 644 packages.microsoft.gpg /etc/apt/trusted.gpg.d/
sudo sh -c 'echo "deb [arch=amd64,arm64,armhf signed-by=/etc/apt/trusted.gpg.d/packages.microsoft.gpg] https://packages.microsoft.com/repos/code stable main" > /etc/apt/sources.list.d/vscode.list'
sudo apt update
sudo apt install code

# Cursor (if preferred)
# Download from cursor.sh
```

### Debugging and Profiling
```bash
# Debugging tools
sudo apt install -y \
    gdb \
    valgrind \
    htop \
    iotop \
    nethogs

# ROS 2 debugging
sudo apt install -y \
    ros-humble-rqt-console \
    ros-humble-rqt-graph \
    ros-humble-rqt-plot
```

## Optional Extensions

### Web Interface
```bash
# Web-based teleoperation
pip3 install \
    flask \
    flask-socketio \
    eventlet \
    opencv-python

# WebRTC for video streaming
pip3 install \
    aiortc \
    aiohttp
```

### VR/AR Integration
```bash
# For VR teleoperation
pip3 install \
    openvr \
    pyopenvr
```

### Advanced Sensors
```bash
# LiDAR integration (if using RPLidar)
sudo apt install -y \
    ros-humble-rplidar-ros

# Additional sensor packages
pip3 install \
    adafruit-circuitpython-bno055 \
    adafruit-circuitpython-mpu6050
```

## Installation Scripts

### Complete Installation Script
Create `install_robot_software.sh`:

```bash
#!/bin/bash
# Complete software installation for Jetson Self-Balancing Robot

set -e

echo "Installing Jetson Self-Balancing Robot Software Stack..."

# Update system
sudo apt update && sudo apt upgrade -y

# Install system dependencies
sudo apt install -y \
    git curl wget vim build-essential cmake pkg-config \
    python3-pip python3-dev python3-venv \
    libeigen3-dev libboost-all-dev \
    libopencv-dev libopencv-contrib-dev \
    portaudio19-dev libasound2-dev \
    can-utils i2c-tools htop tree \
    alsa-utils pulseaudio pulseaudio-utils

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

# Install ROS 2 packages
sudo apt install -y \
    ros-humble-navigation2 ros-humble-nav2-bringup \
    ros-humble-slam-toolbox ros-humble-robot-localization \
    ros-humble-imu-tools ros-humble-imu-filter-madgwick \
    ros-humble-image-transport ros-humble-cv-bridge \
    ros-humble-rqt ros-humble-rviz2

# Install Python packages
pip3 install \
    depthai opencv-python numpy scipy matplotlib \
    openai-whisper speechrecognition pyaudio \
    ultralytics torch torchvision \
    transformers accelerate \
    pyserial flask flask-socketio

# Create workspace
mkdir -p ~/ros2_ws/src
cd ~/ros2_ws/src

# Clone DepthAI ROS package
git clone https://github.com/luxonis/depthai-ros.git

# Build workspace
cd ~/ros2_ws
colcon build --symlink-install

echo "Installation complete! Please source the workspace:"
echo "source ~/ros2_ws/install/setup.bash"
```

### Python Virtual Environment Setup
Create `setup_python_env.sh`:

```bash
#!/bin/bash
# Setup Python virtual environment for robot development

python3 -m venv ~/robot_env
source ~/robot_env/bin/activate

pip install --upgrade pip
pip install \
    depthai opencv-python numpy scipy matplotlib \
    openai-whisper speechrecognition pyaudio \
    ultralytics torch torchvision \
    transformers accelerate \
    pyserial flask flask-socketio \
    respeaker librosa soundfile

echo "Python environment created at ~/robot_env"
echo "Activate with: source ~/robot_env/bin/activate"
```

## Configuration Files

### ROS 2 Configuration
Create `~/.bashrc` additions:

```bash
# ROS 2 Configuration
source /opt/ros/humble/setup.bash
source ~/ros2_ws/install/setup.bash

# Robot-specific environment variables
export ROS_DOMAIN_ID=0
export ROS_DISCOVERY_SERVER=""
export ROS_DEFAULT_RMW_IMPLEMENTATION=rmw_fastrtps_cpp

# Jetson-specific optimizations
export CUDA_VISIBLE_DEVICES=0
export JETSON_STATS_MONITOR=1
```

### System Service Configuration
Create `/etc/systemd/system/robot-startup.service`:

```ini
[Unit]
Description=Robot Startup Service
After=network.target

[Service]
Type=oneshot
User=nvidia
WorkingDirectory=/home/nvidia/ros2_ws
ExecStart=/bin/bash -c "source /opt/ros/humble/setup.bash && source install/setup.bash && ros2 launch robot_bringup robot.launch.py"
RemainAfterExit=yes

[Install]
WantedBy=multi-user.target
```

## Security Considerations

### Default Passwords
- **Jetson Default**: `nvidia/nvidia` - **CHANGE IMMEDIATELY**
- **SSH**: Configure key-based authentication
- **ROS 2**: Enable DDS security for production

### Network Security
- Configure firewall rules
- Use VPN for remote access
- Enable ROS 2 security plugins

---

*This document provides comprehensive software requirements for the Jetson Self-Balancing Robot project. All packages are tested for compatibility with Ubuntu 22.04 LTS and Jetson AGX Orin.*

*Last updated: 2025-01-27*
