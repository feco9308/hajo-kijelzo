# Review request: LS044Q7DH01 hardware bring-up

## Current status — 2026-11-10

**The LCD is not yet working reliably.** Successful build/flash or an OK HAL
return value is not proof of successful LCD communication.

- User-confirmed WeAct Studio STM32G4 Core Board, STM32G431CBU6.
- Sharp LS044Q7DH01, 320x240 monochrome Memory LCD.
- The user measured 5 V on both LCD VDD and VDDA.
- USB CDC enumerates; real diagnostic output was read successfully on Windows.
- ST-LINK reads showed test-stage progression, enabled TIM3, and changing CNT.
- The user reports whole-panel flicker/uneven response and sometimes an
  apparently frozen image. An early photo showed corrupted vertical patterns.
- Slowing SPI from 1 MHz to 250 kHz did not substantially improve the picture.
- No oscilloscope/logic-analyzer capture is available. Connector-level DISP
  and EXTMODE measurements are still missing.
- Neither a firmware error nor an electrical/contact fault is established as
  root cause. Original video/photo captures are not in this public repository.

## Current firmware behavior

`SHARP_SIMPLE_BLINK=ON` takes priority over the five-phase diagnostic option.
After initialization, DISP stays HIGH and a full white frame (stage 12),
2 s wait, full black frame (stage 13), 2 s wait repeat. Transfers and phase
alignment add time to the visible interval.

SPI1: mode 0, 8-bit hardware LSB-first, GPIO-controlled active-HIGH SCS,
currently 250 kHz. SCS/SCLK/SI are driven LOW between transactions.
TIM3_CH3/PB0 supplies nominal 1 Hz / 50% EXTCOMIN independently of main.
USB uses HSI48 + CRS; SYSCLK/SPI/TIM3 use HSI16.

## Files to review first

1. `Core/Src/sharp_ls044q7dh01.c`: command framing, bit order, line chaining,
   dummy clocks, SCS and power-up sequence.
2. `Core/Src/main.c`: SPI GPIO mux changes, HAL one-line TX, timer setup,
   COM-safe transaction window, tests and error handling.
3. `STM32G431CBU6_FLASH.ld` / `CMakeLists.txt`: target, startup and memory map.
4. `USB/Src/usb_diag.c` / `USB/Src/usbd_conf.c`: USB diagnostics and IRQ effects.

## Questions for reviewers

- Is the mode/address/pixel stream correct for **LS044Q7DH01**, specifically
  Sharp **LD-2025315 (2025-03-25)**?
- Are the 24-clock All Clear and single/multiline transfer endings correct?
- Could GPIO-to-SPI transitions or repeated HAL one-line transfers introduce
  unwanted SCLK/SI edges while SCS is HIGH?
- Is the selected power-up/EXTCOMIN/DISP sequence correct for the 2025 timing?
- Is the COM scheduling sufficient at 250 kHz, including USB IRQ overhead?
- Which connector-level measurement best separates protocol, COM timing,
  supply-integrity and signal-integrity faults?

The 2025 specification was supplied locally and is not redistributed here.
`DATASHEET_AUDIT.md` is a prior source-level audit, not proof of validated
hardware. Older reports/docs include historical settings and sizes; source
and CMake defaults define the current firmware.

## Evidence limits

`hal=0` / `spi_err=0` only show that STM32 SPI transmission completed.
The LCD interface has no receive acknowledgement. MCU GPIO readback does not
measure the signal at the LCD connector. One-second USB sampling can show
the same phase of a 1 Hz timer repeatedly due to aliasing.

When reviewing, report the commit, board revision, cable lengths, measured
connector levels, test stage and waveform captures alongside a proposed fix.
