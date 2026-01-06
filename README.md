# GC2607 Camera Driver for Linux

A fully functional Linux V4L2 driver for the GalaxyCore GC2607 camera sensor, integrated with Intel IPU6 on x86_64 systems.

## Overview

This driver successfully ports the GC2607 sensor from embedded platforms to mainline Linux, enabling camera functionality on laptops with Intel IPU6 that use this sensor.

**Status:** ✅ **FULLY FUNCTIONAL** - Successfully capturing images!

### Supported Hardware

- **Sensor:** GalaxyCore GC2607
- **Platform:** Intel IPU6 (tested on Huawei MateBook Pro VGHH-XX)
- **Interface:** MIPI CSI-2 (2 lanes, 672 Mbps/lane)
- **Resolution:** 1920x1080 @ 30fps
- **Format:** 10-bit RAW Bayer (GRBG pattern)
- **ACPI HID:** GCTI2607

## Project Status

- ✅ Phase 1: Skeleton driver with ACPI binding
- ✅ Phase 2: Power management and sensor detection
- ✅ Phase 3: Register initialization (122 registers)
- ✅ Phase 4: V4L2 integration (async subdev, pad ops, controls)
- ✅ Phase 5: IPU6 bridge integration
- ✅ Phase 6: Image capture and streaming **SUCCESS!**

## Features

✅ Full V4L2 subdev integration
✅ Intel IPU6 media controller support
✅ MIPI CSI-2 interface (2 lanes @ 336 MHz)
✅ 1920x1080 @ 30fps capture
✅ 10-bit RAW Bayer output
✅ Power management via INT3472 PMIC
✅ Runtime PM support
✅ Proper reset sequencing

## Prerequisites

### Required Packages (Arch Linux)

```bash
sudo pacman -S base-devel linux-headers v4l-utils media-ctl python-pillow python-numpy feh
```

### Modified IPU6 Bridge Module

**Important:** This driver requires a modified `ipu_bridge` kernel module that recognizes the GC2607 sensor.

#### Installation

```bash
# 1. Run the setup script to download kernel source
./setup_ipu_bridge_mod.sh

# 2. Compile the modified bridge module
./compile_ipu_bridge_simple.sh

# The script will automatically:
# - Add GCTI2607 support to ipu-bridge.c
# - Compile against your running kernel
# - Install the modified module
# - Create a backup of the original
```

## Building the Driver

```bash
make
```

## Usage

### Quick Start - Capture Your First Image

```bash
# 1. Load required kernel modules
sudo modprobe videodev
sudo modprobe v4l2-async
sudo modprobe ipu_bridge
sudo modprobe intel-ipu6
sudo modprobe intel-ipu6-isys

# 2. Load the GC2607 driver
sudo insmod gc2607.ko

# 3. Verify the sensor is detected
media-ctl -d /dev/media0 --print-topology | grep gc2607
# You should see: - entity 349: gc2607 5-0037 (...)

# 4. Configure the video device format
v4l2-ctl -d /dev/video0 --set-fmt-video=width=1920,height=1080,pixelformat=BA10

# 5. Enable the media pipeline link
media-ctl -d /dev/media0 -l '"Intel IPU6 CSI2 0":1 -> "Intel IPU6 ISYS Capture 0":0[1]'

# 6. Capture an image
v4l2-ctl -d /dev/video0 --stream-mmap --stream-count=1 --stream-to=capture.raw

# 7. Convert RAW to viewable PNG (with brightness boost)
./view_raw_bright.py capture.raw 5.0

# 8. View the image
feh capture.png
```

### Automated Capture Script

For convenience, you can create a script:

```bash
#!/bin/bash
# capture.sh - Quick capture script

# Configure and capture
v4l2-ctl -d /dev/video0 --set-fmt-video=width=1920,height=1080,pixelformat=BA10
media-ctl -d /dev/media0 -l '"Intel IPU6 CSI2 0":1 -> "Intel IPU6 ISYS Capture 0":0[1]'
v4l2-ctl -d /dev/video0 --stream-mmap --stream-count=1 --stream-to=capture_$(date +%s).raw

# Convert last captured image
LATEST=$(ls -t capture_*.raw | head -1)
./view_raw_bright.py "$LATEST" 5.0
feh "${LATEST%.raw}.png"
```

## Troubleshooting

### Image is too dark
The driver doesn't yet implement exposure/gain controls. Use the brightness parameter:
```bash
./view_raw_bright.py capture.raw 8.0  # Try values between 3.0 and 10.0
```

### Image is all black
Check if your laptop has a physical camera privacy slider/cover. Many laptops include a hardware privacy mechanism.

### "Link has been severed" error
The media link isn't enabled. Run:
```bash
media-ctl -d /dev/media0 -l '"Intel IPU6 CSI2 0":1 -> "Intel IPU6 ISYS Capture 0":0[1]'
```

