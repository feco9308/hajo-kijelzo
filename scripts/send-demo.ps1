param(
    [ValidateRange(-180,180)][int]$Awa,
    [ValidateRange(0,99.9)][decimal]$Aws,
    [ValidateRange(0,99.9)][decimal]$Tws,
    [ValidateRange(0,99.9)][decimal]$Sog,
    [ValidateRange(0,359)][int]$Cog,
    [ValidateRange(-9.99,9.99)][decimal]$Xte,
    [ValidateRange(0,99.9)][decimal]$Distance,
    [ValidateRange(1,99)][int]$Waypoint
)
$ErrorActionPreference = 'Stop'
# Only supplied fields are changed. With no arguments, read current data.
$data = @{}
foreach ($key in $PSBoundParameters.Keys) { $data[$key] = $PSBoundParameters[$key] }
& (Join-Path $PSScriptRoot 'stlink-command.ps1') -Command data -DemoData $data
