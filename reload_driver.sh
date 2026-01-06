#!/bin/bash
# Properly reload the gc2607 driver

set -e

echo "=== Reloading GC2607 Driver ==="
echo ""

# Disable media link first
echo "Disabling media link..."
media-ctl -d /dev/media0 -l '"Intel IPU6 CSI2 0":1 -> "Intel IPU6 ISYS Capture 0":0[0]' 2>/dev/null || true

# Wait a moment for cleanup
sleep 1

# Unload IPU modules in reverse order
echo "Unloading IPU modules..."
sudo modprobe -r intel-ipu6-isys 2>/dev/null || true
sudo modprobe -r intel-ipu6 2>/dev/null || true

# Wait for modules to fully unload
sleep 1

# Now unload gc2607
echo "Unloading gc2607..."
sudo rmmod gc2607

# Reload everything
echo "Reloading modules..."
sudo modprobe videodev
sudo modprobe v4l2-async
sudo modprobe ipu_bridge
sudo modprobe intel-ipu6
sudo modprobe intel-ipu6-isys

echo "Loading gc2607..."
sudo insmod gc2607.ko

# Wait for device initialization
sleep 2

echo ""
echo "✅ Driver reloaded successfully!"
echo ""
echo "Checking device..."
v4l2-ctl -d /dev/v4l-subdev6 --list-ctrls | grep -E "(exposure|gain|link_freq)"
