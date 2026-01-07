#!/bin/bash
# Create virtual RGB camera for use with OBS/Meet/etc

set -e

echo "=== Creating Virtual RGB Camera ==="
echo ""

# Check if v4l2loopback is installed
if ! lsmod | grep -q v4l2loopback; then
    echo "Installing v4l2loopback module..."
    if ! pacman -Q v4l2loopback-dkms &>/dev/null; then
        echo "Please install: sudo pacman -S v4l2loopback-dkms"
        exit 1
    fi
    sudo modprobe v4l2loopback devices=1 video_nr=10 card_label="GC2607 RGB" exclusive_caps=1
fi

# Find the actual v4l2loopback device
VIRT_DEV=$(v4l2-ctl --list-devices | grep -A1 "GC2607 RGB" | grep "/dev/video" | tr -d '\t')
if [ -z "$VIRT_DEV" ]; then
    echo "Error: Could not find virtual camera device"
    exit 1
fi

echo "Virtual camera device: $VIRT_DEV"
echo ""

# Set optimal exposure/gain
echo "Setting camera parameters..."
v4l2-ctl -d /dev/v4l-subdev6 --set-ctrl exposure=1335,analogue_gain=253

echo ""
echo "Starting Bayer to RGB conversion pipeline..."
echo "Press Ctrl+C to stop"
echo ""

# Use gstreamer to convert Bayer to RGB and feed to virtual camera
# videoflip method=rotate-180 flips the image right-side up
gst-launch-1.0 -v \
    v4l2src device=/dev/video0 ! \
    "video/x-bayer,format=grbg10le,width=1920,height=1080,framerate=30/1" ! \
    bayer2rgb ! \
    videoflip method=rotate-180 ! \
    videoconvert ! \
    "video/x-raw,format=YUY2" ! \
    v4l2sink device=$VIRT_DEV

echo ""
echo "Pipeline stopped."
