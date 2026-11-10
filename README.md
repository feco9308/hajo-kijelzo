# Hajó kijelző — line-by-line black/white sweep

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
