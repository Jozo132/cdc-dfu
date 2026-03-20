# Blinky — STM32F401CC (Black Pill)

Minimal blinky example using USB CDC serial and the `cdc-dfu` library for automatic DFU uploads.

## Usage

1. Open this folder in PlatformIO
2. First flash via hardware DFU (hold BOOT0, press RST) or ST-Link
3. After that, just run `pio run --target upload` — DFU is triggered automatically

The on-board LED (PC13) toggles every 500 ms and status is printed over USB CDC serial.
