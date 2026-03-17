#!/usr/bin/env bash
# Run after build_j5_bundle.sh (or before copying to SD) to catch missing pieces.
# Usage: ./verify_bundle.sh [BUNDLE_ROOT]   (default: ~/j5_bundle)

set -u
BUNDLE_ROOT="${1:-${HOME}/j5_bundle}"
PASS=0
FAIL=0

check() {
  local msg="$1"; shift
  if "$@"; then echo "PASS: ${msg}"; PASS=$((PASS+1)); return 0; fi
  echo "FAIL: ${msg}"; FAIL=$((FAIL+1)); return 1
}

echo "=== Bundle verification: ${BUNDLE_ROOT} ==="

# Debs: ROS 2 must contain actual ros-humble packages (not just deps)
ROS2_DEBS=$(find "${BUNDLE_ROOT}/debs/ros2" -maxdepth 1 -name "ros-humble-*.deb" 2>/dev/null | wc -l)
check "debs/ros2 has ros-humble packages (${ROS2_DEBS} found, need >=20)" [ "${ROS2_DEBS}" -ge 20 ]
check "debs/ros2/Packages.gz exists" [ -f "${BUNDLE_ROOT}/debs/ros2/Packages.gz" ]

SYS_DEBS=$(find "${BUNDLE_ROOT}/debs/system" -maxdepth 1 -name "*.deb" 2>/dev/null | wc -l)
check "debs/system has .deb files (${SYS_DEBS} found, need >=10)" [ "${SYS_DEBS}" -ge 10 ]
check "debs/system/Packages.gz exists" [ -f "${BUNDLE_ROOT}/debs/system/Packages.gz" ]

# Python wheels
check "pip_wheels directory exists" [ -d "${BUNDLE_ROOT}/pip_wheels" ]
WHEELS=$(find "${BUNDLE_ROOT}/pip_wheels" -maxdepth 1 \( -name "*.whl" -o -name "*.tar.gz" \) 2>/dev/null | wc -l)
check "pip_wheels has wheels/tarballs (${WHEELS} found, need >=5)" [ "${WHEELS}" -ge 5 ]

# ROS 2 source tarballs
check "ros2_src/johnny5.tar.gz" [ -f "${BUNDLE_ROOT}/ros2_src/johnny5.tar.gz" ]
check "ros2_src/ldlidar_stl_ros2.tar.gz" [ -f "${BUNDLE_ROOT}/ros2_src/ldlidar_stl_ros2.tar.gz" ]
check "ros2_src/depthai-ros.tar.gz" [ -f "${BUNDLE_ROOT}/ros2_src/depthai-ros.tar.gz" ]

# Models
check "models/whisper_cache" [ -d "${BUNDLE_ROOT}/models/whisper_cache" ]
check "models/llm" [ -d "${BUNDLE_ROOT}/models/llm" ]
check "models/llama_cpp" [ -d "${BUNDLE_ROOT}/models/llama_cpp" ]
check "models/openwakeword" [ -d "${BUNDLE_ROOT}/models/openwakeword" ]

# Udev and scripts
UDEV_RULES=$(find "${BUNDLE_ROOT}/udev" -maxdepth 1 -name "*.rules" 2>/dev/null | wc -l)
check "udev/*.rules (${UDEV_RULES} found)" [ "${UDEV_RULES}" -ge 1 ]
SCRIPTS_N=$(find "${BUNDLE_ROOT}/scripts" -maxdepth 1 -name '[0-9][0-9]_*.sh' 2>/dev/null | wc -l)
check "scripts 01-08 (${SCRIPTS_N} found)" [ "${SCRIPTS_N}" -eq 8 ]
check "README.txt" [ -f "${BUNDLE_ROOT}/README.txt" ]

echo "=== RESULT: ${PASS} PASS / ${FAIL} FAIL ==="
if [ "${FAIL}" -gt 0 ]; then
  echo "Fix failures before copying bundle to SD. Re-run build_j5_bundle.sh if debs or indexes are missing."
  exit 1
fi
echo "Bundle looks complete. Safe to copy to SD and run scripts 01-08 on Jetson."
exit 0
