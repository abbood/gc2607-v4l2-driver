# GC2607 V4L2 Driver for Linux

Port of GalaxyCore GC2607 camera sensor driver from Ingenic T41 to Linux V4L2 subsystem.

## Target Hardware
- **Laptop**: Huawei MateBook Pro VGHH-XX
- **Sensor**: GalaxyCore GC2607 (1920x1080@30fps)
- **Interface**: MIPI CSI-2
- **Platform**: Intel IPU6
- **PMIC**: INT3472 (discrete)

## Current Status
- [x] Hardware detection (ACPI device exists, I2C bus ready)
- [x] Original T41 driver reference collected
- [ ] V4L2 skeleton driver
- [ ] Power management (INT3472 integration)
- [ ] Register initialization
- [ ] Streaming support
- [ ] IPU6 integration

## Build Instructions
TBD

## Hardware Info
```
ACPI Device: GCTI2607:00 (status=15, enabled)
I2C Bus: /dev/i2c-5
Modalias: acpi:GCTI2607:GCTI2607:
PMIC: INT3472:01 (intel_skl_int3472_discrete)
```

## References
- Original driver: https://github.com/iesah/IPC-SDK/tree/master/opensource/drivers/sensors-t41/gc2607
- Upstream issue: [link to your GitHub issue]
