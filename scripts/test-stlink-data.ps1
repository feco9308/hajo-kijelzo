$ErrorActionPreference = 'Stop'
$projectDir = Split-Path $PSScriptRoot -Parent
$control = Join-Path $PSScriptRoot 'stlink-command.ps1'
$demo = Join-Path $PSScriptRoot 'send-demo.ps1'
$cli = 'C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe'
$original = & $control -Command data
$restore = @{}
foreach ($field in @('Awa','Aws','Tws','Sog','Cog','Xte','Distance','Waypoint')) {
    $restore[$field] = $original.$field
}
try {
    & $demo -Awa -45 -Aws 19.5 -Tws 13.4 -Sog 6.8 -Cog 268 -Xte -0.05 -Distance 1.7 -Waypoint 2 | Out-Null
    & $demo -Aws 20.1 | Out-Null
    $current = & $control -Command data
    if ($current.Awa -ne -45 -or $current.Aws -ne 20.1 -or $current.Sog -ne 6.8 -or $current.Xte -ne -0.05) {
        throw 'Signed values or partial update preservation failed.'
    }
    $before = & $control -Command status
    try {
        & $control -Command data -DemoData @{ Aws=100 } | Out-Null
        throw 'Expected host range rejection.'
    } catch {
        if ($_.Exception.Message -notmatch 'Aws must be in') { throw }
    }
    $after = & $control -Command status
    if ($after.completed -ne $before.completed) { throw 'Invalid host data reached the MCU.' }
    Write-Output 'PASS signed values, partial updates and host validation'

    $nm = Get-ChildItem (Join-Path $env:LOCALAPPDATA 'stm32cube\bundles\gnu-tools-for-stm32') -Recurse -Filter arm-none-eabi-nm.exe |
        Sort-Object FullName -Descending | Select-Object -First 1
    $symbols = & $nm.FullName -n (Join-Path $projectDir 'build-st\sharp_lcd.elf')
    $symbol = $symbols | Select-String '^([0-9a-fA-F]+)\s+\w\s+lcd_debug$'
    $address = [Convert]::ToUInt32($symbol.Matches[0].Groups[1].Value,16)
    # Bypass host validation deliberately to exercise firmware rejection.
    & $cli -c port=SWD freq=1000 mode=HOTPLUG -w32 ('0x{0:X8}' -f ($address+16)) 0x000003E7 | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Cannot stage invalid firmware-test payload.' }
    & $cli -c port=SWD freq=1000 mode=HOTPLUG -w32 ('0x{0:X8}' -f $address) 0x0000000B -nv | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Cannot submit firmware-test request.' }
    $rejected = & $control -Command status
    $preserved = & $control -Command data
    if ($rejected.request -ne 0 -or $rejected.result -ne 4 -or $preserved.Awa -ne -45) {
        throw 'Firmware invalid-data rejection failed.'
    }
    Write-Output 'PASS firmware validation and applied-data restoration'

    $testDir = Join-Path $projectDir 'tmp\demo-data-tests'
    New-Item -ItemType Directory -Force -Path $testDir | Out-Null
    $common = @('-NoProfile','-ExecutionPolicy','Bypass','-File',('"{0}"' -f $demo))
    $first = Start-Process powershell.exe -WindowStyle Hidden -ArgumentList ($common + @('-Aws','21.4')) -PassThru `
        -RedirectStandardOutput (Join-Path $testDir 'first.out') -RedirectStandardError (Join-Path $testDir 'first.err')
    [void]$first.Handle
    $second = Start-Process powershell.exe -WindowStyle Hidden -ArgumentList ($common + @('-Sog','7.2')) -PassThru `
        -RedirectStandardOutput (Join-Path $testDir 'second.out') -RedirectStandardError (Join-Path $testDir 'second.err')
    [void]$second.Handle
    $first.WaitForExit()
    $second.WaitForExit()
    if ($first.ExitCode -ne 0 -or $second.ExitCode -ne 0) { throw 'Concurrent sender failed.' }
    $concurrent = & $control -Command data
    if ($concurrent.Aws -ne 21.4 -or $concurrent.Sog -ne 7.2 -or $concurrent.Awa -ne -45) {
        throw 'Concurrent senders lost a partial update.'
    }
    Write-Output 'PASS concurrent partial updates'

    & $demo -Awa 180 -Aws 99.9 -Tws 99.9 -Sog 99.9 -Cog 359 -Xte 9.99 -Distance 99.9 -Waypoint 99 | Out-Null
    & $demo -Awa -180 -Xte -9.99 | Out-Null
    & $demo -Awa 0 -Aws 0 -Tws 0 -Sog 0 -Cog 0 -Xte 0 -Distance 0 -Waypoint 1 | Out-Null
    Write-Output 'PASS supported minimum and maximum values'
} finally {
    & $control -Command data -DemoData $restore | Out-Null
}
Write-Output 'PASS all ST-LINK data checks; original demo values restored'
