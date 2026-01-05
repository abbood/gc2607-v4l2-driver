# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This project is porting the GalaxyCore GC2607 camera sensor driver from the Ingenic T41 platform (MIPS embedded) to the Linux V4L2 subsystem for Intel IPU6 on x86_64.

**Target Hardware:**
- Laptop: Huawei MateBook Pro VGHH-XX
- Sensor: GC2607 (1920x1080@30fps, MIPI CSI-2, RAW10)
- Platform: Intel IPU6
- PMIC: INT3472 discrete (intel_skl_int3472_discrete driver)
- I2C Bus: /dev/i2c-5
- I2C Address: 0x37
- Chip ID: 0x2607 (registers 0x03f0=0x26, 0x03f1=0x07)

**ACPI Matching:**
- Device name: GCTI2607:00
- Modalias: `acpi:GCTI2607:GCTI2607:`
- The driver must use ACPI match table with "GCTI2607" HID

## Architecture

### Reference Driver (reference/gc2607.c)
The original Ingenic T41 driver uses platform-specific APIs:
- `tx-isp-common.h`, `sensor-common.h`: T41 ISP framework (NOT available on standard Linux)
- `private_i2c_transfer()`, `private_gpio_request()`: T41-specific wrappers
- Platform device registration with `tx_isp_subdev` abstraction

### Target V4L2 Driver Structure
The new driver must:
1. Use standard Linux V4L2 subdev APIs (`v4l2_i2c_subdev_init`, etc.)
2. Register as I2C client driver with ACPI match table
3. Implement V4L2 subdev ops (core, video, pad)
4. Use standard GPIO/regulator APIs (via INT3472 PMIC)
5. Support async subdev registration for IPU6 integration
6. Implement MIPI CSI-2 bus configuration via V4L2 controls

### Key Hardware Configuration
From reference driver analysis:
- MIPI: 2 lanes, 672 Mbps/lane, RAW10 format
- Resolution: 1920x1080
- Frame timing: HTS=2048, VTS=1335 (30fps)
- Register addressing: 16-bit addresses, 8-bit values
- Gain control: Analog gain LUT with 17 entries (1x to 15.8125x)

## Development Workflow

### Building the Driver
```bash
# Out-of-tree build against running kernel
make

# Install module (requires sudo)
sudo make install

# Clean build artifacts
make clean
```

### Testing the Driver
```bash
# Load the module
sudo insmod gc2607.ko

# Check if driver probed successfully
dmesg | grep gc2607

# Verify I2C device binding
ls -l /sys/bus/i2c/drivers/gc2607/

# Check V4L2 subdev registration
v4l2-ctl --list-subdevs

# Unload module
sudo rmmod gc2607
```

### Hardware Verification Commands
```bash
# Check ACPI device status (15 = enabled)
cat /sys/bus/acpi/devices/GCTI2607*/status

# Verify I2C bus
ls /dev/i2c-5

# Check PMIC driver loaded
lsmod | grep int3472

# Monitor kernel logs during driver load
sudo dmesg -w
```

## Implementation Phases

**Phase 1 (Current):** Skeleton driver
- I2C client registration with ACPI matching
- Basic probe/remove with logging
- Module metadata

**Phase 2:** Power management
- INT3472 PMIC integration (GPIOs, regulators, clocks)
- Power on/off sequences

**Phase 3:** Sensor initialization
- Register initialization from reference driver tables
- Chip ID detection

**Phase 4:** V4L2 integration
- Subdev ops implementation
- Format/resolution negotiation
- MIPI CSI-2 configuration

**Phase 5:** Streaming
- Start/stop stream operations
- Exposure/gain controls

## Important Reference Drivers

**In-tree GalaxyCore drivers:**
- `drivers/media/i2c/gc2145.c`: Similar GalaxyCore sensor with V4L2 structure
- `drivers/staging/media/atomisp/i2c/atomisp-gc0310.c`: ACPI-based GalaxyCore driver

**IPU6 sensor examples:**
- `drivers/media/i2c/ov01a10.c`: Modern sensor with IPU6 support
- Check drivers with `v4l2_async_register_subdev()` for async registration pattern

## Key Differences from Reference Driver

| Aspect | T41 Reference | V4L2 Target |
|--------|---------------|-------------|
| I2C API | `private_i2c_transfer()` | `i2c_transfer()` |
| Subdev | `tx_isp_subdev` | `v4l2_subdev` |
| Power | Direct GPIO control | INT3472 PMIC subsystem |
| Registration | Platform device | I2C driver + async subdev |
| Bus config | `gc2607_mipi` struct | V4L2_CID_LINK_FREQ, endpoint props |

## Register Map Reference
- Chip ID: 0x03f0 (high byte), 0x03f1 (low byte)
- Exposure: 0x0202 (high), 0x0203 (low)
- Analog gain: 0x02b3, 0x02b4, 0x020c, 0x020d (4-register LUT index)
- VTS (frame length): 0x0220 (high), 0x0221 (low)
- HTS (line length): 0x0342 (high), 0x0343 (low)
- Init sequence: 280 register writes in `gc2607_init_regs_1920_1080_30fps_mipi[]`
