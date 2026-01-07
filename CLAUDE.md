# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Current Status Summary

**✅ CAMERA IS WORKING AND CAPTURING IMAGES!**

**What's Complete (Phases 1-6):**
- ✅ Driver fully functional with V4L2 integration
- ✅ IPU6 bridge integration (modified ipu_bridge.ko installed)
- ✅ Successfully capturing 1920x1080@30fps RAW Bayer images
- ✅ Media pipeline: gc2607 → IPU6 CSI2 0 → /dev/video0
- ✅ Image viewer scripts with brightness adjustment

**What's Next (Phase 7 - IN PROGRESS):**
- ⏳ Implement exposure control (V4L2_CID_EXPOSURE) for proper brightness
- ⏳ Implement gain control (V4L2_CID_ANALOGUE_GAIN)
- Goal: Native proper exposure without post-processing
- Purpose: Enable use with OBS Studio and other camera applications

**Quick Capture Test:**
```bash
sudo modprobe videodev v4l2-async ipu_bridge intel-ipu6 intel-ipu6-isys
sudo insmod gc2607.ko
v4l2-ctl -d /dev/video0 --set-fmt-video=width=1920,height=1080,pixelformat=BA10
media-ctl -d /dev/media0 -l '"Intel IPU6 CSI2 0":1 -> "Intel IPU6 ISYS Capture 0":0[1]'
v4l2-ctl -d /dev/video0 --stream-mmap --stream-count=1 --stream-to=test.raw
./view_raw_wb.py test.raw 5.0 && feh test.png
```

**Note:** Use `view_raw_wb.py` (with white balance) for natural colors, or `view_raw_bright.py` (without WB) for quick testing.

## Project Overview

This project successfully ported the GalaxyCore GC2607 camera sensor driver from the Ingenic T41 platform (MIPS embedded) to the Linux V4L2 subsystem for Intel IPU6 on x86_64.

**Target Hardware:**
- Laptop: Huawei MateBook Pro VGHH-XX
- Sensor: GC2607 (1920x1080@30fps, MIPI CSI-2, RAW10)
- Platform: Intel IPU6
- PMIC: INT3472:01 discrete (intel_skl_int3472_discrete driver)
- I2C Bus: /dev/i2c-5
- I2C Address: 0x37
- Chip ID: 0x2607 (registers 0x03f0=0x26, 0x03f1=0x07) ✅ VERIFIED

**ACPI Matching:**
- Device name: GCTI2607:00
- ACPI path: `\_SB_.PC00.LNK0`
- Modalias: `acpi:GCTI2607:GCTI2607:`
- Driver uses ACPI match table with "GCTI2607" HID

**INT3472 PMIC Resources (INT3472:01):**
- Regulator: `INT3472:01-avdd` (used by sensor)
- Privacy LED: `GCTI2607_00::privacy_led`
- Reset GPIO: Provided via ACPI
- Clock: 19.2 MHz from platform
- Status: Enabled and bound to `int3472-discrete`

## Architecture

### Reference Driver (reference/gc2607.c)
The original Ingenic T41 driver uses platform-specific APIs:
- `tx-isp-common.h`, `sensor-common.h`: T41 ISP framework
- `private_i2c_transfer()`, `private_gpio_request()`: T41-specific wrappers
- Platform device registration with `tx_isp_subdev` abstraction

### Implemented V4L2 Driver (gc2607.c)
The new driver implements:
1. ✅ Standard Linux V4L2 subdev APIs
2. ✅ I2C client driver with ACPI match table
3. ✅ V4L2 subdev ops (video, pad)
4. ✅ GPIO/regulator APIs via INT3472 PMIC
5. ✅ Async subdev registration for IPU6
6. ✅ V4L2 controls (link frequency, pixel rate)

### Key Hardware Configuration
Confirmed from hardware testing:
- MIPI: 2 lanes, 672 Mbps/lane (link_freq=336MHz)
- Pixel format: SGRBG10 (Bayer GRBG 10-bit)
- Resolution: 1920x1080@30fps
- Frame timing: HTS=2048, VTS=1335
- Register addressing: 16-bit addresses, 8-bit values
- Initialization: 122 register writes

## Development Workflow

