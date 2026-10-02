param([switch]$CheckOnly)
$ErrorActionPreference = 'Stop'
$sourceRoot = Join-Path $PSScriptRoot 'out\build\vst3\plugin\TRENCH_artefacts\Release\VST3\TRENCH.vst3'
$targetRoot = Join-Path ([Environment]::GetFolderPath('CommonProgramFiles')) 'VST3\TRENCH.vst3'
$moduleRelative = 'Contents\x86_64-win\TRENCH.vst3'
if (!(Test-Path -LiteralPath (Join-Path $sourceRoot $moduleRelative))) { throw 'Build TRENCH_VST3 before installing.' }
$files = @(Get-ChildItem -LiteralPath $sourceRoot -File -Recurse)
foreach ($file in $files) {
    $relative = $file.FullName.Substring($sourceRoot.Length + 1)
    $target = Join-Path $targetRoot $relative
    if (Test-Path -LiteralPath $target) {
        try {
            $handle = [IO.File]::Open($target, [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
            $handle.Dispose()
        } catch {
            throw "Cannot replace $target. Close the DAW so TRENCH is unloaded, then rerun this installer. No installed files have been changed. $($_.Exception.Message)"
        }
    }
}
if ($CheckOnly) { Write-Output 'TRENCH build is ready and installed files are available for replacement.'; exit 0 }
$backupRoot = Join-Path $PSScriptRoot ('out\install-backups\' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
foreach ($file in $files) {
    $relative = $file.FullName.Substring($sourceRoot.Length + 1)
    $target = Join-Path $targetRoot $relative
    if (Test-Path -LiteralPath $target) {
        $backup = Join-Path $backupRoot $relative
        New-Item -ItemType Directory -Path (Split-Path -Parent $backup) -Force | Out-Null
        Copy-Item -LiteralPath $target -Destination $backup
    }
    New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
    Copy-Item -LiteralPath $file.FullName -Destination $target -Force
    if ((Get-FileHash -LiteralPath $file.FullName).Hash -ne (Get-FileHash -LiteralPath $target).Hash) { throw "Installed hash mismatch: $target" }
}
Write-Output "Installed and hash-verified: $targetRoot"
Write-Output ('User bodies: ' + (Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'TRENCH\User Bodies'))
Write-Output 'Reopen the DAW and choose Body from the TRENCH BODY menu.'
