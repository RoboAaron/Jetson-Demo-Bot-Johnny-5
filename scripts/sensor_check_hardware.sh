#!/usr/bin/env bash
# ============================================================================
# sensor_check_hardware.sh — Hardware-level sensor detection (no ROS required)
#
# Usage:  bash scripts/sensor_check_hardware.sh
# Output: PASS/FAIL per sensor, summary at end
# Runtime: < 2 seconds
#
# Checks lsusb, /dev nodes, and ALSA devices.  Does NOT launch any ROS nodes.
# Run this FIRST before any ROS-level tests.
# ============================================================================

set -euo pipefail

# ── Colours ──────────────────────────────────────────────────────────────────
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

PASS_COUNT=0
FAIL_COUNT=0
WARN_COUNT=0
RESULTS=()

pass()  { PASS_COUNT=$((PASS_COUNT+1));  RESULTS+=("${GREEN}PASS${NC}  $1"); echo -e "  ${GREEN}PASS${NC}  $1"; }
fail()  { FAIL_COUNT=$((FAIL_COUNT+1));  RESULTS+=("${RED}FAIL${NC}  $1");   echo -e "  ${RED}FAIL${NC}  $1"; }
warn()  { WARN_COUNT=$((WARN_COUNT+1));  RESULTS+=("${YELLOW}WARN${NC}  $1"); echo -e "  ${YELLOW}WARN${NC}  $1"; }
info()  { echo -e "  ${CYAN}INFO${NC}  $1"; }

LSUSB_CACHE=$(lsusb 2>/dev/null || true)

echo ""
echo "════════════════════════════════════════════════════════════════"
echo "  Johnny-5 Hardware Sensor Check"
echo "  $(date '+%Y-%m-%d %H:%M:%S')"
echo "════════════════════════════════════════════════════════════════"

# ── A1: Teensy 4.1 ──────────────────────────────────────────────────────────
echo ""
echo "── A1: Teensy 4.1 ──────────────────────────────────────────────"

# A1.1 — lsusb
if echo "$LSUSB_CACHE" | grep -qi "16c0"; then
    TEENSY_LINE=$(echo "$LSUSB_CACHE" | grep -i "16c0" | head -1)
    pass "A1.1 Teensy in lsusb: $TEENSY_LINE"
else
    fail "A1.1 Teensy NOT in lsusb (VID 16c0 not found)"
    info "Check: USB data cable? Different port? Firmware flashed?"
fi

# A1.2 — /dev/ttyACM*
TEENSY_DEVS=$(ls /dev/ttyACM* 2>/dev/null || true)
if [ -n "$TEENSY_DEVS" ]; then
    pass "A1.2 Serial device: $TEENSY_DEVS"
else
    fail "A1.2 No /dev/ttyACM* device found"
    info "Check: dmesg | grep tty"
fi

# ── A2: LiDAR (LDROBOT STL-19P) ────────────────────────────────────────────
echo ""
echo "── A2: LiDAR (LDROBOT STL-19P) ────────────────────────────────"

# A2.1 — lsusb
if echo "$LSUSB_CACHE" | grep -q "10c4"; then
    LIDAR_LINE=$(echo "$LSUSB_CACHE" | grep "10c4" | head -1)
    pass "A2.1 CP210x in lsusb: $LIDAR_LINE"
else
    fail "A2.1 CP210x NOT in lsusb (VID:PID 10c4:ea60 not found)"
fi

# A2.2 — /dev/ldlidar symlink
if [ -L /dev/ldlidar ]; then
    LIDAR_TARGET=$(readlink -f /dev/ldlidar)
    pass "A2.2 /dev/ldlidar -> $LIDAR_TARGET"
elif [ -e /dev/ldlidar ]; then
    warn "A2.2 /dev/ldlidar exists but is not a symlink"
else
    fail "A2.2 /dev/ldlidar not found"
    info "Fix: sudo udevadm control --reload-rules && sudo udevadm trigger"
    info "Check: /etc/udev/rules.d/99-ldlidar.rules"
fi

# ── A3: OAK-D Pro ──────────────────────────────────────────────────────────
echo ""
echo "── A3: OAK-D Pro (Depth Camera) ───────────────────────────────"

# A3.1 — lsusb
if echo "$LSUSB_CACHE" | grep -q "03e7"; then
    OAK_LINE=$(echo "$LSUSB_CACHE" | grep "03e7" | head -1)
    pass "A3.1 Myriad X in lsusb: $OAK_LINE"
else
    fail "A3.1 OAK-D NOT in lsusb (VID 03e7 not found)"
    info "Check: USB-C data cable? Different port? 80-movidius.rules installed?"
fi

# A3.2 — depthai Python (quick import test)
if python3 -c "import depthai" 2>/dev/null; then
    OAKD_COUNT=$(python3 -c "import depthai as d; print(len(d.Device.getAllAvailableDevices()))" 2>/dev/null || echo "0")
    if [ "$OAKD_COUNT" -gt 0 ] 2>/dev/null; then
        pass "A3.2 DepthAI sees $OAKD_COUNT device(s)"
    else
        fail "A3.2 DepthAI imported but found 0 devices"
    fi
