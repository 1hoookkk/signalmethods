[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Windows.Forms

$dialog = New-Object System.Windows.Forms.OpenFileDialog
$dialog.Title = "Choose 2 or 4 WAV files - M0 first, then M100"
$dialog.Filter = "WAV audio (*.wav)|*.wav"
$dialog.Multiselect = $true
$dialog.RestoreDirectory = $true
$dialog.CheckFileExists = $true

while ($true) {
    $result = $dialog.ShowDialog()
    if ($result -ne [System.Windows.Forms.DialogResult]::OK) {
        Write-Host "Cancelled."
        exit 0
    }

    $sources = @($dialog.FileNames)
    if ($sources.Count -eq 2 -or $sources.Count -eq 4) {
        break
    }

    [System.Windows.Forms.MessageBox]::Show(
        "Choose exactly 2 WAV files for a collapsed Q axis, or 4 WAV files for four authored corners.",
        "MAKE BODY",
        [System.Windows.Forms.MessageBoxButtons]::OK,
        [System.Windows.Forms.MessageBoxIcon]::Information
    ) | Out-Null
}

$cornerNames = if ($sources.Count -eq 2) {
    @("M0 Q0", "M100 Q0")
} else {
    @("M0 Q0", "M100 Q0", "M0 Q100", "M100 Q100")
}

Write-Host ""
Write-Host "Selected source order:"
for ($i = 0; $i -lt $sources.Count; $i++) {
    Write-Host ("  {0,-8} {1}" -f $cornerNames[$i], $sources[$i])
}
if ($sources.Count -eq 2) {
    Write-Host "  Q100 will begin as an exact copy of Q0."
}
Write-Host ""

$name = Read-Host "Name your body (e.g. MY_BASS)"
if ([string]::IsNullOrWhiteSpace($name)) {
    $name = "UNTITLED_BODY"
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$maker = Join-Path $PSScriptRoot "make_body.py"

Push-Location $repoRoot
try {
    & python $maker $name @sources
    $buildResult = $LASTEXITCODE
} finally {
    Pop-Location
}

if ($buildResult -ne 0) {
    Write-Host ""
    Write-Host "Something refused - read the message above."
    exit $buildResult
}

$bodyPath = Join-Path $repoRoot "bodies\candidates\$name.body240"
$evidencePath = Join-Path $repoRoot "evidence\body_${name}_e2e"
Write-Host ""
Write-Host "DONE. Body: $bodyPath"
Write-Host "Listen:    $evidencePath"
Start-Process explorer.exe -ArgumentList $evidencePath
exit 0
