# Hajó kijelző — swapped-pin stripe test

Branch: `test/swapped-pins-stripes`.

**User-confirmed current wiring:** PB0 goes to LCD DISP (pin 5), PB1 goes to
LCD EXTCOMIN (pin 4), EXTMODE (pin 8) is tied to LCD VDD / 5 V.
The firmware is deliberately configured for this swapped wiring.

| MCU | LCD | Configuration |
|---|---|---|
| PA5 | SCLK | SPI1 AF5, 1 MHz, mode 0 |
| PA7 | SI | SPI1 AF5, hardware LSB-first |
| PA4 | SCS | GPIO, active HIGH |
| **PB0** | **DISP, LCD pin 5** | GPIO, LOW at startup, then continuously HIGH |
| **PB1** | **EXTCOMIN, LCD pin 4** | **AF2 TIM3_CH4**, 1 Hz / 50% duty |
| — | EXTMODE, LCD pin 8 | fixed to 5 V / LCD VDD |
| — | VDD and VDDA | 5 V |
| — | VSS and VSSA | common GND |

MCU supply/signals remain 3.3 V; MISO/PA6 is unconnected. SPI is normal 2-line
master. PA5/PA7 remain AF5. Only SCS toggles for a packet. Zero dummy data and
CPOL=0 keep SI/SCLK LOW at idle. External pulldowns are needed during reset.

## Repeating sequence

After safe GPIO initialization, hardware EXTCOMIN start, All Clear and DISP
HIGH, the firmware repeats:

1. Full white, hold 3 s (stage 12).
2. Full black, hold 3 s (stage 13).
3. Vertical bars, 8 pixels wide, hold 3 s (stage 14).
4. Inverted bars, hold 3 s (stage 15).

There are 40 alternating bars across 320 pixels, identical across all 240 rows.
Transfer/COM-phase alignment adds time to the holds. The loop does not stop.
DISP is continuously HIGH after startup; it is not the 1 Hz signal.
No USB, console or framebuffer is built. EXTMODE=HIGH means M1 stays 0.

## Build / flash

```sh
git clone --branch test/swapped-pins-stripes --recurse-submodules https://github.com/feco9308/hajo-kijelzo.git
```

```powershell
.\scripts\bootstrap.ps1
.\scripts\build.ps1
.\scripts\flash.ps1
```

Flashing replaces the current firmware. ST-LINK, Arm GNU Toolchain, CMake and
Ninja required. Output under `build-st/`. Target: STM32G431CBU6, 128 KiB flash,
22 KiB contiguous SRAM, initial SP=0x20005800. Build: 8136 B flash, 2248 B SRAM
including stack reserve, no compiler/linker warnings.

The original firmware expected PB0=EXTCOMIN and PB1=DISP. With physically
swapped wires, that old program could route 1 Hz to DISP and cause whole-panel
blinking. A visible response in that state alone does not validate image data.
This revision puts the PWM on PB1/TIM3_CH4 and stable DISP on PB0.

Older reports/docs describe historical wiring and modes. This README defines
the current configuration. Actual optical success still requires observation.
