$ErrorActionPreference = 'Stop'
$projectDir = Split-Path $PSScriptRoot -Parent
$cli = 'C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe'
$elf = Join-Path $projectDir 'build-st\sharp_lcd.elf'
if (!(Test-Path -LiteralPath $elf)) { throw 'Eloszor futtasd a scripts/build.ps1 fajlt.' }
# Explicit user-run operation. Replaces existing firmware. NRST wiring recommended.
& $cli -c port=SWD freq=1000 mode=UR -d $elf -v -rst
if ($LASTEXITCODE -ne 0) { throw 'Flash/verify failed' }
