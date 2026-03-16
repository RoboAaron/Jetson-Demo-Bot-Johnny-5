# Johnny-5 Jetson AGX Orin — Headless Setup & Offline Deployment Guide

> **Scope:** Complete instructions for ROS 2 Humble · SLAM Toolbox · Nav2 · Sensor Stack · Air-Gap (Offline) Install  
> **Hardware:** NVIDIA Jetson AGX Orin Dev Kit · Teensy 4.1 · LDROBOT STL-19P/D500 LiDAR · Luxonis OAK-D Pro · BNO085 IMU · Dual Flipsky FSESC 6.7 Pro · ReSpeaker Mic Array · ESP32 PS3 Bridge  
> **Software:** Ubuntu 22.04 (JetPack 6.x) · ROS 2 Humble · colcon · SLAM Toolbox · Nav2 · robot_localization EKF · twist_mux · teleop_twist_joy · DepthAI · Whisper ASR · llama.cpp

---

## Table of Contents

1. [Architecture Overview](#1-architecture-overview)
2. [Tools & Materials Checklist](#2-tools--materials-checklist)
3. [Phase A — Build Offline Bundle (Internet Machine)](#3-phase-a--build-offline-bundle-internet-machine)
4. [Phase B — Jetson Headless Setup (No Internet)](#4-phase-b--jetson-headless-setup-no-internet)
5. [Headless Operation — SSH Workflow](#5-headless-operation--ssh-workflow)
6. [Launching the Full Robot Stack](#6-launching-the-full-robot-stack)
7. [Sensor Bring-Up & Verification](#7-sensor-bring-up--verification)
8. [Debugging Guide](#8-debugging-guide)
9. [Demo Wave Sequencing & Exit Criteria](#9-demo-wave-sequencing--exit-criteria)
10. [Quick Reference Card](#10-quick-reference-card)

---

## 1. Architecture Overview

### Software Layers

| Layer | What it is | Key files |
|-------|-----------|-----------|
| **1 — Firmware** | Teensy 4.1 (Arduino). 500 Hz balance PID loop, BNO085 IMU, dual FSESC UART. | `teensy_balance_cascaded/teensy_balance_cascaded.ino` |
| **2 — Jetson Bridge** | Python wrapper around USB serial. Watchdog, reconnect, `get_state()` / `set_velocity()` API. | `jetson/jetson_bridge.py`, `jetson/teensy_comms.py` |
| **3 — ROS 2 Nodes** | `balance_bridge_node` publishes `/odom`, `/imu/roll`, `/imu/pitch`, `/imu/yaw`. Subscribes to `/cmd_vel`. `esp32_joy_node` reads PS3 controller. | `jetson/ros2/balance_bridge/` |
| **4 — Navigation** | SLAM Toolbox (mapping), Nav2 (autonomous navigation). | `johnny5_bringup/launch/` |
| **5 — Sensor Fusion** | `robot_localization` EKF: `/odom` + `/imu/data` → `/odometry/filtered`. | `johnny5_sensor_fusion/config/ekf.yaml` |
| **6 — Perception** | DepthAI depthai-ros (OAK-D Pro). openWakeWord + Whisper + llama.cpp (voice demos). | Future waves |

### Key ROS 2 Topics

| Topic | Source | Consumer |
|-------|--------|---------|
| `/cmd_vel` | twist_mux output | balance_bridge_node → Teensy |
| `/cmd_vel_joy` | esp32_joy_node → teleop_twist_joy | twist_mux (priority 100) |
| `/cmd_vel_nav` | Nav2 controller_server | twist_mux (priority 50) |
| `/cmd_vel_web` | rosbridge websocket | twist_mux (priority 25) |
| `/cmd_vel_voice` | voice_cmd_vel node | twist_mux (priority 10) |
| `/odom` | balance_bridge_node (dead-reckoning) | EKF, Nav2 |
| `/odometry/filtered` | robot_localization EKF | Nav2 |
| `/scan` | LDROBOT LiDAR driver | SLAM Toolbox |
| `/map` | SLAM Toolbox | Nav2 global costmap |
| `/imu/roll`, `/imu/pitch`, `/imu/yaw` | balance_bridge_node (Float32) | Monitoring, EKF |
| `/robot_state` | balance_bridge_node (JSON, 20 Hz) | Tuning GUI |
| `/joy` | esp32_joy_node | teleop_twist_joy |

### TF Chain (required for Nav2 and SLAM)

```
map → odom → base_link → laser
                       → imu_link
                       → camera_link
```

- `map → odom` published by SLAM Toolbox
- `odom → base_link` published by balance_bridge_node (TF broadcaster)
- `base_link → laser` static transform (in launch file)

### twist_mux Priority

```
/cmd_vel_joy   (100) — PS3 controller, always overrides everything
/cmd_vel_nav   (50)  — Nav2 autonomous navigation
/cmd_vel_web   (25)  — browser joystick (PBI-12)
/cmd_vel_voice (10)  — voice commands (PBI-10)
         ↓
       /cmd_vel  →  balance_bridge_node  →  Teensy
```

---

## 2. Tools & Materials Checklist

### Hardware Required

- [ ] Jetson AGX Orin Dev Kit (32 GB RAM recommended)
- [ ] Ethernet cable + router/switch
- [ ] Host PC: Ubuntu 22.04 or 24.04, internet connected
- [ ] 16 GB+ USB-A flash drive (for air-gap transfer)
- [ ] Powered USB hub (for simultaneous LiDAR + OAK-D + Teensy + ESP32)
- [ ] Monitor + keyboard (first boot only; SSH after)

### Software to Collect (Phase A, internet machine)

| Item | Source |
|------|--------|
| NVIDIA SDK Manager | https://developer.nvidia.com/sdk-manager |
| JetPack 6.x image | Auto-downloaded by SDK Manager |
| ROS 2 Humble .deb packages (arm64) | packages.ros.org/ros2/ubuntu |
| Python wheels (aarch64) | pypi.org via `pip download` |
| Johnny-5 repo | https://github.com/RoboAaron/Jetson-Demo-Bot-Johnny-5 |
| ldlidar_stl_ros2 | https://github.com/ldrobotSensorTeam/ldlidar_stl_ros2 |
| depthai-ros | https://github.com/luxonis/depthai-ros |
| Whisper small.en weights | ~150 MB, via `openai-whisper` Python package |
| Llama 3.2 3B Instruct Q4_K_M GGUF | ~2.0 GB, HuggingFace: bartowski/Llama-3.2-3B-Instruct-GGUF |
| llama.cpp aarch64 binary | https://github.com/ggerganov/llama.cpp/releases |
| openWakeWord models | via `openwakeword` Python package |

---

## 3. Phase A — Build Offline Bundle (Internet Machine)

> Run every step in this section on your **internet-connected host PC**. Goal: produce a USB drive with everything the Jetson needs.

### 3.1 Flash JetPack 6.x (if not already done)

```bash
# 1. Install SDK Manager on host PC (deb package from NVIDIA)
# 2. Put Jetson into recovery mode:
#    Hold RECOVERY → press POWER → release RECOVERY after 2 s
# 3. Launch SDK Manager, select JetPack 6.1 (or latest 6.x), target: Jetson AGX Orin
# 4. Flash only "Jetson OS" component (~20 min)
# 5. Complete first-boot: set username=robot, set password
```

> **JetPack 6.x = Ubuntu 22.04 (Jammy).** ROS 2 Humble .deb packages are published for Jammy arm64. CUDA 12.6 (JetPack 6.1) is required for GPU-accelerated faster-whisper (ctranslate2 ≥ 4.5.0).

### 3.2 Create Bundle Directory

```bash
# On HOST PC (internet connected)
mkdir -p ~/j5_bundle/{debs/ros2,debs/system,pip_wheels,ros2_src,models/whisper_cache,models/llm,models/llama_cpp,models/openwakeword,udev,scripts}
cd ~/j5_bundle
```

### 3.3 Download ROS 2 Humble .deb Packages

```bash
# Add ROS 2 Humble apt source for arm64
sudo apt install curl gnupg lsb-release
sudo curl -sSL https://raw.githubusercontent.com/ros/rosdistro/master/ros.key \
  -o /usr/share/keyrings/ros-archive-keyring.gpg

echo "deb [arch=arm64 signed-by=/usr/share/keyrings/ros-archive-keyring.gpg] \
  http://packages.ros.org/ros2/ubuntu jammy main" \
  | sudo tee /etc/apt/sources.list.d/ros2-arm64.list

sudo dpkg --add-architecture arm64
sudo apt update

# Download all required packages (do NOT install)
cd ~/j5_bundle/debs/ros2
apt-get download \
  ros-humble-desktop \
  ros-humble-slam-toolbox \
  ros-humble-nav2-bringup \
  ros-humble-robot-localization \
  ros-humble-twist-mux \
  ros-humble-teleop-twist-joy \
  ros-humble-teleop-twist-keyboard \
  ros-humble-rosbridge-suite \
  ros-humble-tf2-tools \
  ros-humble-xacro \
  ros-humble-joint-state-publisher \
  ros-humble-robot-state-publisher \
  ros-humble-image-transport \
  ros-humble-cv-bridge \
  python3-colcon-common-extensions \
  python3-rosdep \
  python3-vcstool \
  python3-argcomplete 2>&1 | tee deb_download.log

# Create local apt repo index
dpkg-scanpackages . /dev/null | gzip -9c > Packages.gz
```

> **x86_64 host note:** If `apt-get download` refuses arm64 packages, run inside arm64 Docker:
> ```bash
> docker run --platform linux/arm64 -it --rm -v ~/j5_bundle:/bundle ubuntu:22.04 bash
> # Then run the apt-get download commands above inside the container
> ```
> Or SSH into the Jetson while it still has internet and run the downloads there.

### 3.4 Download Python Wheels (aarch64)

```bash
cd ~/j5_bundle/pip_wheels

pip download \
  --platform linux_aarch64 \
  --python-version 310 \
  --only-binary=:all: \
  --no-deps \
  -d ~/j5_bundle/pip_wheels \
  pyserial numpy scipy transforms3d smbus2 \
  websocket-client sounddevice pyaudio \
  faster-whisper openwakeword depthai \
  2>&1 | tee pip_download.log

# For any that fail (no aarch64 wheel), download source:
pip download --no-binary transforms3d transforms3d
```

### 3.5 Clone Source Repositories

```bash
cd ~/j5_bundle/ros2_src

git clone https://github.com/RoboAaron/Jetson-Demo-Bot-Johnny-5.git johnny5
git clone https://github.com/ldrobotSensorTeam/ldlidar_stl_ros2.git
git clone --depth 1 https://github.com/luxonis/depthai-ros.git

# Pack as tarballs (preserves git history, easier to transfer)
for d in */; do
  tar czf "${d%/}.tar.gz" "$d"
  echo "Packed: ${d%/}.tar.gz"
done
```

### 3.6 Download AI Models

```bash
# Whisper small.en (~150 MB)
pip install openai-whisper
python3 -c "import whisper; whisper.load_model('small.en')"
cp -r ~/.cache/whisper ~/j5_bundle/models/whisper_cache

# Llama 3.2 3B Instruct Q4_K_M (~2.0 GB)
pip install huggingface_hub
python3 -c "
from huggingface_hub import hf_hub_download
hf_hub_download(
    repo_id='bartowski/Llama-3.2-3B-Instruct-GGUF',
    filename='Llama-3.2-3B-Instruct-Q4_K_M.gguf',
    local_dir='$HOME/j5_bundle/models/llm'
)
"

# openWakeWord models
pip install openwakeword
python3 -c "import openwakeword; openwakeword.utils.download_models()"
cp -r ~/.local/share/openwakeword ~/j5_bundle/models/openwakeword

# llama.cpp aarch64 binary (from GitHub releases)
LLAMA_URL=$(curl -s https://api.github.com/repos/ggerganov/llama.cpp/releases/latest \
  | python3 -c "import sys,json; assets=json.load(sys.stdin)['assets']; \
    print(next(a['browser_download_url'] for a in assets if 'ubuntu' in a['name'] and 'arm64' in a['name']), '')")
wget -O ~/j5_bundle/models/llama_cpp/llama_server_arm64.zip "$LLAMA_URL"
unzip ~/j5_bundle/models/llama_cpp/llama_server_arm64.zip -d ~/j5_bundle/models/llama_cpp/
```

### 3.7 Write udev Rules

```bash
# LDROBOT LiDAR (CP2102 USB-UART bridge, VID:PID 10c4:ea60)
cat > ~/j5_bundle/udev/99-ldlidar.rules << 'EOF'
SUBSYSTEM=="tty", ATTRS{idVendor}=="10c4", ATTRS{idProduct}=="ea60", SYMLINK+="ldlidar", MODE="0666"
EOF

# OAK-D Pro (Luxonis Myriad X, VID 03e7)
cat > ~/j5_bundle/udev/80-movidius.rules << 'EOF'
SUBSYSTEM=="usb", ATTRS{idVendor}=="03e7", MODE="0666"
EOF

# Teensy 4.1 (PJRC, VID 16c0)
cat > ~/j5_bundle/udev/49-teensy.rules << 'EOF'
SUBSYSTEM=="usb", ATTRS{idVendor}=="16c0", MODE="0666", GROUP="plugdev"
EOF

# ESP32 (Silicon Labs CP210x and CH340)
cat > ~/j5_bundle/udev/50-esp32.rules << 'EOF'
SUBSYSTEM=="tty", ATTRS{idVendor}=="10c4", SYMLINK+="esp32_%n", MODE="0666"
SUBSYSTEM=="tty", ATTRS{idVendor}=="1a86", SYMLINK+="esp32_%n", MODE="0666"
EOF
```

### 3.8 Copy Bundle to USB Drive

```bash
# Format USB as ext4
sudo mkfs.ext4 /dev/sdX1    # replace sdX1 with your USB partition
sudo mount /dev/sdX1 /mnt/usb
sudo cp -r ~/j5_bundle/* /mnt/usb/
sudo umount /mnt/usb
sync

echo "Bundle size:"
du -sh ~/j5_bundle/
```

**Expected total size: 6–15 GB** depending on which model weights you include.

---

## 4. Phase B — Jetson Headless Setup (No Internet)

### 4.0 Serial Console Over USB (No Monitor)

Use this when you have **no monitor or keyboard** and need a terminal on the Jetson (first boot, recovery, or before SSH is set up). The Jetson exposes a USB serial console; your laptop is the host.

**Hardware**

- **Jetson side:** USB Type-C **device** port (the one next to the 40-pin header — not the other Type-C ports). See [NVIDIA Get Started — Jetson AGX Orin Dev Kit](https://developer.nvidia.com/embedded/learn/get-started-jetson-agx-orin-devkit).
- **Host side:** USB-A or USB-C on your laptop. Use the cable that came with the dev kit or a known-good USB data cable.

**On your Ubuntu host (laptop)**

1. **Stop services that grab the serial device** (otherwise you may get "connection failed" or no output):

   ```bash
   sudo systemctl stop brltty
   sudo systemctl stop ModemManager
   ```

   To prevent them from starting on boot (optional):  
   `sudo systemctl disable brltty ModemManager`

2. **Connect the Jetson** (power on if needed). Wait a few seconds, then check that the device appears:

   ```bash
   sudo dmesg | tail -30
   # Look for: "cdc_acm ... ttyACM0: USB ACM device"
   ls -la /dev/ttyACM*
   ```

   Use `sudo dmesg` because reading the kernel log often requires root. If the last lines only show storage or network (e.g. `sda`, `cdc_ncm`), the serial line may have appeared earlier — so always check with `ls /dev/ttyACM*`. If you don’t see any `ttyACM*` device, try another USB port or cable; confirm you’re using the **device** port on the Jetson.

3. **Open the serial console** (115200 8N1):

   ```bash
   sudo screen /dev/ttyACM0 115200
   ```

   Alternatives: `sudo minicom -b 115200 -D /dev/ttyACM0` or `sudo cu -l /dev/ttyACM0 -s 115200`.

4. **If the screen is blank:** Press **Enter** several times. If you still see nothing, power off the Jetson, leave the USB cable connected, then power it on again and wait for the boot messages. You may need to start `screen` (or minicom) again after the Jetson reboots.

5. **To exit screen without disconnecting:** `Ctrl+A` then `K`, then `y` to kill the session. To detach and leave the session running: `Ctrl+A` then `D`.

After you have a shell, continue with [4.1 First Boot & SSH Access](#41-first-boot--ssh-access) (e.g. set password, enable SSH, set static IP). For mounting the offline bundle and running install scripts, use [4.2](#42-mount-usb--configure-offline-apt) onward (mount the microSD instead of USB if that’s what you’re using).

### 4.1 First Boot & SSH Access

```bash
# Connect monitor + keyboard for first boot only (or use serial console — see 4.0)
# Complete Ubuntu setup wizard:
#   Username: robot
#   Set a password
#   Connect to Ethernet

# Get IP address
ip addr show eth0

# Enable SSH
sudo systemctl enable ssh && sudo systemctl start ssh

# From your laptop — verify SSH works
ssh robot@<JETSON_IP>

# Set static IP (recommended)
sudo nano /etc/netplan/01-netcfg.yaml
```

```yaml
network:
  version: 2
  ethernets:
    eth0:
      addresses: [192.168.1.200/24]
      gateway4: 192.168.1.1
      nameservers:
        addresses: [8.8.8.8, 8.8.4.4]
```

```bash
sudo netplan apply
# After this, disconnect the monitor — all work is done over SSH
```

### 4.2 Mount USB & Configure Offline apt

```bash
# Insert USB drive
sudo mkdir -p /mnt/j5bundle
sudo mount /dev/sda1 /mnt/j5bundle   # adjust partition as needed

# Verify contents
ls /mnt/j5bundle
# Expected: debs/  pip_wheels/  ros2_src/  models/  udev/  scripts/

# Copy .debs to apt cache
sudo cp /mnt/j5bundle/debs/ros2/*.deb    /var/cache/apt/archives/
sudo cp /mnt/j5bundle/debs/system/*.deb  /var/cache/apt/archives/

# Create local apt repository
cd /mnt/j5bundle/debs
dpkg-scanpackages . /dev/null | gzip -9c > Packages.gz

# Add as apt source
echo "deb [trusted=yes] file:///mnt/j5bundle/debs ./" \
  | sudo tee /etc/apt/sources.list.d/local-bundle.list

# Disable internet sources (prevents failures if no internet)
sudo mv /etc/apt/sources.list /etc/apt/sources.list.bak

sudo apt update
```

### 4.3 Install ROS 2 Humble (Offline)

```bash
sudo apt install -y \
  ros-humble-desktop \
  ros-humble-slam-toolbox \
  ros-humble-nav2-bringup \
  ros-humble-robot-localization \
  ros-humble-twist-mux \
  ros-humble-teleop-twist-joy \
  ros-humble-teleop-twist-keyboard \
  ros-humble-rosbridge-suite \
  ros-humble-tf2-tools \
  ros-humble-xacro \
  ros-humble-joint-state-publisher \
  ros-humble-robot-state-publisher \
  python3-colcon-common-extensions \
  python3-rosdep \
  python3-vcstool

# Source ROS 2 (add to .bashrc)
echo "source /opt/ros/humble/setup.bash" >> ~/.bashrc
source ~/.bashrc

# Verify
ros2 doctor
```

### 4.4 Install Python Dependencies (Offline)

```bash
pip install --no-index --find-links=/mnt/j5bundle/pip_wheels \
  pyserial numpy scipy transforms3d smbus2 \
  websocket-client sounddevice faster-whisper openwakeword

# DepthAI (may need source build on Jetson if no wheel)
pip install --no-index --find-links=/mnt/j5bundle/pip_wheels depthai \
  || pip install /mnt/j5bundle/pip_wheels/depthai-*.tar.gz
```

### 4.5 Build the ROS 2 Workspace

```bash
mkdir -p ~/ros2_ws/src
cd ~/ros2_ws/src

# Extract repos from USB
tar xzf /mnt/j5bundle/ros2_src/johnny5.tar.gz
tar xzf /mnt/j5bundle/ros2_src/ldlidar_stl_ros2.tar.gz
tar xzf /mnt/j5bundle/ros2_src/depthai-ros.tar.gz

# Symlink Johnny-5 ROS packages into workspace
ln -s ~/ros2_ws/src/johnny5/jetson/ros2        ~/ros2_ws/src/balance_bridge
ln -s ~/ros2_ws/src/johnny5/johnny5_bringup    ~/ros2_ws/src/
ln -s ~/ros2_ws/src/johnny5/johnny5_description ~/ros2_ws/src/
ln -s ~/ros2_ws/src/johnny5/johnny5_sensor_fusion ~/ros2_ws/src/

# Build (skip Gazebo if simulation not needed on Jetson)
cd ~/ros2_ws
colcon build \
  --packages-skip johnny5_gazebo \
  --cmake-args -DCMAKE_BUILD_TYPE=Release \
  2>&1 | tee build.log

# Source workspace
echo "source ~/ros2_ws/install/setup.bash" >> ~/.bashrc
source ~/.bashrc

# Verify
ros2 pkg list | grep johnny
# Expected: balance_bridge  johnny5_bringup  johnny5_description  johnny5_sensor_fusion
```

### 4.6 Install udev Rules & Add User to Groups

```bash
sudo cp /mnt/j5bundle/udev/*.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules && sudo udevadm trigger

# Add user to required groups
sudo usermod -aG dialout robot
sudo usermod -aG plugdev robot
# Log out and back in for group changes to take effect
```

### 4.7 Verify LiDAR

```bash
# Plug in LiDAR
lsusb | grep 10c4          # CP210x USB-UART bridge
ls -la /dev/ldlidar        # symlink should exist

# Test data flow
ros2 launch ldlidar_stl_ros2 ld19.launch.py &
sleep 5
ros2 topic hz /scan
# Expected: ~10.0 Hz
ros2 topic echo /scan --no-arr | grep frame_id
# Expected: frame_id: laser
```

### 4.8 Verify OAK-D Pro

```bash
# Plug in OAK-D Pro
lsusb | grep 03e7          # Myriad X

python3 -c "
import depthai as dai
devices = dai.Device.getAllAvailableDevices()
print('Devices found:', devices)
assert len(devices) > 0, 'No OAK-D detected!'
print('OAK-D OK')
"
```

### 4.9 Verify Teensy / Balance Bridge

```bash
# Plug in Teensy (must have firmware flashed)
ls /dev/ttyACM*            # expect /dev/ttyACM0

python3 ~/ros2_ws/src/johnny5/jetson/jetson_bridge.py
# Type "s" to see state
# Type "q" to quit
# Expected: connected: True, roll: <some angle>
```

### 4.10 Verify ReSpeaker Microphone

```bash
# Plug in ReSpeaker
arecord -l
# Look for: USB Audio or USB PnP Sound Device

# Record 3-second test
arecord -D plughw:X,0 -f S16_LE -r 16000 -c 1 -d 3 /tmp/test.wav
aplay /tmp/test.wav

# Set as default (replace X with card number from arecord -l)
echo "defaults.pcm.card X" | sudo tee -a /etc/asound.conf
echo "defaults.ctl.card X" | sudo tee -a /etc/asound.conf
```

### 4.11 Install AI Models

```bash
# Whisper
mkdir -p ~/.cache/whisper
cp -r /mnt/j5bundle/models/whisper_cache/* ~/.cache/whisper/

# LLM weights
mkdir -p ~/models/llm
cp /mnt/j5bundle/models/llm/Llama-3.2-3B-Instruct-Q4_K_M.gguf ~/models/llm/

# openWakeWord
mkdir -p ~/.local/share/openwakeword
cp -r /mnt/j5bundle/models/openwakeword/* ~/.local/share/openwakeword/

# llama.cpp server binary
mkdir -p ~/models/llama_cpp
cp /mnt/j5bundle/models/llama_cpp/llama-server ~/models/llama_cpp/
chmod +x ~/models/llama_cpp/llama-server

# Test Whisper
python3 -c "
import faster_whisper
model = faster_whisper.WhisperModel('small.en', device='cuda', compute_type='float16')
segs, _ = model.transcribe('/tmp/test.wav')
print([s.text for s in segs])
"
```

---

## 5. Headless Operation — SSH Workflow

### 5.1 SSH Config (on your laptop)

```
# ~/.ssh/config on your laptop
Host jetson
    HostName 192.168.1.200
    User robot
    ForwardX11 yes
```

```bash
ssh jetson
```

### 5.2 tmux — Keep Sessions Alive

Always use tmux. If SSH drops, everything keeps running.

```bash
tmux new-session -s robot       # create session
tmux attach -t robot            # reattach after disconnect

# Inside tmux (prefix = Ctrl+B)
#   c    — new window
#   0-9  — switch windows
#   "    — split horizontal
#   %    — split vertical
#   d    — detach (session keeps running)
```

**Recommended 4-window layout:**

| Window | Purpose |
|--------|---------|
| `0: bridge` | balance_bridge.launch.py |
| `1: sensors` | LiDAR + OAK-D drivers |
| `2: nav` | SLAM Toolbox + Nav2 |
| `3: monitor` | `ros2 topic echo`, logs, `tegrastats` |

### 5.3 Systemd Auto-Start Service

```bash
sudo tee /etc/systemd/system/johnny5.service << 'EOF'
[Unit]
Description=Johnny-5 Balance Bridge
After=network.target

[Service]
User=robot
WorkingDirectory=/home/robot
ExecStart=/bin/bash -c "source /opt/ros/humble/setup.bash && \
  source /home/robot/ros2_ws/install/setup.bash && \
  ros2 launch balance_bridge balance_bridge.launch.py"
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable johnny5
sudo systemctl start johnny5

# Check status
sudo systemctl status johnny5
journalctl -u johnny5 -f
```

---

## 6. Launching the Full Robot Stack

### 6.1 Startup Order

Always start layers in this order — each depends on the one above it:

1. `balance_bridge.launch.py` — Teensy bridge + twist_mux + esp32_joy_node
2. `sensors.launch.py` — LDROBOT LiDAR driver
3. `sensor_fusion.launch.py` — EKF (needs `/odom` + `/imu/data`)
4. `slam.launch.py` — SLAM Toolbox (needs `/scan`)
5. `navigation.launch.py` — Nav2 (needs `/map` + `/odometry/filtered`)

### 6.2 Launch Commands

```bash
# Source environment first
source /opt/ros/humble/setup.bash
source ~/ros2_ws/install/setup.bash

# Option A: Full launch with SLAM (mapping mode)
ros2 launch johnny5_bringup robot.launch.py \
  enable_slam:=true \
  enable_nav2:=false \
  use_rviz:=false

# Option B: Full launch with Nav2 (using saved map)
ros2 launch johnny5_bringup robot.launch.py \
  enable_slam:=false \
  enable_nav2:=true \
  use_rviz:=false

# Option C: Balance bridge only (development/tuning)
ros2 launch balance_bridge balance_bridge.launch.py \
  device:=/dev/ttyACM0 \
  watchdog_s:=0.5 \
  debug:=false
```

### 6.3 Remote Visualization (Laptop)

```bash
# Option A: RViz over SSH (X forwarding, requires ForwardX11 in ssh config)
ssh -X jetson
rviz2 -d ~/ros2_ws/src/johnny5/johnny5_description/rviz/johnny5.rviz

# Option B: Foxglove Studio (browser-based, no ROS install on laptop)
# On Jetson:
ros2 run rosbridge_server rosbridge_websocket
# Open in browser: https://studio.foxglove.dev
# Connect → WebSocket → ws://192.168.1.200:9090
```

### 6.4 Save a SLAM Map

```bash
ros2 service call /slam_toolbox/save_map slam_toolbox/srv/SaveMap \
  "{name: {data: '/home/robot/maps/mymap'}}"
# Creates: ~/maps/mymap.yaml and ~/maps/mymap.pgm
```

---

## 7. Sensor Bring-Up & Verification

### 7.1 Teensy / Balance Bridge

| Check | Command | Expected |
|-------|---------|---------|
| Teensy appears | `ls /dev/ttyACM*` | `/dev/ttyACM0` |
| Bridge connects | `python3 ~/ros2_ws/src/johnny5/jetson/jetson_bridge.py` then type `s` | `connected: True` |
| Roll topic live | `ros2 topic echo /imu/roll` | Float32 values at 20 Hz |
| Odometry updates | `ros2 topic echo /odom --no-arr` | pose.position changes when moved |
| cmd_vel reaches Teensy | `ros2 topic pub /cmd_vel_joy geometry_msgs/Twist '{linear: {x: 0.1}}'` | Robot tilts forward |

### 7.2 LiDAR (LDROBOT STL-19P)

| Check | Command | Expected |
|-------|---------|---------|
| Device detected | `lsusb \| grep 10c4` | CP2102 USB-UART Bridge |
| Symlink exists | `ls -la /dev/ldlidar` | symlink to ttyUSB* |
| Launch driver | `ros2 launch ldlidar_stl_ros2 ld19.launch.py` | "ldlidar communication is normal" |
| Scan rate | `ros2 topic hz /scan` | ~10.0 Hz |
| Point count | `ros2 topic echo /scan \| grep -c ranges` | 502 values |
| Frame ID correct | `ros2 topic echo /scan \| grep frame_id` | `frame_id: laser` |

### 7.3 OAK-D Pro Camera

| Check | Command | Expected |
|-------|---------|---------|
| Device detected | `python3 -c "import depthai as dai; print(dai.Device.getAllAvailableDevices())"` | List with 1+ devices |
| USB recognized | `lsusb \| grep 03e7` | Movidius / Intel Myriad X |
| ROS node starts | `ros2 run depthai_ros_driver camera_node` | No errors |
| Topics published | `ros2 topic list \| grep camera` | `/camera/rgb/image_raw`, etc. |

### 7.4 ReSpeaker Microphone

| Check | Command | Expected |
|-------|---------|---------|
| Listed as device | `arecord -l` | USB Audio listed |
| Records audio | `arecord -D plughw:X,0 -f S16_LE -r 16000 -c 1 -d 3 /tmp/t.wav` | File created |
| Whisper transcribes | `python3 -c "import faster_whisper; m=faster_whisper.WhisperModel('small.en'); segs,_=m.transcribe('/tmp/t.wav'); print([s.text for s in segs])"` | Text output |

### 7.5 ESP32 / PS3 Controller

| Check | Command | Expected |
|-------|---------|---------|
| ESP32 appears | `ls /dev/ttyUSB*` | `/dev/ttyUSB0` |
| Flash firmware | Arduino IDE → `esp32/ps3_bridge/ps3_bridge.ino` | Upload success |
| JSON output | `minicom -b 115200 -D /dev/ttyUSB0` | JSON packets when PS3 connected |
| Joy topic | `ros2 run balance_bridge esp32_joy_node` then `ros2 topic echo /joy` | Joy messages |

---

## 8. Debugging Guide

### 8.1 ROS 2 General Diagnostics

```bash
# Running nodes
ros2 node list

# All topics with types
ros2 topic list -t

# TF tree (outputs frames.pdf)
ros2 run tf2_tools view_frames
# scp frames.pdf to laptop to view

# Full system health check
ros2 doctor

# Node parameters
ros2 param list /slam_toolbox

# Topic bandwidth
ros2 topic bw /scan

# Topic delay
ros2 topic delay /odom
```

### 8.2 TF Tree Problems (Most Common Nav2 Failure)

Nav2 and SLAM Toolbox **will not start** if the TF chain is broken. Required chain: `map → odom → base_link → laser`

```bash
# Check full chain
ros2 run tf2_ros tf2_echo map base_link
# If this fails, the chain is broken

# Check individual transforms
ros2 run tf2_ros tf2_echo odom base_link
ros2 run tf2_ros tf2_echo base_link laser

# If base_link → laser is missing, add static TF to launch file:
# Node(
#   package='tf2_ros',
#   executable='static_transform_publisher',
#   arguments=['0.1', '0', '0.1', '0', '0', '0', 'base_link', 'laser'],
# )

# If odom → base_link is missing:
# balance_bridge_node is not running or not connected to Teensy

# TF latency check (should be < 0.1 s)
ros2 run tf2_ros tf2_monitor
```

> **Clock sync warning (air-gap):** If Jetson clock is wrong, TF transforms are rejected with `"Lookup would require extrapolation into the past"`. Fix: `sudo date -s "YYYY-MM-DD HH:MM:SS"`. For offline systems, never use `use_sim_time:=true` unless replaying a rosbag.

### 8.3 LiDAR Debugging

```bash
# Not detected
lsusb | grep 10c4            # Silicon Labs CP210x
dmesg | grep tty             # kernel log: device attachment
ls /dev/ttyUSB*              # fallback if udev not applied

# Permission denied
sudo chmod 666 /dev/ldlidar
# Permanent: verify udev rule in /etc/udev/rules.d/99-ldlidar.rules

# /scan not publishing
ros2 run ldlidar_stl_ros2 ldlidar_stl_ros2_node --ros-args \
  -p port_name:=/dev/ldlidar -p port_baudrate:=230400
# Check output for: "ldlidar communication is normal"

# Wrong frame_id (SLAM fails)
ros2 topic echo /scan | head -10   # frame_id must be "laser"
# Must match static TF: base_link → laser

# LiDAR tilt check
# Mounting must be ≤2° from horizontal
# Too much tilt → scans hits floor/ceiling → SLAM fails to match
```

### 8.4 SLAM Toolbox Debugging

```bash
# SLAM not building map — check prerequisites in order:
ros2 topic hz /scan           # 1. /scan must be ~10 Hz
ros2 topic hz /odom           # 2. /odom must be publishing
ros2 run tf2_ros tf2_echo map base_link  # 3. TF chain must be complete

# Map stops updating
ros2 topic echo /odom --no-arr    # confirm pose changes when moving

# Loop closure not working (small room)
# Edit slam_params.yaml:
#   loop_match_minimum_chain_size: 5  (default 10 — reduce for small spaces)

# SLAM uses too much CPU
# Edit slam_params.yaml:
#   minimum_travel_distance: 0.5   (increase — update less often)
#   minimum_travel_heading: 0.5

# Save map
ros2 service call /slam_toolbox/save_map slam_toolbox/srv/SaveMap \
  "{name: {data: '/home/robot/maps/mymap'}}"
```

### 8.5 Balance Bridge / Teensy Debugging

```bash
# No /dev/ttyACM0
dmesg | grep tty             # "ttyACM0: USB ACM device"
lsusb | grep "16c0"          # Teensy VID
# If missing: verify firmware is flashed (LED should blink slowly)

# Bridge connects but data is wrong
python3 ~/ros2_ws/src/johnny5/jetson/jetson_bridge.py
# Type "s" — if roll=0.0, IMU may not be initializing
# Attach laptop directly to Teensy USB for serial debug output

# /odom not updating
ros2 topic hz /odom           # expect 20 Hz
ros2 node list | grep balance # balance_bridge_node must be running

# Watchdog keeps halting
ros2 topic hz /cmd_vel        # must be > 0 if joy/nav are sending commands

# IMU values wrong (unexpected angles)
# IMU is mounted upside-down — firmware compensates:
#   pitch = -pitch
#   roll += 180 (with wrap)
# If still wrong, verify BNO085 physical orientation in mount

# VEL SIGN MISMATCH warnings
# See teensy_balance_cascaded/TUNING_RECOMMENDATIONS.md
# Check RIGHT_VELOCITY_SIGN matches RIGHT_MOTOR_DIRECTION_SIGN
```

### 8.6 Nav2 Debugging

```bash
# Nav2 fails to start
ros2 lifecycle_manager list   # all nav2 nodes should be active/configured

# "Waiting for costmap to be ready"
ros2 topic hz /scan           # /scan must be publishing
ros2 topic echo /map --no-arr # /map must exist (from SLAM)

# Robot does not follow path
ros2 topic echo /plan         # is a path being computed?
ros2 topic echo /cmd_vel      # is velocity being sent?

# "Goal rejected" (unknown space)
# The goal is in an unmapped area
# Robot must drive to map that area first
# Or: in nav2_params.yaml set allow_unknown: True in planner_server

# Costmap inflation too large
# Edit nav2_params.yaml: inflation_radius: 0.35 (reduce from 0.60)

# Robot oscillates or spins
# max_vel_x too high — keep at 0.5 m/s for balance robot
# Increase xy_goal_tolerance: 0.40 (more tolerant goal acceptance)
```

### 8.7 Offline apt / pip Errors

```bash
# "Unable to locate package"
# Re-index the local deb repo
cd /mnt/j5bundle/debs
dpkg-scanpackages . /dev/null | gzip -9c > Packages.gz
sudo apt update

# "Dependency not satisfiable"
# Missing .deb in bundle — on internet machine:
apt-get download --download-only <missing-package>
# Transfer to USB, re-run dpkg-scanpackages

# pip "No matching distribution found"
# No aarch64 wheel — on internet machine (arm64 docker):
pip download --platform linux_aarch64 <package>
# Transfer wheel to pip_wheels/

# Build from source (no binary wheel)
pip install /mnt/j5bundle/pip_wheels/<package>-*.tar.gz
```

### 8.8 colcon Build Failures

```bash
# Clean and retry single package
cd ~/ros2_ws
rm -rf build/balance_bridge install/balance_bridge
colcon build --packages-select balance_bridge

# "ImportError: No module named 'rclpy'"
# ROS 2 not sourced — check .bashrc has both:
source /opt/ros/humble/setup.bash
source ~/ros2_ws/install/setup.bash

# CMake cannot find package
sudo apt install ros-humble-<package>  # install from local cache

# Out of memory during build
colcon build --parallel-workers 2     # reduce parallel jobs

# Package not found after build
ls ~/ros2_ws/src/  # confirm symlinks exist and are not broken
ls -la ~/ros2_ws/src/balance_bridge   # should resolve to johnny5/jetson/ros2
```

---

## 9. Demo Wave Sequencing & Exit Criteria

Based on `docs/delivery/5/tasks.md` (PBI-5).

| Wave | PBI | Demo | Hardware | Exit Criteria |
|------|-----|------|---------|--------------|
| 0 | Foundation | Balance + serial + /odom | All connected | `ros2 topic echo /imu/roll` returns live data. Robot balances on floor. |
| 1a | PBI-15 | Safety + Recovery | Teensy | Tip past 45° → motors cut → `/balance/safety_state` publishes `FALLEN` |
| 1b | PBI-12 | Web Teleoperation | OAK-D, network | Browser joystick drives robot. Camera stream visible in browser. |
| 2a | PBI-16 | SLAM (LiDAR) | LiDAR | `/map` grows as robot is driven around room. Map saved successfully. |
| 2b | PBI-10 | Conversational Companion | ReSpeaker, GPU | "go forward" moves robot. "stop" halts. Response latency < 2 s. |
| 3a | PBI-14 | Wake Word | ReSpeaker | "Hey Johnny" activates 5-second listen window. >90% detection rate. False positive < 1/min. |
| 3b | PBI-8 | Vision Autonomy (Nav2) | LiDAR, OAK-D | Robot autonomously navigates 5 m × 5 m loop 3× without intervention. |
| 3c | PBI-9 | Human Following | OAK-D | Robot follows person at 1–1.5 m. Responds to STOP gesture. |
| 4 | PBI-11 | Object Recognition | OAK-D | Identifies 5 objects (bottle, chair, person, box, phone) at >90% accuracy. |

---

## 10. Quick Reference Card

### Most-Used Commands

| Task | Command |
|------|---------|
| SSH to Jetson | `ssh robot@192.168.1.200` |
| Attach tmux session | `tmux attach -t robot` |
| Full launch (SLAM) | `ros2 launch johnny5_bringup robot.launch.py enable_slam:=true use_rviz:=false` |
| Full launch (Nav2) | `ros2 launch johnny5_bringup robot.launch.py enable_nav2:=true use_rviz:=false` |
| Balance bridge only | `ros2 launch balance_bridge balance_bridge.launch.py` |
| LiDAR only | `ros2 launch johnny5_bringup sensors.launch.py` |
| Save SLAM map | `ros2 service call /slam_toolbox/save_map slam_toolbox/srv/SaveMap "{name: {data: '/home/robot/maps/map1'}}"` |
| Drive with keyboard | `ros2 run teleop_twist_keyboard teleop_twist_keyboard --ros-args -r /cmd_vel:=/cmd_vel_joy` |
| Check all topics | `ros2 topic list -t` |
| TF tree | `ros2 run tf2_tools view_frames` |
| System health | `ros2 doctor` |
| GPU usage | `tegrastats` |
| Restart service | `sudo systemctl restart johnny5` |
| Follow service logs | `journalctl -u johnny5 -f` |

### Key File Locations

| Item | Path |
|------|------|
| Workspace root | `~/ros2_ws/` |
| Johnny-5 repo | `~/ros2_ws/src/johnny5/` |
| Balance bridge ROS node | `~/ros2_ws/src/johnny5/jetson/ros2/balance_bridge/` |
| Jetson bridge (standalone) | `~/ros2_ws/src/johnny5/jetson/jetson_bridge.py` |
| Teensy firmware | `~/ros2_ws/src/johnny5/teensy_balance_cascaded/` |
| Launch files | `~/ros2_ws/src/johnny5/johnny5_bringup/launch/` |
| Nav2 params | `~/ros2_ws/src/johnny5/johnny5_bringup/config/nav2_params.yaml` |
| SLAM params | `~/ros2_ws/src/johnny5/johnny5_bringup/config/slam_params.yaml` |
| EKF config | `~/ros2_ws/src/johnny5/johnny5_sensor_fusion/config/ekf.yaml` |
| URDF robot model | `~/ros2_ws/src/johnny5/johnny5_description/urdf/` |
| Saved maps | `~/maps/` |
| LLM weights | `~/models/llm/` |
| Whisper cache | `~/.cache/whisper/` |
| openWakeWord models | `~/.local/share/openwakeword/` |
| udev rules | `/etc/udev/rules.d/` |
| ROS 2 install | `/opt/ros/humble/` |
| Systemd service | `/etc/systemd/system/johnny5.service` |

### USB Device IDs (for lsusb / udev)

| Device | VID:PID | Symlink / Path |
|--------|---------|----------------|
| LDROBOT LiDAR (CP2102) | `10c4:ea60` | `/dev/ldlidar` |
| OAK-D Pro (Myriad X) | VID `03e7` | `/dev/bus/usb/...` |
| Teensy 4.1 | VID `16c0` | `/dev/ttyACM0` |
| ESP32 (CP210x) | `10c4:ea60` | `/dev/ttyUSB0` |
| ESP32 (CH340) | VID `1a86` | `/dev/ttyUSB0` |
| ReSpeaker Mic Array | USB Audio class | `arecord -l` card X |

### Environment Variables (add to ~/.bashrc)

```bash
source /opt/ros/humble/setup.bash
source ~/ros2_ws/install/setup.bash
export ROS_DOMAIN_ID=0
export BALANCE_BRIDGE_REPO_ROOT=~/ros2_ws/src/johnny5
```

---

*End of guide — keep this file in `docs/` for Cursor context and on the USB drive for offline Jetson reference.*
