#!/usr/bin/env bash
# Update existing J5 bundle in place: LiDAR ld19 port_name patch and script 04 (depthai-ros + set +u).
# No downloads, no sudo. Usage: ./scripts/update_bundle_jetson_setup.sh [BUNDLE_ROOT]
# Default BUNDLE_ROOT: $HOME/j5_bundle

set -e
BUNDLE_ROOT="${1:-$HOME/j5_bundle}"
SRC="${BUNDLE_ROOT}/ros2_src"
TARBALL="${SRC}/ldlidar_stl_ros2.tar.gz"
DIR="${SRC}/ldlidar_stl_ros2"
LAUNCH="${DIR}/launch/ld19.launch.py"

echo "[update_bundle_jetson_setup] BUNDLE_ROOT=${BUNDLE_ROOT}"

# 1. Patch ldlidar_stl_ros2: ld19.launch.py port_name default /dev/ldlidar, then repack
if [[ -f "${TARBALL}" ]]; then
  if [[ ! -f "${LAUNCH}" ]]; then
    echo "[update_bundle_jetson_setup] Extracting ldlidar_stl_ros2.tar.gz..."
    (cd "${SRC}" && tar xzf ldlidar_stl_ros2.tar.gz)
  fi
  echo "[update_bundle_jetson_setup] Patching ld19.launch.py (port_name=/dev/ldlidar)..."
  cat > "${LAUNCH}" <<'LDPATCH'
#!/usr/bin/env python3
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node

'''
Parameter Description:
---
- Set laser scan directon:
  1. Set counterclockwise, example: {'laser_scan_dir': True}
  2. Set clockwise,        example: {'laser_scan_dir': False}
- Angle crop setting, Mask data within the set angle range:
  1. Enable angle crop fuction:
    1.1. enable angle crop,  example: {'enable_angle_crop_func': True}
    1.2. disable angle crop, example: {'enable_angle_crop_func': False}
  2. Angle cropping interval setting:
  - The distance and intensity data within the set angle range will be set to 0.
  - angle >= 'angle_crop_min' and angle <= 'angle_crop_max' which is [angle_crop_min, angle_crop_max], unit is degress.
    example:
      {'angle_crop_min': 135.0}
      {'angle_crop_max': 225.0}
      which is [135.0, 225.0], angle unit is degress.
'''

def generate_launch_description():
  port_arg = DeclareLaunchArgument(
      'port_name',
      default_value='/dev/ldlidar',
      description='Serial port for LiDAR (use udev symlink /dev/ldlidar for CP210x)'
  )

  # LDROBOT LiDAR publisher node
  ldlidar_node = Node(
      package='ldlidar_stl_ros2',
      executable='ldlidar_stl_ros2_node',
      name='LD19',
      output='screen',
      parameters=[
        {'product_name': 'LDLiDAR_LD19'},
        {'topic_name': 'scan'},
        {'frame_id': 'base_laser'},
        {'port_name': LaunchConfiguration('port_name')},
        {'port_baudrate': 230400},
        {'laser_scan_dir': True},
        {'enable_angle_crop_func': False},
        {'angle_crop_min': 135.0},
        {'angle_crop_max': 225.0}
      ]
  )

  # base_link to base_laser tf node
  base_link_to_laser_tf_node = Node(
    package='tf2_ros',
    executable='static_transform_publisher',
    name='base_link_to_base_laser_ld19',
    arguments=['0','0','0.18','0','0','0','base_link','base_laser']
  )

  ld = LaunchDescription()
  ld.add_action(port_arg)
  ld.add_action(ldlidar_node)
  ld.add_action(base_link_to_laser_tf_node)
  return ld
LDPATCH
  echo "[update_bundle_jetson_setup] Repacking ldlidar_stl_ros2.tar.gz..."
  (cd "${SRC}" && rm -f ldlidar_stl_ros2.tar.gz && tar czf ldlidar_stl_ros2.tar.gz ldlidar_stl_ros2)
  echo "[update_bundle_jetson_setup] LiDAR patch done."
