#!/bin/bash
# Reload v4l2loopback with Chrome-compatible settings

set -e

echo "=== Reloading v4l2loopback for Chrome compatibility ==="
echo ""

# Kill gstreamer if running
echo "Stopping gstreamer pipeline..."
pkill -f "gst-launch.*video48" 2>/dev/null || true
sleep 1

# Unload v4l2loopback
echo "Unloading v4l2loopback..."
sudo modprobe -r v4l2loopback
sleep 1

# Reload with Chrome-friendly parameters
echo "Loading v4l2loopback with Chrome-compatible settings..."
sudo modprobe v4l2loopback \
    devices=1 \
    video_nr=50 \
    card_label="GC2607 RGB Camera" \
    exclusive_caps=1 \
    max_buffers=2

echo ""
echo "✅ v4l2loopback reloaded"
echo ""

# Find the new device (try both possible names)
VIRT_DEV=$(v4l2-ctl --list-devices | grep -A1 "GC2607 RGB" | grep "/dev/video" | tr -d '\t' | head -1)

if [ -z "$VIRT_DEV" ]; then
    # Fallback to video50 if we can't find it by name
    if [ -e /dev/video50 ]; then
        VIRT_DEV=/dev/video50
    else
        echo "❌ Error: Could not find virtual camera device"
        exit 1
    fi
fi

echo "Virtual camera: $VIRT_DEV"
echo ""

# Set optimal exposure/gain
echo "Setting camera parameters..."
v4l2-ctl -d /dev/v4l-subdev6 --set-ctrl exposure=1335,analogue_gain=253

echo ""
echo "Starting conversion pipeline..."
echo "Press Ctrl+C to stop"
echo ""

# Start pipeline
gst-launch-1.0 -v \
    v4l2src device=/dev/video0 ! \
    "video/x-bayer,format=grbg10le,width=1920,height=1080,framerate=30/1" ! \
    bayer2rgb ! \
    videoflip method=rotate-180 ! \
    videoconvert ! \
    "video/x-raw,format=I420" ! \
    v4l2sink device=$VIRT_DEV