### "Format mismatch" error
Ensure you're using the BA10 pixel format (GRBG Bayer):
```bash
v4l2-ctl -d /dev/video0 --set-fmt-video=width=1920,height=1080,pixelformat=BA10
```

### Sensor not detected
1. Check that ipu_bridge recognizes GCTI2607:
   ```bash
   sudo dmesg | grep -i "GCTI2607\|gc2607"
   ```
2. Verify the modified ipu_bridge is loaded:
   ```bash
   modinfo ipu_bridge
   strings /lib/modules/$(uname -r)/kernel/drivers/media/pci/intel/ipu-bridge.ko.zst | grep GCTI2607
   ```

## Architecture

### Media Pipeline

```
┌─────────────┐      ┌──────────────────┐      ┌─────────────────────┐
│  gc2607     │      │ Intel IPU6 CSI2 0│      │ Intel IPU6 ISYS     │
│  5-0037     │─────▶│  (MIPI Receiver) │─────▶│ Capture 0           │
│ (Sensor)    │      │  /dev/v4l-subdev0│      │ /dev/video0         │
└─────────────┘      └──────────────────┘      └─────────────────────┘
 /dev/v4l-subdev6
   SGRBG10_1X10           SGRBG10_1X10              BA10 (GRBG)
   1920x1080              1920x1080                 1920x1080
```

### Key Components

- **gc2607.c** - Main driver (V4L2 subdev, power management, register initialization)
- **ipu-bridge.c** - Modified to recognize GCTI2607 sensor
- **view_raw_bright.py** - RAW Bayer to PNG converter with brightness boost
- **compile_ipu_bridge_simple.sh** - Builds modified ipu_bridge module

## Technical Details

### Sensor Specifications
- **Resolution:** 1920x1080
- **Frame Rate:** 30 fps
- **Bit Depth:** 10-bit RAW
- **Bayer Pattern:** GRBG (MEDIA_BUS_FMT_SGRBG10_1X10)
- **I2C Address:** 0x37
- **Chip ID:** 0x2607

### MIPI Configuration
- **Lanes:** 2
- **Link Frequency:** 336 MHz
- **Data Rate:** 672 Mbps/lane
- **Pixel Rate:** 134.4 MHz

### Power Management
- **PMIC:** INT3472:01 (intel_skl_int3472_discrete)
- **Clock:** 19.2 MHz from platform
- **Regulators:** avdd (INT3472:01), dovdd (dummy), dvdd (dummy)
- **Reset GPIO:** Provided by INT3472 PMIC
- **Reset Sequence:** HIGH (20ms) → LOW (20ms) → HIGH (10ms)

## Test Scripts

- `test_phase4.sh` - Verify V4L2 integration
- `test_camera_streaming.sh` - Check IPU6 integration
- `investigate_ipu_bridge.sh` - Analyze bridge sensor support
- `QUICK_TEST.sh` - Quick functionality test
- `view_raw.py` - Basic RAW converter
- `view_raw_bright.py` - RAW converter with brightness boost

## Future Enhancements

The driver is fully functional for basic image capture. Planned improvements include:

**High Priority:**
- Exposure control (V4L2_CID_EXPOSURE)
- Analog gain control (V4L2_CID_ANALOGUE_GAIN)
- Auto white balance
- Improved demosaicing algorithm

**Medium Priority:**
- Multiple resolution support
- Frame rate control
- Test pattern mode

**Low Priority:**
- Auto-focus support (if VCM present)
- HDR support

## Documentation

- **[CLAUDE.md](CLAUDE.md)** - Complete development guide and technical details
- **[INT3472_INTEGRATION_ANALYSIS.md](INT3472_INTEGRATION_ANALYSIS.md)** - PMIC integration analysis

## References

- [V4L2 Documentation](https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/v4l2.html)
- [Media Controller Documentation](https://www.kernel.org/doc/html/latest/userspace-api/media/mediactl/media-controller.html)
- [Intel IPU6 Driver](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/drivers/media/pci/intel)

## License

This driver is released under the GPL-2.0 license, consistent with the Linux kernel.

## Contributing

Contributions welcome! Areas of interest:
- Exposure/gain control implementation
- Multi-resolution support
- Other GalaxyCore sensors (GC1029, etc.)
- Testing on different hardware platforms

## Acknowledgments

- Reference driver from Ingenic T41 platform
- Linux kernel V4L2 subsystem documentation
- Intel IPU6 driver developers

---

**Status:** ✅ Production ready for basic image capture
**Tested on:** Huawei MateBook Pro VGHH-XX
**Kernel:** 6.17.9-arch1-1
**Last Updated:** January 6, 2026
**Achievement:** Successfully ported proprietary embedded camera driver to mainline Linux V4L2 with IPU6 integration
