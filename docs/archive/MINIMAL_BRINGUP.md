# Minimal bring-up review — 2026-11-10

Branch: `fix/minimal-spi-bringup`, based on reviewed commit `65279da`.

## Changes

- `SPI_DIRECTION_2LINES` instead of `SPI_DIRECTION_1LINE`. RX is unused and
  PA6/MISO remains at its reset analog setting, physically unconnected.
  The local ST HAL confirms 1LINE disables SPI and sets BIDIOE before each
  `HAL_SPI_Transmit`, while 2LINES does not. The expected receive overrun in
  transmit-only use of 2LINES is cleared by the HAL at transfer completion.
- SPI is enabled once during initialization, then PA5/PA7 are set to AF5
  once. Normal transactions no longer disable SPI or change GPIO modes.
  The only normal per-packet GPIO switching is PA4/SCS (active HIGH).
- CPOL=0 keeps SCLK LOW, and every successful packet ends in zero dummy bits
  so MOSI remains LOW. Initial GPIO latches are LOW before AF setup. Fatal
  errors are the sole exception: SPI is disabled and SCLK/SI forced GPIO LOW.
- New `SHARP_USB_DIAG` option defaults OFF. USB source files and PCD are not
  linked, PCD HAL is not enabled, USB init and polling are compiled out.
- Default test: initialization/All Clear, hardware 1 Hz EXTCOMIN, DISP HIGH,
  black for 2 s, white for 2 s, repeat. No checkerboard or USB in this mode.
  Only SysTick timing and the hardware TIM3 output are needed.
- Frequency remains **250 kHz** (16 MHz / 64). The protocol, 24-clock Clear,
  row encoding, final dummy clocks and existing external-COM phase guard are
  unchanged. Returning to 1 MHz is a separate experiment.

## Build

Default minimal configuration:

```powershell
cmake -S . -B build-st -DSHARP_USB_DIAG=OFF -DSHARP_SIMPLE_BLINK=ON
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1
```

Alternative USB build: configure with `-DSHARP_USB_DIAG=ON` and rebuild.
Both options can be selected using a separate build directory. CMake caches
option values, so use an explicit OFF when reusing an earlier USB-enabled tree.

## Validation and limits

Minimal build: 8072 B flash, 2248 B allocated SRAM including 2 KiB stack reserve.
Build uses `-Wall -Wextra -Werror`. No USB/PCD/CDC functions in the minimal ELF.
The optional USB build is also compiled to check the configuration guards.
Both builds passed without compiler/linker warnings. Optional USB build size:
23252 B flash, 4968 B SRAM including stack reserve.
ST startup and linker remain targeted to STM32G431CBU6, 128 KiB flash / 22 KiB
contiguous SRAM, initial SP 0x20005800.

The minimal image was flashed, readback-verified and reset successfully.
Post-flash ST-LINK observations:

- `last_hal_status=0`; `test_stage` changed from 13 (black) to 12 (white).
- SPI1 CR1=0x03EC at both observations: SPE enabled, BIDIMODE=0,
  master/LSB-first/mode 0 and divider 64.
- SPI1 SR=0x02 in the idle observation: TXE set, BSY and OVR clear.
- GPIOA MODER confirms PA4 GPIO output, PA5/PA7 alternate function,
  PA6 analog. AFRL confirms AF5 on PA5/PA7.
- GPIOA IDR shows PA4/PA5/PA7 LOW during the idle observation.

These are MCU-side observations; no optical success or connector-level
waveform has yet been confirmed for this revision.

Successful build/flash and MCU readback do not prove the LCD input waveform.
An improvement in the physical image is still to be verified by observation.
Probe DISP, EXTMODE, EXTCOMIN, SCS, SCLK and SI at the **LCD connector** if the
problem remains. In particular, swapped PB0/PB1 or FPC numbering could route
the 1 Hz signal to DISP; this is a hypothesis, not a confirmed wiring fault.
