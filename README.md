# Hajó kijelző — minimal black/white test

Branch: `test/minimal-black-white`.

This branch runs only the requested bring-up sequence:

```text
STM32 startup
SPI1 normal 2-line master, 1 MHz, mode 0, LSB first
TIM3 EXTCOMIN hardware 1 Hz / 50% starts
All Clear
DISP HIGH
full black -> wait 2 s -> full white -> wait 2 s -> repeat
```

There is no USB, mode selection, checkerboard call, framebuffer or UI in the
application. `Core/Src/main.c` contains the entire application loop.
The older USB source and reports remain as reference files but are not built.

## Hardware

WeAct Studio STM32G4 Core Board, STM32G431CBU6.
Sharp LS044Q7DH01, 320x240 monochrome Memory LCD.

| STM32 | LCD | Mode |
|---|---|---|
| PA5 | SCLK | permanent AF5 SPI1, 1 MHz |
| PA7 | SI/MOSI | permanent AF5 SPI1 |
| PA4 | SCS | GPIO, active HIGH |
| PB0 | EXTCOMIN | AF2 TIM3_CH3, nominal 1 Hz / 50% |
| PB1 | DISP | GPIO, LOW at startup, then continuously HIGH |
| — | EXTMODE | fixed to LCD VDD, 5 V |
| — | VDD / VDDA | both 5 V |
| — | VSS / VSSA | common GND |

MCU power and signals are 3.3 V. PA6/MISO stays analog and unconnected.
External 10 kΩ pulldowns on the five LCD control signals provide LOW during
MCU reset. Stable LCD power and correct supply sequencing are hardware
prerequisites; the firmware has no LCD power switch or power-good signal.

## Initialization details

EXTCOMIN starts before All Clear, while DISP remains LOW. After Clear and
its guard delay, DISP goes HIGH. Startup waits for the next external COM
rising edge and its initialization guard before the first image write.
The driver retains the datasheet COM-safe phase guard around transactions.
Image transfer and phase alignment add time to the 2 s application waits.

The Sharp framing is unchanged: active-HIGH SCS, LSB-first mode/address/pixel
bytes, 24 clocks for All Clear, 16 transfer clocks after the last line.
Normal transactions never disable SPI or change PA5/PA7 GPIO modes.

## Build and flash

```sh
git clone --branch test/minimal-black-white --recurse-submodules https://github.com/feco9308/hajo-kijelzo.git
cd hajo-kijelzo
```

Dependencies are pinned Git submodules. Windows bootstrap if required:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\bootstrap.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\flash.ps1
```

Build requires Arm GNU Toolchain, CMake and Ninja. Flash uses ST-LINK and
STM32CubeProgrammer and replaces the existing firmware. VS Code Ctrl+Shift+B
runs the build. Artifacts are under `build-st/`.

ST startup/linker target STM32G431CBU6: 128 KiB flash, 22 KiB contiguous SRAM;
initial SP=0x20005800. CCM RAM is unused. HSI16 supplies SYSCLK/SPI/TIM3.

## Validation and current fault

Build: 8080 B flash, 2248 B SRAM including 2 KiB stack reserve, no compiler or
linker warnings. USB/PCD/CDC and checkerboard symbols are absent from the ELF.
The LCD's actual optical response still needs confirmation.

No COM port is expected in this test. ST-LINK variables:
`test_stage=1` initialization, `13` black, `12` white,
`0xFFFFFFFF` error. `last_hal_status=0` means HAL_OK, not LCD acknowledgement.

Before this branch, the user observed uneven whole-panel flicker and sometimes
an unchanged/corrupt image. The 5 V rails were measured; connector-level
DISP/EXTMODE/EXTCOMIN/SPI timing still needs verification if the issue persists.
Older reviews and reports describe earlier firmware settings, not this exact
minimal test. The prior change is on `fix/minimal-spi-bringup`; the original
USB diagnostic baseline is on `main`.
