#!/usr/bin/env bash
# ============================================================================
# sensor_check_ros.sh — ROS 2 topic-level sensor verification
#
# Usage:
#   bash scripts/sensor_check_ros.sh all
#   bash scripts/sensor_check_ros.sh teensy
#   bash scripts/sensor_check_ros.sh lidar
#   bash scripts/sensor_check_ros.sh oak
#   bash scripts/sensor_check_ros.sh combo bridge+lidar
#   bash scripts/sensor_check_ros.sh combo all_sensors
#
# What it does:
#   1. Sources ROS 2 + workspace
#   2. Launches the requested sensor driver(s) in background
#   3. Waits for startup (configurable)
#   4. Checks topic existence and publish rate
#   5. Kills background processes
#   6. Reports PASS/FAIL
#
# Prerequisites:
#   - sensor_check_hardware.sh passes (hardware detected)
#   - ROS 2 Humble installed, workspace built
#   - No other ROS nodes running on the same topics
# ============================================================================

set -uo pipefail

# ── Config ───────────────────────────────────────────────────────────────────
STARTUP_WAIT=10          # seconds to wait after launch before checking topics
TOPIC_HZ_DURATION=5      # seconds to measure topic rate
ROS_SETUP="/opt/ros/humble/setup.bash"
WS_SETUP="$HOME/Projects/robotics/ros2_ws/install/setup.bash"

# ── Colours ──────────────────────────────────────────────────────────────────
GREEN='\033[0;32m'; RED='\033[0;31m'; YELLOW='\033[1;33m'
CYAN='\033[0;36m'; NC='\033[0m'

PASS_COUNT=0; FAIL_COUNT=0; WARN_COUNT=0
BG_PIDS=()

pass()  { PASS_COUNT=$((PASS_COUNT+1)); echo -e "  ${GREEN}PASS${NC}  $1"; }
fail()  { FAIL_COUNT=$((FAIL_COUNT+1)); echo -e "  ${RED}FAIL${NC}  $1"; }
warn()  { WARN_COUNT=$((WARN_COUNT+1)); echo -e "  ${YELLOW}WARN${NC}  $1"; }
info()  { echo -e "  ${CYAN}INFO${NC}  $1"; }

# ── Source ROS ───────────────────────────────────────────────────────────────
source_ros() {
    set +u  # ROS setup scripts use unbound variables
    if [ -f "$ROS_SETUP" ]; then source "$ROS_SETUP"; fi
    if [ -f "$WS_SETUP" ]; then source "$WS_SETUP"; fi
    set -u
}

# ── Cleanup ──────────────────────────────────────────────────────────────────
cleanup() {
    info "Cleaning up background processes..."
    for pid in "${BG_PIDS[@]}"; do
        if kill -0 "$pid" 2>/dev/null; then
            kill "$pid" 2>/dev/null || true
            wait "$pid" 2>/dev/null || true
        fi
    done
    BG_PIDS=()
}
trap cleanup EXIT

# ── Launch helpers ───────────────────────────────────────────────────────────
launch_in_background() {
    local label="$1"
    shift
    info "Launching: $label"
    "$@" > /tmp/sensor_check_${label}.log 2>&1 &
    local pid=$!
    BG_PIDS+=("$pid")
    info "  PID=$pid  log=/tmp/sensor_check_${label}.log"
}

wait_for_startup() {
    local secs="${1:-$STARTUP_WAIT}"
    info "Waiting ${secs}s for nodes to start..."
    sleep "$secs"
}

# ── Topic check helpers ──────────────────────────────────────────────────────
check_topic_exists() {
    local topic="$1"
    local label="$2"
    if ros2 topic list 2>/dev/null | grep -qF "$topic"; then
        pass "$label: topic $topic exists"
        return 0
    else
        fail "$label: topic $topic NOT found"
        return 1
    fi
}

check_topic_rate() {
    local topic="$1"
    local label="$2"
    local min_hz="$3"
    local max_hz="$4"

    local hz_output
    hz_output=$(timeout "$TOPIC_HZ_DURATION" ros2 topic hz "$topic" 2>/dev/null | tail -1 || true)

    if [ -z "$hz_output" ]; then
        fail "$label: topic $topic not publishing (no hz data in ${TOPIC_HZ_DURATION}s)"
        return 1
    fi

    local rate
    rate=$(echo "$hz_output" | grep -oP 'average rate: \K[0-9.]+' || echo "0")

    if [ -z "$rate" ] || [ "$rate" = "0" ]; then
        fail "$label: could not parse rate from: $hz_output"
        return 1
    fi

    # Compare using bc or awk
    local in_range
    in_range=$(awk "BEGIN { print ($rate >= $min_hz && $rate <= $max_hz) }")

    if [ "$in_range" = "1" ]; then
        pass "$label: $topic at ${rate} Hz (expected ${min_hz}–${max_hz})"
    else
        fail "$label: $topic at ${rate} Hz (expected ${min_hz}–${max_hz})"
    fi
}

