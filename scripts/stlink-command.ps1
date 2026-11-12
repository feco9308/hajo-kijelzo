param(
    [ValidateSet('status','clear','white','black','pattern','invert','checkerboard','on','off','stop')]
    [string]$Command = 'status'
)
$ErrorActionPreference = 'Stop'
$projectDir = Split-Path $PSScriptRoot -Parent
$cli = 'C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe'
$elf = Join-Path $projectDir 'build-st\sharp_lcd.elf'
$nm = Get-ChildItem (Join-Path $env:LOCALAPPDATA 'stm32cube\bundles\gnu-tools-for-stm32') -Recurse -Filter arm-none-eabi-nm.exe |
    Sort-Object FullName -Descending | Select-Object -First 1
if (!$nm -or !(Test-Path -LiteralPath $elf)) { throw 'Build the matching firmware first.' }
$symbols = & $nm.FullName -n $elf
if ($LASTEXITCODE -ne 0) { throw 'Cannot read ELF symbols.' }
$symbol = $symbols | Select-String '^([0-9a-fA-F]+)\s+\w\s+lcd_debug$'
if (!$symbol) { throw 'ELF has no lcd_debug mailbox.' }
$address = '0x' + $symbol.Matches[0].Groups[1].Value
function Read-Mailbox {
    $output = (& $cli -c port=SWD freq=1000 mode=HOTPLUG -r32 $address 16 2>&1) -join "`n"
    if ($LASTEXITCODE -ne 0) { throw "SWD read failed: $output" }
    $match = [regex]::Match($output, '(?im)^' + [regex]::Escape($address) + '\s*:\s*([0-9a-f]{8})\s+([0-9a-f]{8})\s+([0-9a-f]{8})\s+([0-9a-f]{8})')
    if (!$match.Success) { throw "Cannot parse mailbox: $output" }
    return [pscustomobject]@{
        request = [Convert]::ToUInt32($match.Groups[1].Value,16)
        result = [Convert]::ToUInt32($match.Groups[2].Value,16)
        completed = [Convert]::ToUInt32($match.Groups[3].Value,16)
        last_command = [Convert]::ToUInt32($match.Groups[4].Value,16)
    }
}
$before = Read-Mailbox
if ($Command -eq 'status') { $before; return }
if ($before.request -ne 0) { throw 'An SWD command is already pending; CPU must be running.' }
$commands = @{ clear=1; white=2; black=3; pattern=4; invert=5; on=6; off=7; stop=8; checkerboard=9 }
$code = $commands[$Command]
$write = (& $cli -c port=SWD freq=1000 mode=HOTPLUG -w32 $address $code 2>&1) -join "`n"
# CubeProgrammer may report a verification error when firmware consumes and
# clears request before its readback. Never resend: check the completion counter.
$writeExit = $LASTEXITCODE
$watch = [Diagnostics.Stopwatch]::StartNew()
do {
    $state = Read-Mailbox
    if ($state.request -eq 0 -and $state.completed -ne $before.completed -and $state.last_command -eq $code) {
        if ($state.result -ne 0) { throw "Command $Command failed: HAL=$($state.result)" }
        Write-Output "OK SWD $Command HAL=0 completed=$($state.completed)"
        return
    }
    Start-Sleep -Milliseconds 100
} while ($watch.Elapsed.TotalSeconds -lt 10)
if ($writeExit -ne 0) { throw "No command completion; CubeProgrammer reported: $write" }
throw 'No mailbox completion within 10 seconds. Check CPU is running and ELF matches flashed image.'