### Building the Driver
```bash
# Out-of-tree build against running kernel
make

# Clean build artifacts
make clean
```

### Testing the Driver
```bash
# Quick test (recommended)
sudo ./test_phase4.sh

# Load module manually
sudo insmod gc2607.ko

# Check probe status
dmesg | grep gc2607

# Check V4L2 registration
v4l2-ctl --list-subdevs

# Unload module
sudo rmmod gc2607
```

### Camera Integration Testing
```bash
# Test IPU6 integration
sudo ./test_camera_streaming.sh

# Check media controller topology
media-ctl -d /dev/media0 --print-topology

# Investigate ipu_bridge
sudo ./investigate_ipu_bridge.sh
```

## Implementation Status

### Phase 1: Skeleton Driver ✅ COMPLETE
**Status:** Fully working
- I2C client registration with ACPI matching
- Basic probe/remove with logging
- Module metadata and build system

**Test:** Module loads and binds to ACPI device

### Phase 2: Power Management ✅ COMPLETE
**Status:** Fully working
- INT3472 PMIC integration (GPIOs, regulators, clocks)
- Proper reset sequence: HIGH (20ms) → LOW (20ms) → HIGH (10ms)
- Sensor detection confirmed (chip ID 0x2607)
- Power on/off sequences working

**Test:** `sudo ./QUICK_TEST.sh` shows chip ID 0x2607

**Key Achievement:** Fixed critical reset sequence bug where sensor was left in reset state

### Phase 3: Register Initialization ✅ COMPLETE
**Status:** Fully working
- 122-register initialization sequence from reference driver
- Register write functions implemented
- Integrated into s_stream() for streaming start
- Register array with proper handling of delays

**Test:** `sudo ./test_phase3.sh` confirms all registers ready

**Files:** Register table `gc2607_1080p_30fps_regs[]` in gc2607.c

### Phase 4: V4L2 Integration ✅ COMPLETE
**Status:** Fully working
- V4L2 pad operations (enum_mbus_code, enum_frame_size, get_fmt, set_fmt)
- V4L2 controls (link_freq=336MHz, pixel_rate=134.4MHz)
- Async subdev registration
- Format: SGRBG10 1920x1080@30fps
- Mode management structure

**Test:** `sudo ./test_phase4.sh` shows successful probe and V4L2 integration

**What works:**
- Driver loads and probes successfully
- Sensor detection (chip ID 0x2607)
- V4L2 format negotiation ready
- Async subdev registered

### Phase 5: IPU6 Bridge Integration ✅ COMPLETE
**Status:** Fully working - Camera integrated with IPU6

**What Was Completed:**
1. ✅ Downloaded Linux kernel 6.17.9 source to ~/kernel/dev
2. ✅ Modified `drivers/media/pci/intel/ipu-bridge.c` to add GC2607 support
3. ✅ Successfully compiled modified ipu_bridge module (511KB)
4. ✅ Installed modified module with GC2607 (GCTI2607) support
5. ✅ Camera detected by IPU6 bridge
6. ✅ Media pipeline established and working

**Modification Made:**
Added to `/home/abbood/kernel/dev/linux-6.17.9/drivers/media/pci/intel/ipu-bridge.c` at line 52:
```c
/* GalaxyCore GC2607 */
IPU_SENSOR_CONFIG("GCTI2607", 1, 336000000),
```

**Files Created:**
- `setup_ipu_bridge_mod.sh` - Downloads kernel source and prepares for modification
- `compile_ipu_bridge.sh` - Compiles ipu_bridge module (use after copying Module.symvers)
- `install_ipu_bridge.sh` - Installs modified module (✅ COMPLETED)
- `reload_ipu_modules.sh` - Reloads IPU modules (had device busy errors)

**Key Achievements:**
- ✅ Modified ipu_bridge.ko successfully compiled and installed
- ✅ IPU6 bridge recognizes GCTI2607 sensor
- ✅ Media pipeline established: gc2607 → Intel IPU6 CSI2 0 → /dev/video0
- ✅ V4L2 subdev created at /dev/v4l-subdev6

**Critical Fix Required:**
Added `V4L2_SUBDEV_FL_HAS_DEVNODE` flag to gc2607 driver to create /dev/v4l-subdev device node for sensor.