else
  echo "[update_bundle_jetson_setup] No ${TARBALL} found; skipping LiDAR patch."
fi

# 2. Write updated 04_build_ros2_workspace.sh (set +u, enable depthai-ros build)
SCRIPT04="${BUNDLE_ROOT}/scripts/04_build_ros2_workspace.sh"
mkdir -p "${BUNDLE_ROOT}/scripts"
echo "[update_bundle_jetson_setup] Writing ${SCRIPT04}..."
cat > "${SCRIPT04}" <<'EOF'
#!/usr/bin/env bash
set +u
set -o pipefail
BUNDLE_ROOT="${1:-/mnt/j5bundle}"
WS="${HOME}/ros2_ws"
PASS_COUNT=0
FAIL_COUNT=0
pass(){ echo "PASS: $*"; PASS_COUNT=$((PASS_COUNT+1)); }
fail(){ echo "FAIL: $*"; FAIL_COUNT=$((FAIL_COUNT+1)); }

source /opt/ros/humble/setup.bash || { fail "Source /opt/ros/humble/setup.bash"; exit 1; }

mkdir -p "${WS}/src"
cd "${WS}/src" || exit 1

tar xzf "${BUNDLE_ROOT}/ros2_src/johnny5.tar.gz" && pass "Extracted johnny5.tar.gz" || fail "Extract johnny5.tar.gz"
tar xzf "${BUNDLE_ROOT}/ros2_src/ldlidar_stl_ros2.tar.gz" && pass "Extracted ldlidar_stl_ros2.tar.gz" || fail "Extract ldlidar_stl_ros2.tar.gz"
tar xzf "${BUNDLE_ROOT}/ros2_src/depthai-ros.tar.gz" && pass "Extracted depthai-ros.tar.gz" || fail "Extract depthai-ros.tar.gz"

rm -f "${WS}/src/depthai-ros/COLCON_IGNORE" && pass "Enabled depthai-ros build (OAK-D driver)" || true

ln -sfn "${WS}/src/johnny5/jetson/ros2" "${WS}/src/balance_bridge" && pass "Linked balance_bridge" || fail "Link balance_bridge"
ln -sfn "${WS}/src/johnny5/johnny5_bringup" "${WS}/src/johnny5_bringup" && pass "Linked johnny5_bringup" || fail "Link johnny5_bringup"
ln -sfn "${WS}/src/johnny5/johnny5_description" "${WS}/src/johnny5_description" && pass "Linked johnny5_description" || fail "Link johnny5_description"
ln -sfn "${WS}/src/johnny5/johnny5_sensor_fusion" "${WS}/src/johnny5_sensor_fusion" && pass "Linked johnny5_sensor_fusion" || fail "Link johnny5_sensor_fusion"

cd "${WS}" || exit 1
colcon build --packages-skip johnny5_gazebo --cmake-args -DCMAKE_BUILD_TYPE=Release \
  && pass "Built ROS 2 workspace" \
  || fail "Build ROS 2 workspace"

grep -qxF "source /opt/ros/humble/setup.bash" "${HOME}/.bashrc" || echo "source /opt/ros/humble/setup.bash" >> "${HOME}/.bashrc"
grep -qxF "source ${WS}/install/setup.bash" "${HOME}/.bashrc" || echo "source ${WS}/install/setup.bash" >> "${HOME}/.bashrc"
pass "Updated ~/.bashrc"

echo "RESULT: ${PASS_COUNT} PASS / ${FAIL_COUNT} FAIL"
exit $(( FAIL_COUNT > 0 ))
EOF
chmod +x "${SCRIPT04}"
echo "[update_bundle_jetson_setup] Script 04 updated (set +u, depthai-ros enabled)."
echo "[update_bundle_jetson_setup] Done."