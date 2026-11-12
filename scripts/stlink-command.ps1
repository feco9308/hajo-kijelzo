param(
    [ValidateSet('status','data','clear','white','black','pattern','invert','checkerboard','boat','on','off','stop')]
    [string]$Command = 'status',
    [hashtable]$DemoData
)
$ErrorActionPreference = 'Stop'
$projectDir = Split-Path $PSScriptRoot -Parent
$cli = 'C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe'
$elf = Join-Path $projectDir 'build-st\sharp_lcd.elf'
$nm = Get-ChildItem (Join-Path $env:LOCALAPPDATA 'stm32cube\bundles\gnu-tools-for-stm32') -Recurse -Filter arm-none-eabi-nm.exe |
    Sort-Object FullName -Descending | Select-Object -First 1
if (!$nm -or !(Test-Path -LiteralPath $elf)) { throw 'Build the matching firmware first.' }
if ($DemoData -and $Command -ne 'data') { throw 'DemoData requires -Command data.' }
$symbols = & $nm.FullName -n $elf
if ($LASTEXITCODE -ne 0) { throw 'Cannot read ELF symbols.' }
$symbol = $symbols | Select-String '^([0-9a-fA-F]+)\s+\w\s+lcd_debug$'
if (!$symbol) { throw 'ELF has no lcd_debug mailbox.' }
$mailboxAddress = [Convert]::ToUInt32($symbol.Matches[0].Groups[1].Value,16)
$dataAddress = $mailboxAddress + 16

function Read-Words([uint32]$Address,[int]$Count) {
    $hexAddress = '0x{0:X8}' -f $Address
    $output = (& $cli -c port=SWD freq=1000 mode=HOTPLUG -r32 $hexAddress ($Count*4) 2>&1) -join "`n"
    if ($LASTEXITCODE -ne 0) { throw "SWD read failed: $output" }
    $words = @()
    $rows = [regex]::Matches($output,'(?im)^0x([0-9a-f]{8})\s*:\s*((?:[0-9a-f]{8}[ \t]*){1,4})')
    foreach ($row in $rows) {
        $rowAddress = [Convert]::ToUInt32($row.Groups[1].Value,16)
        if ($rowAddress -ne $Address + $words.Count*4) { throw 'Unexpected SWD read address.' }
        foreach ($word in [regex]::Matches($row.Groups[2].Value,'[0-9a-fA-F]{8}')) {
            $words += [Convert]::ToUInt32($word.Value,16)
        }
    }
    if ($words.Count -ne $Count) { throw "Cannot parse SWD words: $output" }
    return $words
}
function Read-Mailbox {
    $words = @(Read-Words $mailboxAddress 4)
    return [pscustomobject]@{ request=$words[0]; result=$words[1]; completed=$words[2]; last_command=$words[3] }
}
function Signed([uint32]$Value) {
    return [BitConverter]::ToInt32([BitConverter]::GetBytes($Value),0)
}
function Unsigned([int32]$Value) {
    return [BitConverter]::ToUInt32([BitConverter]::GetBytes($Value),0)
}
function Read-AppliedData {
    $words = @(Read-Words $dataAddress 8)
    return [ordered]@{
        Awa=(Signed $words[0]); Aws=([decimal]$words[1]/10); Tws=([decimal]$words[2]/10)
        Sog=([decimal]$words[3]/10); Cog=$words[4]; Xte=([decimal](Signed $words[5])/100)
        Distance=([decimal]$words[6]/10); Waypoint=$words[7]
    }
}
function Scaled([System.Collections.IDictionary]$Values,[string]$Name,[int]$Scale,[decimal]$Min,[decimal]$Max) {
    if ($null -eq $Values[$Name]) { throw "Missing value: $Name" }
    $value = [decimal]$Values[$Name]
    if ($value -lt $Min -or $value -gt $Max) { throw "$Name must be in $Min..$Max." }
    if ($Scale -eq 1 -and $value -ne [decimal]::Truncate($value)) { throw "$Name must be an integer." }
    return [int32][decimal]::Round($value*$Scale,0,[MidpointRounding]::AwayFromZero)
}
function Write-Words([uint32]$Address,[uint32[]]$Words) {
    $cliArguments = @('-c','port=SWD','freq=1000','mode=HOTPLUG','-w32',('0x{0:X8}' -f $Address))
    foreach ($word in $Words) { $cliArguments += ('0x{0:X8}' -f $word) }
    $output = (& $cli @cliArguments 2>&1) -join "`n"
    return [pscustomobject]@{ ExitCode=$LASTEXITCODE; Output=$output }
}