### Phase 6: Image Capture ✅ COMPLETE
**Status:** Fully working - Camera successfully capturing images!

**Steps to Success:**
1. ✅ Loaded all required modules (videodev, ipu_bridge, intel-ipu6, intel-ipu6-isys, gc2607)
2. ✅ Enabled media link: `media-ctl -d /dev/media0 -l '"Intel IPU6 CSI2 0":1 -> "Intel IPU6 ISYS Capture 0":0[1]'`
3. ✅ Fixed format mismatch: Changed video0 from GB10 to BA10 (GRBG) to match sensor output
4. ✅ Captured first image successfully!

**Format Configuration:**
- Sensor output: SGRBG10_1X10 (0x300a) - GRBG Bayer pattern
- Video device: BA10 pixel format (10-bit Bayer GRGR/BGBG)
- Resolution: 1920x1080
- Link frequency: 336 MHz
- Pixel rate: 134.4 MHz

**Capture Commands:**
```bash
# Set correct pixel format (BA10 for GRBG pattern)
v4l2-ctl -d /dev/video0 --set-fmt-video=width=1920,height=1080,pixelformat=BA10

# Enable media link
media-ctl -d /dev/media0 -l '"Intel IPU6 CSI2 0":1 -> "Intel IPU6 ISYS Capture 0":0[1]'

# Capture image
v4l2-ctl -d /dev/video0 --stream-mmap --stream-count=1 --stream-to=capture.raw

# Convert to viewable PNG (with brightness boost and rotation)
./view_raw_bright.py capture.raw 5.0

# View image
feh capture.png
```

**Known Issues & Solutions:**
1. **Image too dark**: Use `view_raw_bright.py` with brightness multiplier (3.0-8.0)
2. **Image upside down**: `view_raw_bright.py` automatically flips the image
3. **Privacy slider**: Check laptop for physical camera privacy slider/cover

**Files Created:**
- `view_raw.py` - Basic raw Bayer to PNG converter
- `view_raw_bright.py` - Converter with brightness boost and auto-flip
- `compile_ipu_bridge_simple.sh` - Simplified single-module build script

### Phase 7: Exposure & Gain Controls ⏳ IN PROGRESS
**Status:** Ready to implement - Camera captures but images are dark

**Current Situation:**
- Camera successfully captures images at 1920x1080@30fps
- Images are very dark due to lack of exposure/gain controls
- Currently using post-processing workaround (`view_raw_bright.py`)
- Driver only implements read-only controls (link_freq, pixel_rate)

**Next Steps - Implement V4L2 Controls:**
1. Add `V4L2_CID_EXPOSURE` control
   - Register: 0x0202 (high byte), 0x0203 (low byte)
   - Range: TBD (needs testing, reference uses up to ~0x537)
   - Default: Start with 0x0400 (1024 lines)

2. Add `V4L2_CID_ANALOGUE_GAIN` control
   - Registers: 0x02b3, 0x02b4, 0x020c, 0x020d
   - Reference driver uses lookup table (23 entries)
   - Simplified implementation: Start with just 0x02b3, 0x02b4

3. Implement `s_ctrl` callback in V4L2 control ops
   - Write exposure value to registers during streaming
   - Write gain value to registers during streaming

4. Update control handler initialization
   - Change from 2 controls to 4 controls
   - Add both new controls with proper ranges

**Implementation Reference:**
From reference driver (reference/gc2607.c):
```c
// Exposure write (lines 428-429)
ret += gc2607_write(sd, 0x0202, expo >> 8);
ret += gc2607_write(sd, 0x0203, expo & 0xff);

// Gain write (lines 430-433)
ret = gc2607_write(sd, 0x02b3, val_lut[again].reg2b3);
ret = gc2607_write(sd, 0x02b4, val_lut[again].reg2b4);
ret = gc2607_write(sd, 0x020c, val_lut[again].reg20c);
ret = gc2607_write(sd, 0x020d, val_lut[again].reg20d);
```

**Benefits After Implementation:**
- ✅ Proper image brightness without post-processing
- ✅ Real-time exposure adjustment
- ✅ Compatible with standard camera applications (OBS, Cheese, etc.)
- ✅ Auto-exposure can be added later