else
    warn "A3.2 depthai Python module not importable (pip install depthai)"
fi

# ── A4: ReSpeaker Microphone ────────────────────────────────────────────────
echo ""
echo "── A4: ReSpeaker Microphone ────────────────────────────────────"

# A4.1 — ALSA device
ARECORD_OUT=$(arecord -l 2>/dev/null || true)
if echo "$ARECORD_OUT" | grep -qiE "usb|respeaker|pnp"; then
    CARD_LINE=$(echo "$ARECORD_OUT" | grep -iE "usb|respeaker|pnp" | head -1)
    pass "A4.1 ALSA USB audio: $CARD_LINE"

    # Extract card number for info
    CARD_NUM=$(echo "$CARD_LINE" | grep -oP 'card \K[0-9]+' | head -1)
    if [ -n "$CARD_NUM" ]; then
        info "Card number: $CARD_NUM  (use plughw:$CARD_NUM,0 for recording)"
    fi
else
    fail "A4.1 No USB audio device in arecord -l"
    info "Check: Is ReSpeaker plugged in? lsusb | grep 2886"
fi

# ReSpeaker model detection (bonus)
if echo "$LSUSB_CACHE" | grep -q "2886:0018"; then
    info "ReSpeaker model: 4-Mic Array (2886:0018)"
elif echo "$LSUSB_CACHE" | grep -q "2886:0008"; then
    info "ReSpeaker model: 6-Mic Circular (2886:0008)"
elif echo "$LSUSB_CACHE" | grep -q "2886"; then
    RESPEAKER_LINE=$(echo "$LSUSB_CACHE" | grep "2886" | head -1)
    info "ReSpeaker detected: $RESPEAKER_LINE"
fi

# ── A5: ESP32 / PS3 Controller ─────────────────────────────────────────────
echo ""
echo "── A5: ESP32 / PS3 Controller ──────────────────────────────────"

# A5.1 — /dev/ttyUSB* (may conflict with LiDAR)
ESP32_DEVS=$(ls /dev/ttyUSB* 2>/dev/null || true)
if [ -n "$ESP32_DEVS" ]; then
    # Count ttyUSB devices
    USB_COUNT=$(echo "$ESP32_DEVS" | wc -w)
    if [ "$USB_COUNT" -gt 1 ]; then
        pass "A5.1 Found $USB_COUNT /dev/ttyUSB devices: $ESP32_DEVS"
        info "Note: LiDAR also uses ttyUSB. ESP32 is the non-ldlidar device."
        if [ -L /dev/ldlidar ]; then
            LIDAR_DEV=$(readlink -f /dev/ldlidar)
            info "LiDAR is $LIDAR_DEV — ESP32 is the other one"
        fi
    elif [ "$USB_COUNT" -eq 1 ]; then
        # Only one ttyUSB — could be LiDAR or ESP32
        if [ -L /dev/ldlidar ]; then
            LIDAR_DEV=$(readlink -f /dev/ldlidar)
            if [ "$ESP32_DEVS" = "$LIDAR_DEV" ]; then
                warn "A5.1 Only ttyUSB device is LiDAR ($LIDAR_DEV). ESP32 not detected."
            else
                pass "A5.1 ESP32 likely at $ESP32_DEVS (LiDAR at $LIDAR_DEV)"
            fi
        else
            warn "A5.1 One ttyUSB found ($ESP32_DEVS) but unclear if ESP32 or LiDAR"
        fi
    fi
else
    warn "A5.1 No /dev/ttyUSB* devices (ESP32 not connected or not needed yet)"
fi

# ── USB Conflict Detection ──────────────────────────────────────────────────
echo ""
echo "── USB Conflict Check ──────────────────────────────────────────"

CP210X_COUNT=$(echo "$LSUSB_CACHE" | grep -c "10c4" || true)
if [ "$CP210X_COUNT" -gt 1 ]; then
    warn "Multiple CP210x devices ($CP210X_COUNT). LiDAR and ESP32 share VID:PID."
    info "Differentiate with: udevadm info -a -n /dev/ttyUSBx | grep ATTR{serial}"
    info "Add ESP32-specific udev rule using serial number or devpath."
elif [ "$CP210X_COUNT" -eq 1 ]; then
    pass "Single CP210x device — no USB conflict"
else
    info "No CP210x devices detected"
fi

# ── Summary ─────────────────────────────────────────────────────────────────
echo ""
echo "════════════════════════════════════════════════════════════════"
echo "  SUMMARY: ${GREEN}${PASS_COUNT} PASS${NC}  ${RED}${FAIL_COUNT} FAIL${NC}  ${YELLOW}${WARN_COUNT} WARN${NC}"
echo "════════════════════════════════════════════════════════════════"
echo ""

if [ "$FAIL_COUNT" -gt 0 ]; then
    echo -e "  ${RED}Action required: Fix FAIL items before proceeding to ROS tests.${NC}"
    exit 1
else
    echo -e "  ${GREEN}Hardware checks passed. Proceed to ROS-level tests.${NC}"
    exit 0
fi
