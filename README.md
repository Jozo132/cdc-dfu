# cdc-dfu

Zero-overhead DFU bootloader jump for STM32F4 via USB CDC. Add the library dependency and uploads "just work" over USB — no `#include`, no BOOT0 pins, no ST-Link, no extra scripts.

## How it works

1. **Host side**: Before uploading, PlatformIO toggles DTR 5 times and sends `"DFU!"` magic bytes over the CDC serial port
2. **Firmware side**: The STM32 framework's `DTR_TOGGLING_SEQ` mechanism calls `dtr_togglingHook()` from the USB interrupt — zero polling, zero timers
3. **ISR → Reset → Bootloader**: The hook disconnects USB, writes a magic value to `.noinit` RAM, and triggers `NVIC_SystemReset()`. A C++ constructor (Thread mode) catches the flag and jumps to the ROM DFU bootloader
4. **Flash**: `dfu-util` uploads the new firmware

## Quick start

### platformio.ini

```ini
[env:myboard]
platform = ststm32
board = genericSTM32F401RC
framework = arduino
upload_protocol = dfu
lib_deps = https://github.com/Jozo132/cdc-dfu.git
build_flags =
    -D PIO_FRAMEWORK_ARDUINO_ENABLE_CDC
    -D USBCON
    -D USBD_USE_CDC
    -D HAL_PCD_MODULE_ENABLED
```

That's it — no `extra_scripts` line needed. The library auto-injects the DFU trigger via `library.json` → `build.extraScript`.

### main.cpp

```cpp
void setup() {
    Serial.begin(115200);
}

void loop() {
    // Your application code — no #include, no polling needed
}
```

No `#include` required. The library's extra script compiles the DFU firmware code and injects the upload trigger automatically.

### First flash

The very first time you need to flash via hardware DFU (hold BOOT0, press RST) or ST-Link. After that, every subsequent `pio run --target upload` triggers DFU automatically.

## Requirements

- STM32F4 with USB OTG FS (e.g., F401, F411, F405, F407, F446)
- Arduino framework (framework-arduinoststm32)
- USB CDC enabled (`USBCON`, `USBD_USE_CDC`, `PIO_FRAMEWORK_ARDUINO_ENABLE_CDC`)

## What the library provides

| File | Purpose |
|------|---------|
| `src/cdc_dfu.cpp` | Firmware implementation: constructor + ISR hook |
| `src/cdc_dfu.h` | Backward-compatibility stub (inclusion not required) |
| `extra_script.py` | Force-compiles firmware code + auto-configures DFU trigger before upload |
| `library.json` | Library metadata + loads extra script |

## How the auto-configuration works

The library's `library.json` declares `build.extraScript`, which PlatformIO runs during library processing. The script:

1. **Compiles the firmware code**: Uses `BuildSources()` to compile `cdc_dfu.cpp` directly into the firmware build, bypassing PlatformIO's LDF scanner (which skips libraries that nothing `#include`s from)
2. **Adds build flags**: Injects `-D DTR_TOGGLING_SEQ` so the framework enables the DTR toggle hook
3. **Registers the upload trigger**: When `upload_protocol = dfu`, registers a pre-upload action on `DefaultEnvironment()` that toggles DTR + sends magic bytes to enter the bootloader before `dfu-util` runs

## License

MIT