**Test Plan:**
1. Add controls and rebuild driver
2. Load driver and verify controls exist:
   ```bash
   v4l2-ctl -d /dev/v4l-subdev6 --list-ctrls
   ```
3. Test manual exposure adjustment:
   ```bash
   v4l2-ctl -d /dev/v4l-subdev6 --set-ctrl exposure=2000
   ```
4. Capture image and verify brightness improved
5. Find optimal default values through testing

**Files to Create:**
- Patch file with implementation details (for reference)
- Test script to iterate through exposure/gain values

## Key Differences from Reference Driver

| Aspect | T41 Reference | V4L2 Implementation |
|--------|---------------|---------------------|
| I2C API | `private_i2c_transfer()` | `i2c_transfer()` ✅ |
| Subdev | `tx_isp_subdev` | `v4l2_subdev` ✅ |
| Power | Direct GPIO control | INT3472 PMIC subsystem ✅ |
| Registration | Platform device | I2C driver + async subdev ✅ |
| Bus config | `gc2607_mipi` struct | V4L2 controls (link_freq) ✅ |
| Reset | Direct GPIO | Proper pulse sequence ✅ |

## Register Map Reference
- Chip ID: 0x03f0 (high byte=0x26), 0x03f1 (low byte=0x07)
- Exposure: 0x0202 (high), 0x0203 (low)
- Analog gain: 0x02b3, 0x02b4, 0x020c, 0x020d
- VTS (frame length): 0x0220 (high), 0x0221 (low)
- HTS (line length): 0x0342 (high=0x08), 0x0343 (low=0x00) = 2048
- Init sequence: 122 register writes in `gc2607_1080p_30fps_regs[]`

## Test Scripts & Documentation

### Test Scripts
- `test_phase3.sh` - Verify register initialization code
- `test_phase4.sh` - Verify V4L2 integration
- `test_camera_streaming.sh` - Check IPU6 integration and media devices
- `investigate_ipu_bridge.sh` - Analyze ipu_bridge sensor support
- `QUICK_TEST.sh` - Quick driver functionality test

### Documentation Files
- `CLAUDE.md` (this file) - Project overview and current status
- `INT3472_INTEGRATION_ANALYSIS.md` - PMIC integration details
- `NEXT_STEPS.md` - Detailed implementation roadmap
- `PHASE2_FIX.md` - Reset sequence fix documentation
- `TEST_PHASE2.md` - Initial testing results

### Reference Files
- `reference/gc2607.c` - Original Ingenic T41 driver
- `reference/` - Hardware documentation and datasheets

## Driver Implementation Details

### Power-On Sequence (gc2607.c:318-428)
1. Enable regulators (if available) - 5-6ms delay
2. Enable clock (19.2 MHz) - 5-6ms delay
3. Reset pulse: LOW (20ms) → HIGH (20ms) → LOW (20ms) → HIGH (10ms)
4. Powerdown pulse (if GPIO exists): HIGH → LOW (10ms) → HIGH (10ms)
5. Wait for sensor boot: 20ms
6. Total: ~100ms power-on sequence

**Critical:** Reset must end in de-asserted state (HIGH) or sensor won't respond!

### Register Initialization (gc2607.c:158-296)
- 122 registers configured for 1920x1080@30fps
- Includes frame timing, MIPI config, ISP settings
- Called during s_stream(enable=1)
- Supports DELAY marker for timing-sensitive sequences

### V4L2 Controls (gc2607.c:729-751)
- `V4L2_CID_LINK_FREQ`: 336 MHz (read-only, required by IPU6)
- `V4L2_CID_PIXEL_RATE`: 134.4 MHz (read-only)
- Both controls are mandatory for IPU6 integration

### Pad Operations (gc2607.c:435-525)
- `enum_mbus_code`: Reports MEDIA_BUS_FMT_SGRBG10_1X10
- `enum_frame_size`: Reports 1920x1080
- `get_fmt`: Returns current format
- `set_fmt`: Validates and applies format

## Known Issues & Solutions

