param(
    [string]$Port = 'COM10',
    [string]$Command = ''
)
$ErrorActionPreference = 'Stop'
$serial = New-Object System.IO.Ports.SerialPort $Port,115200,'None',8,'One'
$serial.DtrEnable = $true
$serial.ReadTimeout = 100
$serial.WriteTimeout = 1000
$serial.NewLine = "`n"
try {
    if ($Command) {
        $serial.Open()
        $serial.WriteLine($Command)
        $capture = ''
        $watch = [Diagnostics.Stopwatch]::StartNew()
        while ($watch.Elapsed.TotalSeconds -lt 5) {
            $chunk = $serial.ReadExisting()
            if ($chunk) { $capture += $chunk; Write-Host -NoNewline $chunk }
            if ($capture -match '(?m)^(?:OK |ERR |STATUS |COMMANDS:)[^\n]*\n') { return }
            Start-Sleep -Milliseconds 20
        }
        throw 'No command response within 5 seconds.'
    }
    Write-Host "LCD manual control on $Port. Enter a command, then press Enter."
    Write-Host 'clear | black | white | pattern | invert | checkerboard | off | on | auto | stop | status | help'
    Write-Host 'The image stays unchanged between commands. Type exit to close.'
    Write-Host -NoNewline '> '
    $line = ''
    while ($true) {
      try {
        if (!$serial.IsOpen) {
            try { $serial.Open() } catch { Start-Sleep -Milliseconds 500; continue }
            $line = ''
            Write-Host "`nConnected to $Port. Type help, then Enter."
            Write-Host -NoNewline '> '
        }
        $chunk = $serial.ReadExisting()
        if ($chunk) {
            Write-Host -NoNewline $chunk
            if ($chunk.EndsWith("`n")) { Write-Host -NoNewline "> $line" }
        }
        if ([Console]::KeyAvailable) {
            $key = [Console]::ReadKey($true)
            if ($key.Key -eq [ConsoleKey]::Enter) {
                Write-Host ''
                if ($line.Trim() -eq 'exit') { break }
                if ($line.Length) { $serial.WriteLine($line) }
                $line = ''
                Write-Host -NoNewline '> '
            } elseif ($key.Key -eq [ConsoleKey]::Backspace) {
                if ($line.Length) {
                    $line = $line.Substring(0,$line.Length-1)
                    [Console]::Write("`b `b")
                }
            } elseif ([int]$key.KeyChar -ge 32 -and [int]$key.KeyChar -le 126 -and $line.Length -lt 47) {
                $line += $key.KeyChar
                [Console]::Write($key.KeyChar)
            }
        }
        Start-Sleep -Milliseconds 20
      } catch {
        Write-Host "`nUSB connection lost. Waiting for $Port; commands are not replayed."
        try { $serial.Close() } catch { }
        $line = ''
        Start-Sleep -Milliseconds 500
      }
    }
} finally {
    if ($serial.IsOpen) { $serial.Close() }
    $serial.Dispose()
}