# Serialize all normal SWD entry points, including payload staging and request.
$mutex = New-Object System.Threading.Mutex($false,'Local\HajoKijelzoStlink')
$lockTaken = $false
try {
    try { $lockTaken = $mutex.WaitOne(15000) }
    catch [System.Threading.AbandonedMutexException] { $lockTaken = $true }
    if (!$lockTaken) { throw 'Another ST-LINK command is still running.' }
    $before = Read-Mailbox
    if ($Command -eq 'status') { $before; return }
    if ($before.request -ne 0) { throw 'An SWD command is pending; CPU must be running.' }
    $codes = @{ clear=1; white=2; black=3; pattern=4; invert=5; on=6; off=7; stop=8; checkerboard=9; boat=10; data=11 }
    if ($Command -eq 'data') {
        $values = Read-AppliedData
        if (!$DemoData -or $DemoData.Count -eq 0) { [pscustomobject]$values; return }
        foreach ($key in $DemoData.Keys) {
            if ($key -notin $values.Keys) { throw "Unknown demo field: $key" }
            $canonical = @($values.Keys | Where-Object { $_ -eq $key })[0]
            $values[$canonical] = $DemoData[$key]
        }
        [uint32[]]$payload = @(
            (Unsigned (Scaled $values 'Awa' 1 -180 180)),
            (Scaled $values 'Aws' 10 0 99.9),
            (Scaled $values 'Tws' 10 0 99.9),
            (Scaled $values 'Sog' 10 0 99.9),
            (Scaled $values 'Cog' 1 0 359),
            (Unsigned (Scaled $values 'Xte' 100 -9.99 9.99)),
            (Scaled $values 'Distance' 10 0 99.9),
            (Scaled $values 'Waypoint' 1 1 99)
        )
        $staged = Write-Words $dataAddress $payload
        if ($staged.ExitCode -ne 0) { throw "Cannot stage demo data: $($staged.Output)" }
        $readback = @(Read-Words $dataAddress 8)
        for ($i=0; $i -lt 8; $i++) {
            if ($readback[$i] -ne $payload[$i]) { throw 'Demo payload verification failed.' }
        }
    }
    $code = $codes[$Command]
    $write = Write-Words $mailboxAddress @($code)
    # Fast requests may be consumed before CubeProgrammer's verification.
    # The completion counter is authoritative; never resend a request.
    $watch = [Diagnostics.Stopwatch]::StartNew()
    do {
        $state = Read-Mailbox
        if ($state.request -eq 0 -and $state.completed -ne $before.completed -and $state.last_command -eq $code) {
            if ($state.result -eq 4) { throw 'Firmware rejected invalid demo data; current data preserved.' }
            if ($state.result -ne 0) { throw "Command $Command failed: HAL=$($state.result)" }
            Write-Output "OK SWD $Command HAL=0 completed=$($state.completed)"
            if ($Command -eq 'data') {
                $applied = @(Read-Words $dataAddress 8)
                for ($i=0; $i -lt 8; $i++) {
                    if ($applied[$i] -ne $payload[$i]) { throw 'Applied data differs from submitted data.' }
                }
                [pscustomobject](Read-AppliedData)
            }
            return
        }
        Start-Sleep -Milliseconds 100
    } while ($watch.Elapsed.TotalSeconds -lt 10)
    if ($write.ExitCode -ne 0) { throw "No command completion: $($write.Output)" }
    throw 'No completion within 10 seconds. Check running CPU and matching flashed ELF.'
} finally {
    if ($lockTaken) { $mutex.ReleaseMutex() }
    $mutex.Dispose()
}
