# Sailing dashboard demonstration

Active branch `feat/sailing-dashboard-demo`. Startup displays a static
320x240 sailing instrument screen marked DEMO. All values are examples:
AWA +35 degrees, AWS 14.2kn, TWS 10.1kn, SOG 5.6kn, COG 247 degrees,
XTE 0.02NM and waypoint WP01 at 2.4NM. No sensors supply these values yet.
USB `boat` / `vitorlas`, or ST-LINK `-Command boat`, redraws the screen.
Rendering uses a 9600-byte monochrome framebuffer and compact 5x7 glyphs.
The display driver, SPI timing and verified pin mapping are retained.
Build uses 27944 bytes Flash and 15104 bytes SRAM including stack reserve.
Earlier sections below document the hardware test commands still available.

On branch `test/checkerboard`, USB `checkerboard` (alias `sakk`) and
ST-LINK `scripts/stlink-command.ps1 -Command checkerboard` display static
16x16-pixel squares, 20 columns by 15 rows, top-left white. The command
stops automatic white/black updates. After reset the automatic test starts.

Active branch `test/automatic-black-white`: starts on power-up and repeats
full white (hold 2 seconds), full black (hold 2 seconds). Frame transfer
time is added between holds. USB `stop` holds the image, `auto` restarts.
Wiring: PA5=SCLK, PA7=SI, PA4=active-HIGH SCS, PB0=EXTCOMIN,
PB1=DISP; LCD pin8 EXTMODE=5V, VDD/VDDA=5V, common GND.
Hardware SPI1 mode0, 2LINES, LSB-first, 250kHz; PB0 timer 1Hz/50%.
The build and flash reset restore PA7 AF5 and clear any static pin test.
The sections below describe the historical manual test base.

## Active diagnostic: 250 kHz, separate transaction for every row

Same wiring and external COM startup as the previous Techtoys-style test:
PB0 -> LCD pin4 EXTCOMIN (TIM3_CH3, 1Hz/50%), PB1 -> pin5 DISP,
pin8 EXTMODE -> VDD/5V. PA4=SCS active HIGH, PA5=SCLK, PA7=SI.
Two All Clears at startup; M1 stays zero. USB commands are unchanged.

Only the SPI speed and image-write transaction structure changed:
16MHz / 64 = 250kHz; each row sends 01, row address (1..240), 40 pixel
bytes, 00, 00 under its own SCS HIGH pulse. 44 bytes = 352 clocks =
1.408ms per row, approximately 338ms per image plus software/COM guards.
The COM guard avoids CNT497..501 around the rising edge at CNT500.
No continuous multi-row SCS pulse remains. Pixel patterns are unchanged.
This is a diagnostic comparison, not verified working LCD operation.
Earlier sections below describe historical firmware.

## Active test: Techtoys-style external COM startup

LCD pin8 EXTMODE -> VDD (5V), pin4 EXTCOMIN -> PB0 (TIM3_CH3 AF2),
pin5 DISP -> PB1. PA5=SCLK, PA7=SI, PA4=active-HIGH SCS unchanged.
Do not ground pin4: it is now driven by a timer. Change pin8 only with power off.
Startup: 100ms rail guard, two 24-clock All Clears, 50ms guard, DISP HIGH,
1ms guard, start EXTCOMIN 1Hz/50%, wait first rising edge plus 2ms.
M1 remains zero. SPI stays mode0, LSB-first, 1MHz. USB commands retained.
Full image and row transactions start in the safe COM window; row sweep
therefore has pauses near COM cycles. Hardware verification is pending.
Earlier sections below describe historical builds, not this active profile.

## Active firmware: EXTMODE grounded, serial COM inversion

LCD pin 8 EXTMODE must be GND. PB1 -> DISP (pin 5), PB0 -> EXTCOMIN
(pin 4), held LOW. PA4 -> active-HIGH SCS, PA5 -> SCLK, PA7 -> SI.
VDD/VDDA remain 5 V. Change wiring only with LCD power off.

The main loop sends a 24-clock hold packet every 500 ms, alternating M1
(00 00 00 / 02 00 00, LSB-first). This gives a nominal 1 Hz COM cycle.
Clear and pixel writes carry the same current M1 value; USB commands do not
independently flip it. COM service runs in manual, stop and off states.
Blocking full-frame writes can delay a COM edge by approximately 81 ms;
this version uses main-loop service, not a hard real-time timer SPI interrupt.
USB commands remain: clear black white pattern invert off on auto stop status help.
`pattern` is vertical bars. The older notes below describe historical firmware,
including EXTMODE=5V builds, and do not override this active wiring.

﻿# Hajó kijelző — line-by-line black/white sweep

Current branch: `feat/usb-control-swapped-pins`.

**USB manual mode is now the default.** Wiring remains PB0=DISP,
PB1=EXTCOMIN/TIM3_CH4, EXTMODE pin8=5 V. There are no automatic image changes
after startup Clear. The historical sweep below runs only after `auto`.

