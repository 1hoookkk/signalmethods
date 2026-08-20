[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'bootstrap-dependencies.ps1')

$repoRoot = Split-Path -Parent $PSScriptRoot
$runtimeRoot = Join-Path $repoRoot 'out\build\windows-msvc-release\vcpkg_installed\x64-windows-release'
$env:PATH = "$(Join-Path $runtimeRoot 'bin');$env:PATH"
$env:QT_PLUGIN_PATH = Join-Path $runtimeRoot 'Qt6\plugins'

Push-Location $repoRoot
try {
    $ctest = Join-Path (Split-Path -Parent $env:TRENCH_CMAKE) 'ctest.exe'
    & $ctest --preset windows-msvc-release
    if ($LASTEXITCODE -ne 0) {
        throw "Native tests failed with exit code $LASTEXITCODE."
    }
}
finally {
    Pop-Location
}
