[CmdletBinding()]
param(
    [string]$Body,
    [double]$SampleRate = 44100.0
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$executable = Join-Path $repoRoot 'out\build\windows-msvc-release\native\app\trench_native.exe'
$runtimeRoot = Join-Path $repoRoot 'out\build\windows-msvc-release\vcpkg_installed\x64-windows-release'
if (-not (Test-Path -LiteralPath $executable)) {
    throw "Native executable not found: $executable"
}

$env:PATH = "$(Join-Path $runtimeRoot 'bin');$env:PATH"
$env:QT_PLUGIN_PATH = Join-Path $runtimeRoot 'Qt6\plugins'

$arguments = @('--sample-rate', $SampleRate.ToString([Globalization.CultureInfo]::InvariantCulture))
if ($Body) {
    $arguments += @('--body', (Resolve-Path -LiteralPath $Body).Path)
}

Start-Process -FilePath $executable -ArgumentList $arguments -WorkingDirectory $repoRoot -Wait
