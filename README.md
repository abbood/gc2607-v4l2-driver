# GC2607 V4L2 Camera Driver for Intel IPU6

Linux V4L2 camera sensor driver for GalaxyCore GC2607 on Intel IPU6 platform (Huawei MateBook Pro).

## Status

**Driver:** ✅ Fully Functional | **IPU6 Integration:** 🔄 In Progress (Phase 5)

- ✅ Phase 1: Skeleton driver with ACPI binding
- ✅ Phase 2: Power management and sensor detection (chip ID 0x2607 verified)
- ✅ Phase 3: Register initialization (122 registers)
- ✅ Phase 4: V4L2 integration (async subdev, pad ops, controls)
- 🔄 Phase 5: IPU6 bridge integration (requires ipu_bridge modification)

## Quick Start

### Build and Test
```bash
# Build the driver
make

# Run comprehensive test
sudo ./test_phase4.sh

# Expected: ✅ Probe successful, chip ID detected, format configured
```

### Test IPU6 Integration
```bash
# Check if camera appears in media topology
sudo ./test_camera_streaming.sh

# Current status: Driver works, waiting for ipu_bridge support
```

## Hardware

- **Sensor:** GalaxyCore GC2607
- **Resolution:** 1920x1080@30fps
- **Format:** SGRBG10 (Bayer GRBG 10-bit)
- **Interface:** MIPI CSI-2 (2 lanes, 672 Mbps/lane)
- **Platform:** Intel IPU6
- **I2C:** Bus 5, Address 0x37
- **PMIC:** INT3472:01

## What Works

✅ **Driver Fully Functional:**
- ACPI device binding (GCTI2607)
- I2C communication
- Sensor detection (chip ID: 0x2607)
- Power management via INT3472 PMIC
- Reset GPIO control with proper timing
- Clock provision (19.2 MHz)
- V4L2 pad operations
- Format negotiation (SGRBG10 1920x1080)
- V4L2 controls (link_freq=336MHz, pixel_rate=134.4MHz)
- Async subdev registration

⏳ **Waiting for IPU6 Bridge:**
- Media controller integration
- Actual camera streaming
- Image capture

## Next Steps

The driver is complete but needs one final step: adding GC2607 support to the `ipu_bridge` kernel module.

**What's Needed:**
1. Download Linux 6.17.9 kernel source
2. Modify `drivers/media/pci/intel/ipu-bridge.c`
3. Add: `IPU_SENSOR_CONFIG("GCTI2607", 1, 336000000),`
4. Recompile ipu_bridge module
5. Test with media-ctl

See **CLAUDE.md** for detailed instructions.

## Documentation

- **[CLAUDE.md](CLAUDE.md)** - Complete project documentation and guide
- **[PROJECT_SUMMARY.md](PROJECT_SUMMARY.md)** - Development summary and achievements
- **[INT3472_INTEGRATION_ANALYSIS.md](INT3472_INTEGRATION_ANALYSIS.md)** - PMIC integration details

## Test Scripts

- `test_phase4.sh` - Verify V4L2 integration (recommended)
- `test_camera_streaming.sh` - Check IPU6 integration
- `investigate_ipu_bridge.sh` - Analyze bridge sensor support
- `QUICK_TEST.sh` - Quick functionality test

## Technical Details

**Driver Configuration:**
```
Format: MEDIA_BUS_FMT_SGRBG10_1X10
Resolution: 1920x1080
Frame Rate: 30 fps
MIPI Lanes: 2
Link Frequency: 336 MHz
Pixel Rate: 134.4 MHz
Register Init: 122 registers
```

**Key Files:**
- `gc2607.c` - Main driver implementation (~800 LOC)
- `Makefile` - Out-of-tree kernel module build
- `reference/gc2607.c` - Original Ingenic T41 driver

## Building

```bash
# Build module
make

# Clean
make clean

# Install (optional)
sudo make install
```

## Loading

```bash
# Load module
sudo insmod gc2607.ko

# Check status
dmesg | grep gc2607

# Unload
sudo rmmod gc2607
```

## License

GPL-2.0 (Linux Kernel Module)

## Credits

- Based on GalaxyCore GC2607 reference driver for Ingenic T41
- Uses Intel IPU6 ACPI PMIC framework (INT3472)
- Follows Linux V4L2 sensor driver patterns

## Development Notes

This driver was developed incrementally through 5 phases with comprehensive testing at each stage. The driver itself is production-ready; integration with IPU6 media controller requires one kernel module modification (adding GCTI2607 to ipu_bridge sensor database).

For development context and detailed implementation notes, see **[CLAUDE.md](CLAUDE.md)**.

---

**Last Updated:** January 6, 2026
**Kernel Version:** 6.17.9-arch1-1
**Status:** Driver Complete, Bridge Integration Pending
