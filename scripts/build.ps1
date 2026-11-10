$ErrorActionPreference = 'Stop'
$projectDir = Split-Path $PSScriptRoot -Parent
$stBundles = Join-Path $env:LOCALAPPDATA 'stm32cube\bundles'
$stNinja = Get-ChildItem (Join-Path $stBundles 'ninja') -Recurse -Filter ninja.exe -ErrorAction SilentlyContinue | Sort-Object FullName -Descending | Select-Object -First 1
$ninjaCmd = Get-Command ninja -ErrorAction SilentlyContinue
if ($stNinja) { $ninjaPath = $stNinja.FullName }
elseif ($ninjaCmd) { $ninjaPath = $ninjaCmd.Source } else {
    $ninjaPath = Join-Path $env:USERPROFILE '.platformio\packages\tool-ninja\ninja.exe'
}
if (!(Test-Path -LiteralPath $ninjaPath)) { throw 'Ninja nem talalhato.' }
$stGcc = Get-ChildItem (Join-Path $stBundles 'gnu-tools-for-stm32') -Recurse -Filter arm-none-eabi-gcc.exe -ErrorAction SilentlyContinue | Sort-Object FullName -Descending | Select-Object -First 1
$compilerArgs = @()
if ($stGcc) { $compilerArgs = @("-DARM_GCC_BIN=$($stGcc.DirectoryName.Replace('\','/'))") }
Push-Location $projectDir
try {
    cmake -S . -B build-st -G Ninja "-DCMAKE_MAKE_PROGRAM=$ninjaPath" @compilerArgs
    if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }
    cmake --build build-st
    if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
} finally { Pop-Location }
