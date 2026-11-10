# Hajó kijelző / Boat display bring-up

Minimal STM32 HAL firmware for a **WeAct Studio STM32G4 Core Board
(STM32G431CBU6)** driving a **Sharp LS044Q7DH01** 320x240 monochrome Memory LCD.

**Status: hardware bring-up in progress. The display is not yet reliable.**
Build, ST-LINK flashing and USB CDC logging work, but the panel shows uneven
flicker / corrupted or unchanged images. Start with [review notes](REVIEW_NOTES.md)
for the symptoms, evidence limits and specific review questions.

Magyarul: a hajókijelző első hardvertesztje, jelenleg hibakeresési állapotban.
A kód átnézéséhez a [REVIEW_NOTES.md](REVIEW_NOTES.md) az ajánlott kezdőpont.

## Hardware

| STM32 | LCD | Configuration |
|---|---|---|
| PA5 | SCLK | SPI1, mode 0, currently **250 kHz** |
| PA7 | SI/MOSI | SPI1, hardware LSB first |
| PA4 | SCS | GPIO, **active HIGH**, not hardware NSS |
| PB0 | EXTCOMIN | AF2 TIM3_CH3, nominal 1 Hz, 50% duty |
| PB1 | DISP | GPIO; initially LOW, then HIGH |
| — | EXTMODE | Fixed directly to LCD VDD |
| — | VDD + VDDA | 5 V |
| — | VSS + VSSA | Common GND |

MCU supply/output logic: 3.3 V. External 10 kΩ pulldowns on the five MCU-driven
LCD signals are recommended for LOW levels during reset. Supply sequencing is
hardware-dependent: this configuration has no LCD power switch/power-good input.
USB CDC uses the native PA11/PA12 USB-C connection, without a USB-UART adapter.

## Scope and default test

Own small C driver; no framebuffer, fonts, CAN or graphical UI.
API: `sharp_init`, `sharp_clear`, `sharp_fill`, `sharp_write_line`,
`sharp_test_pattern`. Returns HAL status codes. Line numbers **1..240**,
40 bytes/line, bit0 first; 1=white, 0=black.

Power-up: safe LOW pins, All Clear, guard delay, start hardware EXTCOMIN timer,
then DISP HIGH. Default test: **2 s white / 2 s black**, repeated. Image transfer
and COM-phase alignment add to the visible interval.

`SHARP_SIMPLE_BLINK=ON` is the current default. With it OFF,
`SHARP_DIAGNOSTIC_LOOP=ON` selects DISP/clear/white/black/checkerboard phases.
With both OFF, the original one-shot white/black/checkerboard test runs.
The original 1 MHz SPI was reduced to 250 kHz as a diagnostic trial; it did not
resolve the reported panel problem.

## Clone and dependencies

```sh
git clone --recurse-submodules https://github.com/feco9308/hajo-kijelzo.git
cd hajo-kijelzo
```

Four ST/ARM dependencies are Git submodules pinned to exact commits.
If cloned without submodules:

```sh
git submodule update --init --depth 1
```

Windows `scripts/bootstrap.ps1` also limits CMSIS checkout to the needed Core
headers. Third-party licenses remain in their modules; ST example descriptor
attribution is retained under `USB/`.

## Build

Requirements: Arm GNU Toolchain (`arm-none-eabi-gcc`, newlib-nano), CMake >=3.22,
Ninja. Tested: ST GCC 14.3.1, CMake 4.0.0, Ninja 1.13.2.

Windows / VS Code (`Ctrl+Shift+B`):

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1
```

The script discovers STM32 VS Code tool bundles, with a PATH fallback.
Generic command-line build:

```sh
cmake -S . -B build -G Ninja
cmake --build build
```

On Windows, `-DARM_GCC_BIN=C:/path/to/toolchain/bin` selects an explicit compiler.
Outputs: `sharp_lcd.elf`, `.hex`, `.bin`, `.map`. The Windows script uses
`build-st/`; the generic example uses `build/`.

Target: ST `startup_stm32g431xx.s`, `STM32G431xx`, Cortex-M4F, hard-float ABI.
Linker: 128 KiB flash at 0x08000000, 22 KiB contiguous SRAM at 0x20000000,
initial SP 0x20005800. Separate 10 KiB CCM SRAM is unused.

## Flash and USB log

ST-LINK: SWDIO→PA13, SWCLK→PA14, common GND, 3.3 V reference, NRST recommended.
Flash **replaces the current firmware**:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\flash.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\monitor.ps1 -Port COM10 -Seconds 20
```

COM10 is development-machine-specific. Terminal: 115200 8N1, no flow control,
DTR enabled. [USB diagnostics](USB_DIAGNOSTICS.md) explains the log fields.

## Documentation

- [Review request / current fault](REVIEW_NOTES.md)
- [Diagnostic test modes](DIAGNOSTIC.md)
- [USB diagnostics](USB_DIAGNOSTICS.md)
- [Prior datasheet audit](DATASHEET_AUDIT.md)
- [Historical build report](BUILD_REPORT.md)
- [Earlier Hungarian development notes](docs/DEVELOPMENT_NOTES.hu.md)

Primary display reference: Sharp **LD-2025315, 2025-03-25**, supplied locally.
The PDF and video captures are not included in this source repository.
