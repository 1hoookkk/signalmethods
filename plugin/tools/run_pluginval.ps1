# Gate B: host-lifecycle validation of the installed TRENCH VST3.
# pluginval (Tracktion) at max strictness hammers the things a DAW will:
# state save/restore, editor open/close cycles, automation of every
# parameter, sample-rate/block-size churn, allocation and thread misuse.
# Red here = not commercially ready, regardless of how the face looks.
$ErrorActionPreference = 'Stop'
$bin = Join-Path $PSScriptRoot 'bin'
$exe = Join-Path $bin 'pluginval.exe'
if (-not (Test-Path $exe)) {
    New-Item -ItemType Directory -Force $bin | Out-Null
    $zip = Join-Path $bin 'pluginval.zip'
    Write-Host 'fetching pluginval (Tracktion, latest release)...'
    Invoke-WebRequest -Uri 'https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_Windows.zip' -OutFile $zip
    Expand-Archive -Path $zip -DestinationPath $bin -Force
    Remove-Item $zip
}
$plugin = 'C:\Program Files\Common Files\VST3\TRENCH.vst3'
if (-not (Test-Path $plugin)) { throw "not installed: $plugin (run tools\build_install_vst3.ps1 first)" }
# GUI-subsystem exe: $LASTEXITCODE never gets set, so wait on the process
$p = Start-Process -FilePath $exe -NoNewWindow -Wait -PassThru -ArgumentList @(
    '--strictness-level', '10', '--timeout-ms', '120000', '--validate', "`"$plugin`"")
if ($p.ExitCode -ne 0) { throw "pluginval FAILED (exit $($p.ExitCode))" }
Write-Host 'pluginval PASS at strictness 10'