# ── Test: Teensy / Balance Bridge ────────────────────────────────────────────
test_teensy() {
    echo ""
    echo "── Testing: Teensy / Balance Bridge ─────────────────────────"

    launch_in_background "bridge" \
        ros2 launch balance_bridge balance_bridge.launch.py
    wait_for_startup 12  # bridge needs time to connect to Teensy

    check_topic_exists "/imu/roll" "A1.4"
    check_topic_exists "/odom" "A1.5"
    check_topic_rate "/imu/roll" "A1.4-rate" 10 30
}

# ── Test: LiDAR ──────────────────────────────────────────────────────────────
test_lidar() {
    echo ""
    echo "── Testing: LiDAR (LDROBOT STL-19P) ────────────────────────"

    launch_in_background "lidar" \
        ros2 launch ldlidar_stl_ros2 ld19.launch.py
    wait_for_startup 8

    check_topic_exists "/scan" "A2.4"
    check_topic_rate "/scan" "A2.4-rate" 8 12

    # Check scan content (frame_id)
    local scan_echo
    scan_echo=$(timeout 3 ros2 topic echo /scan --once 2>/dev/null || true)
    if echo "$scan_echo" | grep -q "frame_id"; then
        local frame_id
        frame_id=$(echo "$scan_echo" | grep "frame_id" | head -1 | awk '{print $2}')
        pass "A2.5: /scan frame_id=$frame_id"
    else
        fail "A2.5: Could not read /scan content"
    fi
}

# ── Test: OAK-D ──────────────────────────────────────────────────────────────
test_oak() {
    echo ""
    echo "── Testing: OAK-D Pro (Depth Camera) ───────────────────────"

    # Check if depthai_ros_driver is built
    if ! ros2 pkg list 2>/dev/null | grep -q "depthai_ros_driver"; then
        fail "A3.3: depthai_ros_driver package not built"
        info "Fix: sudo /mnt/j5bundle/scripts/04_build_ros2_workspace.sh"
        return 1
    fi
    pass "A3.3: depthai_ros_driver package found"

    launch_in_background "oakd" \
        ros2 launch depthai_ros_driver driver.launch.py
    wait_for_startup 12  # OAK-D init can be slow

    # Discover actual topic names (namespace varies)
    local img_topics
    img_topics=$(ros2 topic list 2>/dev/null | grep -iE "rgb|image_raw|color" | head -3 || true)

    if [ -n "$img_topics" ]; then
        local first_topic
        first_topic=$(echo "$img_topics" | head -1)
        pass "A3.5: Image topics found: $(echo $img_topics | tr '\n' ' ')"
        check_topic_rate "$first_topic" "A3.6-rate" 5 35
    else
        # Try broader search
        local all_oak
        all_oak=$(ros2 topic list 2>/dev/null | grep -iE "oak|depth|camera" || true)
        if [ -n "$all_oak" ]; then
            warn "A3.5: Found OAK-D topics but no image_raw: $(echo $all_oak | tr '\n' ' ')"
        else
            fail "A3.5: No OAK-D topics found"
            info "Check /tmp/sensor_check_oakd.log for errors"
        fi
    fi
}

# ── Combo: Bridge + LiDAR ───────────────────────────────────────────────────
test_combo_bridge_lidar() {
    echo ""
    echo "── Joint Test: Bridge + LiDAR ──────────────────────────────"

    launch_in_background "bridge" \
        ros2 launch balance_bridge balance_bridge.launch.py
    launch_in_background "lidar" \
        ros2 launch ldlidar_stl_ros2 ld19.launch.py
    wait_for_startup 12

    check_topic_exists "/odom" "B1-odom"
    check_topic_exists "/scan" "B1-scan"
    check_topic_rate "/odom" "B1-odom-rate" 10 30
    check_topic_rate "/scan" "B1-scan-rate" 8 12

    # TF check: base_link -> laser
    local tf_out
    tf_out=$(timeout 3 ros2 run tf2_ros tf2_echo base_link laser 2>&1 || true)
    if echo "$tf_out" | grep -q "Translation"; then
        pass "B1-tf: base_link -> laser TF exists"
    else
        warn "B1-tf: base_link -> laser TF not found (add static_transform_publisher)"
    fi
}

