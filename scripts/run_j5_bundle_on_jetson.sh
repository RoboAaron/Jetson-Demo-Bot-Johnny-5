#!/bin/bash
# Run this script ON THE JETSON (after reflash to 22.04) with the j5_bundle SD card inserted.
# Usage: copy to Jetson, then: sudo bash run_j5_bundle_on_jetson.sh

set -e

echo "=== J5 bundle install (mount + Packages + scripts 01-08) ==="

# 1) Find SD card (usually mmcblk1 on Jetson; mmcblk0 is often internal eMMC)
SD_DEV=""
for dev in /dev/mmcblk1 /dev/sda1 /dev/sdb1; do
  if [ -b "$dev" ]; then
    SD_DEV="$dev"
    break
  fi
done
if [ -z "$SD_DEV" ]; then
  echo "ERROR: No SD card found. Check: lsblk"
  exit 1
fi
echo "Using SD device: $SD_DEV"

# 2) Mount at both paths (scripts expect /mnt/j5bundle)
mkdir -p /mnt/j5_bundle /mnt/j5bundle
mount "$SD_DEV" /mnt/j5_bundle 2>/dev/null || true
mount --bind /mnt/j5_bundle /mnt/j5bundle 2>/dev/null || true

if [ ! -d /mnt/j5bundle/scripts ]; then
  echo "ERROR: /mnt/j5bundle/scripts not found. Is the j5_bundle SD card mounted?"
  exit 1
fi

# 3) Create Packages from Packages.gz if needed
for repo in ros2 system; do
  D="/mnt/j5bundle/debs/$repo"
  if [ -f "$D/Packages.gz" ] && [ ! -f "$D/Packages" ]; then
    echo "Creating $D/Packages from Packages.gz"
    gzip -dc "$D/Packages.gz" > "$D/Packages"
  fi
done

# 4) Run install scripts in order
cd /mnt/j5bundle/scripts
chmod +x *.sh

for script in 01_configure_offline_apt.sh 02_install_ros2.sh 03_install_python_deps.sh 04_build_ros2_workspace.sh 05_install_udev_rules.sh 06_copy_models.sh 07_create_systemd_service.sh 08_verify_install.sh; do
  if [ -f "$script" ]; then
    echo "=== Running $script ==="
    ./"$script" || { echo "FAILED: $script"; exit 1; }
  fi
done

echo "=== All steps completed. Reboot if any script suggested it. ==="
