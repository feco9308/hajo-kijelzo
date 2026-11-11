param([string]$Port = 'COM10')
# Explicit hardware test: sends LCD commands and leaves the display cleared.
$ErrorActionPreference = 'Stop'
$serial = New-Object System.IO.Ports.SerialPort $Port,115200,'None',8,'One'
$serial.DtrEnable = $true
$serial.NewLine = "`n"
$serial.WriteTimeout = 1000
function Await-Reply([string]$Pattern) {
    $capture = ''
    $watch = [Diagnostics.Stopwatch]::StartNew()
    while ($watch.Elapsed.TotalSeconds -lt 5) {
        $capture += $serial.ReadExisting()
        if ($capture -match $Pattern) { Write-Host $capture.Trim(); return $capture }
        Start-Sleep -Milliseconds 20
    }
    throw "Missing reply $Pattern; received: $capture"
}
function Send-Test([string]$Text,[string]$Pattern) {
    $serial.WriteLine($Text)
    return Await-Reply $Pattern
}
try {
    $serial.Open()
    [void](Send-Test 'help' '(?m)^COMMANDS:.*\n')
    [void](Send-Test 'clear' '(?m)^OK clear HAL=0\r?\n')
    # Command split across USB packets, then CRLF: execute exactly once.
    $serial.Write('bla')
    Start-Sleep -Milliseconds 80
    $serial.Write("ck`r`n")
    [void](Await-Reply '(?m)^OK black HAL=0\r?\n')
    $state = Send-Test 'status' '(?m)^STATUS .*stage=13 .*disp=1 .*spi_hz=250000 .*\n'
    [void](Send-Test ' WHITE ' '(?m)^OK white HAL=0\r?\n')
    [void](Send-Test 'status' '(?m)^STATUS .*stage=12 .*\n')
    [void](Send-Test 'off' '(?m)^OK off DISP=0 .*\n')
    $a = Send-Test 'status' '(?m)^STATUS .*disp=0 .*\n'
    Start-Sleep -Milliseconds 120
    $b = Send-Test 'status' '(?m)^STATUS .*disp=0 .*\n'
    $countA = [regex]::Match($a,'tim3=(\d+)').Groups[1].Value
    $countB = [regex]::Match($b,'tim3=(\d+)').Groups[1].Value
    if ($countA -eq $countB) { throw 'External COM timer did not advance while DISP was off' }
    [void](Send-Test 'on' '(?m)^OK on DISP=1 .*\n')
    [void](Send-Test 'pattern' '(?m)^OK pattern HAL=0\r?\n')
    [void](Send-Test 'invert' '(?m)^OK invert HAL=0\r?\n')
    [void](Send-Test 'nonsense' '(?m)^ERR unknown-command.*\n')
    [void](Send-Test ('x' * 80) '(?m)^ERR line-too-long.*\n')
    [void](Send-Test 'status' '(?m)^STATUS .*stage=16 .*rx_dropped=0\r?\n')
    [void](Send-Test 'auto' '(?m)^OK auto .*\n')
    Start-Sleep -Milliseconds 1400
    [void](Send-Test 'status' '(?m)^STATUS .*stage=21 .*line=[1-9]\d* auto=1 .*\n')
    [void](Send-Test 'stop' '(?m)^OK stop .*\n')
    $stoppedA = Send-Test 'status' '(?m)^STATUS .*auto=0 .*\n'
    Start-Sleep -Milliseconds 200
    $stoppedB = Send-Test 'status' '(?m)^STATUS .*auto=0 .*\n'
    if ([regex]::Match($stoppedA,'line=(\d+)').Groups[1].Value -ne [regex]::Match($stoppedB,'line=(\d+)').Groups[1].Value) { throw 'Line advanced after stop' }
    [void](Send-Test 'clear' '(?m)^OK clear HAL=0\r?\n')
    Start-Sleep -Seconds 3
    [void](Send-Test 'status' '(?m)^STATUS .*stage=11 .*disp=1 .*hal=0 .*spi_err=0 .*\n')
    Write-Host 'PASS: manual commands, split RX, invalid/overlong input, external COM timer, auto/stop and image hold.'
} finally {
    if ($serial.IsOpen) { $serial.Close() }
    $serial.Dispose()
}
