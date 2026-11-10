$ErrorActionPreference = 'Stop'
$projectDir = Split-Path $PSScriptRoot -Parent
Push-Location $projectDir
try {
    git submodule update --init --depth 1
    if ($LASTEXITCODE -ne 0) { throw 'Dependency checkout failed' }
    git -C Drivers/CMSIS_5 sparse-checkout set CMSIS/Core/Include
    if ($LASTEXITCODE -ne 0) { throw 'CMSIS sparse checkout failed' }
} finally { Pop-Location }