### Issue: Format Mismatch Error
- **Symptom**: `VIDIOC_STREAMON` fails with "format mismatch 1920x1080,300a != 1920x1080,300e"
- **Root Cause**: Video device pixel format (GB10) doesn't match sensor output (SGRBG10)
- **Solution**: Use BA10 pixel format on video0 to match sensor's GRBG pattern
- **Status**: ✅ RESOLVED

### Issue: Dummy Regulators
- **Symptom**: Driver reports "supply dovdd not found, using dummy regulator"
- **Impact**: None - INT3472 PMIC handles power internally
- **Status**: Expected behavior, sensor works correctly

### Issue: Dark Images
- **Symptom**: Captured images are very dark
- **Root Cause**: No exposure/gain controls implemented yet
- **Workaround**: Use `view_raw_bright.py` with brightness multiplier (3.0-8.0)
- **Future**: Implement V4L2 exposure and gain controls

## Hardware Verification

### Fully Confirmed Working:
- ✅ ACPI device detection (GCTI2607:00 status=15)
- ✅ I2C communication (chip ID 0x2607 read successfully)
- ✅ Reset GPIO control via INT3472:01
- ✅ Clock provision (19.2 MHz)
- ✅ Power sequencing
- ✅ Register initialization (122 registers)
- ✅ V4L2 subdev registration with device node (/dev/v4l-subdev6)
- ✅ Async subdev registration
- ✅ IPU6 media controller integration
- ✅ MIPI CSI-2 data transmission (2 lanes, 336 MHz link frequency)
- ✅ Image capture and streaming
- ✅ Frame buffer management

## Other Sensors on This Laptop

ACPI scan revealed multiple camera sensors:
- **GCTI2607:00** - GC2607 rear camera (this driver)
- **GCTI1029:00** - GC1029 (likely front camera, also needs bridge support)
- **OVTI01AS:00** - OmniVision sensor (bridge supported)
- **OVTI13B1:00** - OmniVision sensor (bridge supported)
- **INT3472:00-12** - Multiple PMIC devices

This laptop has a multi-camera setup with at least 4 sensors.

## Future Enhancements

The camera is now fully functional! Potential improvements:

**High Priority:**
- Exposure control implementation (V4L2_CID_EXPOSURE)
- Gain control with LUT (V4L2_CID_ANALOGUE_GAIN)
- Auto white balance
- Better Bayer demosaicing algorithm

**Medium Priority:**
- Multiple resolution support (currently fixed at 1920x1080)
- Frame rate control (currently fixed at 30fps)
- Test pattern mode for debugging
- Privacy LED control integration

**Low Priority:**
- Auto-focus integration (if VCM present)
- HDR support
- Advanced ISP features

## Quick Start Guide

**Build the driver:**
```bash
make
```

**Load modules and capture an image:**
```bash
# Load V4L2 and IPU6 modules
sudo modprobe videodev
sudo modprobe v4l2-async
sudo modprobe ipu_bridge
sudo modprobe intel-ipu6
sudo modprobe intel-ipu6-isys

# Load GC2607 driver
sudo insmod gc2607.ko

# Configure format and enable link
v4l2-ctl -d /dev/video0 --set-fmt-video=width=1920,height=1080,pixelformat=BA10
media-ctl -d /dev/media0 -l '"Intel IPU6 CSI2 0":1 -> "Intel IPU6 ISYS Capture 0":0[1]'

# Capture image
v4l2-ctl -d /dev/video0 --stream-mmap --stream-count=1 --stream-to=test.raw

# Convert and view
./view_raw_bright.py test.raw 5.0
feh test.png
```

**Note:** First-time setup requires installing the modified ipu_bridge module (see Phase 5).

## Contact & References

**Key Resources:**
- Linux kernel source: https://kernel.org
- V4L2 documentation: https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/v4l2.html
- Intel IPU6 documentation: Linux kernel drivers/media/pci/intel/
- Media controller documentation: https://www.kernel.org/doc/html/latest/userspace-api/media/mediactl/media-controller.html

**Project Status:** ✅ FULLY FUNCTIONAL - Camera driver complete and capturing images!
**Last Updated:** January 6, 2026
**Kernel Version:** 6.17.9-arch1-1
**Achievement:** Successfully ported proprietary embedded camera driver to mainline Linux V4L2 with IPU6 integration
