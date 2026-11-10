# Hajó kijelző — line-by-line black/white sweep

Branch: `test/line-by-line`.

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