# ── Combo: All sensors ──────────────────────────────────────────────────────
test_combo_all() {
    echo ""
    echo "── Joint Test: All Sensors ─────────────────────────────────"

    launch_in_background "bridge" \
        ros2 launch balance_bridge balance_bridge.launch.py
    launch_in_background "lidar" \
        ros2 launch ldlidar_stl_ros2 ld19.launch.py

    # Only launch OAK-D if the package is built
    if ros2 pkg list 2>/dev/null | grep -q "depthai_ros_driver"; then
        launch_in_background "oakd" \
            ros2 launch depthai_ros_driver driver.launch.py
    else
        info "Skipping OAK-D (depthai_ros_driver not built)"
    fi

    wait_for_startup 15

    check_topic_exists "/odom" "B4-odom"
    check_topic_exists "/scan" "B4-scan"
    check_topic_exists "/imu/roll" "B4-imu"

    local oak_topics
    oak_topics=$(ros2 topic list 2>/dev/null | grep -iE "oak|depth|camera" | head -1 || true)
    if [ -n "$oak_topics" ]; then
        pass "B4-oakd: OAK-D topics present"
    else
        warn "B4-oakd: No OAK-D topics (skipped or not built)"
    fi

    # Full topic dump for documentation
    echo ""
    info "Full topic list:"
    ros2 topic list 2>/dev/null | while read -r t; do echo "    $t"; done

    # TF tree snapshot
    info "TF frames (saving to /tmp/sensor_check_frames.pdf):"
    timeout 5 ros2 run tf2_tools view_frames 2>/dev/null || true
    if [ -f frames.pdf ]; then
        mv frames.pdf /tmp/sensor_check_frames.pdf
        info "TF tree saved to /tmp/sensor_check_frames.pdf"
    fi
}

# ── Main ─────────────────────────────────────────────────────────────────────
usage() {
    echo "Usage: $0 <mode> [submode]"
    echo ""
    echo "Modes:"
    echo "  all              Run all individual sensor tests sequentially"
    echo "  teensy           Test Teensy / balance bridge only"
    echo "  lidar            Test LiDAR only"
    echo "  oak              Test OAK-D only"
    echo "  combo <type>     Joint tests:"
    echo "                     bridge+lidar    Bridge + LiDAR together"
    echo "                     all_sensors     All sensors together"
    echo ""
    echo "Examples:"
    echo "  $0 all"
    echo "  $0 teensy"
    echo "  $0 combo bridge+lidar"
    echo "  $0 combo all_sensors"
    exit 1
}

main() {
    local mode="${1:-}"
    local submode="${2:-}"

    if [ -z "$mode" ]; then usage; fi

    source_ros

    echo ""
    echo "════════════════════════════════════════════════════════════════"
    echo "  Johnny-5 ROS 2 Sensor Check — mode: $mode $submode"
    echo "  $(date '+%Y-%m-%d %H:%M:%S')"
    echo "════════════════════════════════════════════════════════════════"

    case "$mode" in
        all)
            test_teensy; cleanup
            test_lidar;  cleanup
            test_oak;    cleanup
            ;;
        teensy) test_teensy ;;
        lidar)  test_lidar  ;;
        oak)    test_oak    ;;
        combo)
            case "$submode" in
                bridge+lidar) test_combo_bridge_lidar ;;
                all_sensors)  test_combo_all ;;
                *) echo "Unknown combo: $submode"; usage ;;
            esac
            ;;
        *) usage ;;
    esac

    # Summary
    echo ""
    echo "════════════════════════════════════════════════════════════════"
    echo -e "  SUMMARY: ${GREEN}${PASS_COUNT} PASS${NC}  ${RED}${FAIL_COUNT} FAIL${NC}  ${YELLOW}${WARN_COUNT} WARN${NC}"
    echo "════════════════════════════════════════════════════════════════"
    echo ""

    if [ "$FAIL_COUNT" -gt 0 ]; then
        echo -e "  ${RED}Some tests failed. Check logs in /tmp/sensor_check_*.log${NC}"
        exit 1
    fi
}

main "$@"