## USB commands

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\usb-console.ps1 -Port COM10
```

Type one command + Enter and wait for OK/ERR. `exit` closes the console.
The console reconnects after USB loss without replaying commands. One program
can open the port at a time; the COM number may differ on another machine.

| Command | Effect |
|---|---|
| clear / torles | All Clear, white memory |
| black / fekete | full black |
| white / feher | full white |
| pattern / invert | normal / inverted vertical bars |
| off / on | DISP off/on, preserving image memory |
| auto | start repeating line-by-line black/white sweep |
| stop | stop sweep, hold the current partial image |
| status | report state without changing the image or stopping auto |
| help | show commands |

Image/off/on commands stop auto. Invalid input does not change auto state.
Auto is a state machine, servicing USB between lines and during holds. Stop
takes effect after an executing packet/COM guard. A full-frame command may
wait up to about 0.8 s; on/auto can also wait for a safe COM rising edge.

Status includes stage, last auto line, auto flag, MCU-side DISP/EXTCOMIN,
configured extmode=1, TIM3, HAL/SPI errors, SPI clock and dropped RX count.
OK is an MCU result, not LCD acknowledgement. RX supports 47 ASCII characters
per CR/LF-terminated command, with three queued slots; overlong input is
rejected. All LCD/SPI work runs in main, not in the USB IRQ.

Current build: 25956 B flash, 5488 B SRAM including stack reserve, no compiler
or linker warnings. Flash/readback and real COM10 tests passed: all commands,
fragmented RX/CRLF, case/space handling, invalid/overlong input, TIM3 with DISP
off, auto progression, stop holding the line, and manual image hold.
`scripts/test-usb-manual.ps1` repeats the test and leaves the image cleared.

One-shot command: `scripts/usb-console.ps1 -Port COM10 -Command clear`.
Terminal: 115200 8N1, DTR on, no flow control.
Source: `USB/Src/usb_manual.c` and `USB/Src/usb_diag.c`.

## Historical USB-free sweep below

The pin mapping and LCD timing remain the same in USB mode; external COM
continues from hardware independently of USB. Size/no-USB details below
describe the prior `test/line-by-line` revision.

The test clears the panel to white, then cumulatively writes one black line
at a time, addresses 1..240. After 2 s full black it writes the same lines
white, one at a time, then holds full white for 2 s and repeats.
Each line has a 40 ms application delay: a sweep takes about 10 seconds.
The physical sweep direction follows the panel's native line orientation;
rotating the panel can make the boundary move sideways rather than downward.

Only a 40-byte row buffer is used. Every row is a separate active-HIGH SCS
transaction, sent through `sharp_write_line`, with 320 pixels and 16 final
dummy clocks. The driver uses a short COM-safe window for these ~352 us
packets so the sweep does not pause at the broad full-frame timing window.

## Current, user-confirmed wiring

| MCU | LCD | Mode |
|---|---|---|
| PA5 | SCLK | AF5 SPI1, 1 MHz, mode 0 |
| PA7 | SI/MOSI | AF5 SPI1, hardware LSB-first |
| PA4 | SCS | active-HIGH GPIO |
| PB0 | DISP, LCD pin 5 | LOW at startup, then continuously HIGH |
| PB1 | EXTCOMIN, LCD pin 4 | AF2 TIM3_CH4, nominal 1 Hz, 50% |
| — | EXTMODE, LCD pin 8 | fixed to LCD VDD / 5 V |
| — | VDD and VDDA | 5 V |
| — | VSS and VSSA | common GND |

MCU supply/signals are 3.3 V; PA6/MISO is analog and unconnected. PA5/PA7 stay
AF5 throughout normal operation. No USB or framebuffer is built.
The LCD power must already be stable. Hardware EXTCOMIN runs through all holds.

## Debugger state

- `test_stage=20`: clear/white starting hold.
- `21`: writing black lines; `test_line` is the current address 1..240.
- `22`: full black hold.
- `23`: writing white lines; `test_line` is the current address.
- `24`: full white hold.
- `0xFFFFFFFF`: fatal error; DISP forced LOW.
- `last_hal_status=0`: HAL transmit completed, not an LCD acknowledgement.

Observe whether the same bad line/points persist while the sweep crosses
those positions. Actual panel behavior is not validated by the MCU counters.

## Build and flash

```sh
git clone --branch test/line-by-line --recurse-submodules https://github.com/feco9308/hajo-kijelzo.git
```

```powershell
.\scripts\bootstrap.ps1
.\scripts\build.ps1
.\scripts\flash.ps1
```

Flash replaces the existing firmware. Artifacts: `build-st/`.
Target: STM32G431CBU6, ST startup, 128 KiB flash / 22 KiB contiguous SRAM,
initial SP=0x20005800. Build: 8220 B flash, 2256 B SRAM including stack reserve;
no compiler/linker warnings.

Earlier docs describe historical modes and wiring; this README defines the
current line-sweep test. Previous stripe test: `test/swapped-pins-stripes`.
