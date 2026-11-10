param(
    [string]$Port = 'COM10',
    [int]$Seconds = 20
)
$ErrorActionPreference = 'Stop'
$serial = New-Object System.IO.Ports.SerialPort $Port,115200,'None',8,'One'
$serial.DtrEnable = $true
$serial.ReadTimeout = 500
try {
    $serial.Open()
    $watch = [Diagnostics.Stopwatch]::StartNew()
    while ($Seconds -eq 0 -or $watch.Elapsed.TotalSeconds -lt $Seconds) {
        $chunk = $serial.ReadExisting()
        if ($chunk) { Write-Host -NoNewline $chunk }
        Start-Sleep -Milliseconds 100
    }
} finally {
    if ($serial.IsOpen) { $serial.Close() }
    $serial.Dispose()
}
