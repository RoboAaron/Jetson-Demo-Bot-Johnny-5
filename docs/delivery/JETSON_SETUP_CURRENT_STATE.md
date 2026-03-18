# Jetson Setup — Current State

**Last updated:** 2026-03-18 (verify re-run with LiDAR + depth camera plugged in)  
**Branch:** `feature/jetson-setup`  
**Target:** Jetson AGX Orin Dev Kit, Ubuntu 22.04, ROS 2 Humble, offline bundle at `/mnt/j5bundle`

---

## 1. What’s Working

### Bundle on SD card (`/mnt/j5bundle`)

- **Scripts 01–08** present and executable. Path used by scripts: `/mnt/j5bundle` (no underscore).
- **Script 04** (build ROS 2 workspace):
  - Uses `set +u` when sourcing ROS (avoids unbound variable errors).
  - Removes `depthai-ros/COLCON_IGNORE` so the OAK-D driver is built with the workspace.
- **Script 08** (verify install):
  - Uses `set +u` and pipe-safe package/topic checks (no BrokenPipeError when grepping `ros2 pkg list` / `ros2 topic list`).
- **LiDAR (ldlidar_stl_ros2):**
  - `ros2_src/ldlidar_stl_ros2.tar.gz` contains a patched `ld19.launch.py` that:
    - Declares launch argument `port_name` with default **`/dev/ldlidar`**.
    - Uses `LaunchConfiguration('port_name')` so the node respects the udev symlink.
  - No more hard-coded `/dev/ttyUSB0`; works with udev rule that creates `/dev/ldlidar` for CP210x (LiDAR).

### Jetson install (already run)

- **01–07:** Run successfully (offline apt, ROS 2, Python deps, workspace build, udev, models, systemd).
- **Workspace:** `/root/ros2_ws` (when using sudo). Built packages: `balance_bridge`, `johnny5_bringup`, `johnny5_description`, `johnny5_sensor_fusion`, `ldlidar_stl_ros2`.
- **Udev:** LiDAR (`99-ldlidar.rules` → `/dev/ldlidar`), OAK-D (Movidius), Teensy, ESP32 rules installed.
- **Systemd:** `johnny5.service` enabled; starts balance_bridge with ROS 2 workspace sourced.

### Verify (08) — last run (LiDAR + depth camera plugged in)

- **Result:** 5 PASS / 2 FAIL.
- **5 PASS:** ros2 doctor, johnny packages visible, balance_bridge package visible, **LiDAR / CP210x visible in lsusb**, **OAK-D / Myriad X visible in lsusb**.
- **2 FAIL (expected without Teensy):** Teensy visible in lsusb, Bridge topics visible.
- **Hardware in lsusb:** 10c4:ea60 (CP210x LiDAR), 03e7 (MyriadX OAK-D) — both detected when connected.

---

## 2. What’s Pending / Conditional

### depthai-ros (OAK-D driver)

- **In bundle:** Script 04 now enables building depthai-ros (removes `COLCON_IGNORE`).
- **On current Jetson image:** `depthai_ros_driver` is **not** yet built because the workspace was built with the *previous* 04 script (before the update). To get the OAK-D ROS driver on this Jetson, re-run script 04:
  ```bash
  sudo /mnt/j5bundle/scripts/04_build_ros2_workspace.sh
  ```
  Then `ros2 pkg list | grep depthai` should show `depthai_ros_driver` (and deps). If the build fails for missing system packages, add those to the offline bundle or install them as needed.

### LiDAR at runtime

- **Config:** Correct. Launch uses `/dev/ldlidar` by default; udev creates it for the CP210x LiDAR.
- **Verify 08:** “LiDAR / CP210x visible” only passes when the LiDAR is plugged in and shows up in `lsusb` (10c4). If it’s unplugged, that check fails as expected.

### Teensy and bridge topics

- “Teensy visible in lsusb” and “Bridge topics visible” pass only when the Teensy is connected and the bridge is running and publishing `/odom`, `/robot_state`, or `/imu/roll`.

---

## 3. Repo / Host Side

| Item | Location | Purpose |
|------|----------|--------|
| Bundle build | `build_j5_bundle.sh` | Builds offline bundle; includes LiDAR patch and 04/08 script content. |
| In-place bundle update | `scripts/update_bundle_jetson_setup.sh` | Updates existing bundle: patch LiDAR tarball, overwrite script 04. No downloads. |
| Host verify | `verify_bundle.sh` | Optional: run on host to check bundle layout before copying to SD. |
| Jetson helper | `scripts/run_j5_bundle_on_jetson.sh` | Copy to Jetson and run with sudo to execute bundle scripts. |

---

## 4. Quick reference

- **Bundle path on Jetson:** `/mnt/j5bundle` (scripts and README assume this).
- **ROS 2 workspace:** `$HOME/ros2_ws` (e.g. `/root/ros2_ws` when using sudo).
- **Source before ROS commands:**  
  `source /opt/ros/humble/setup.bash` and  
  `source ~/ros2_ws/install/setup.bash` (or `/root/ros2_ws/install/setup.bash` for root).
- **LiDAR:** `ros2 launch ldlidar_stl_ros2 ld19.launch.py` (uses `/dev/ldlidar` by default; override with `port_name:=/dev/ttyUSB1` if needed).
- **OAK-D (after 04 re-run):** `ros2 launch depthai_ros_driver driver.launch.py`.
- **Full verify:** `sudo /mnt/j5bundle/scripts/08_verify_install.sh`.

---

## 5. Summary

The Jetson portion is in a good state: bundle on SD has the fixed 04 and 08 scripts and the patched LiDAR launch. With **LiDAR and OAK-D (depth camera) connected**, verify (08) gives **5 PASS / 2 FAIL**. The two remaining failures are expected without the Teensy: “Teensy visible in lsusb” and “Bridge topics visible.” Once the Teensy is connected and the bridge is running, 08 should reach 7 PASS. Re-running script 04 on the Jetson will add the depthai-ros (OAK-D) driver to the workspace if you need the ROS 2 OAK-D nodes.
