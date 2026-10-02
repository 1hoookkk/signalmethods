$ErrorActionPreference = 'Stop'
$workspace = 'C:\Users\hooki\trench-native'
$reportRoot = Join-Path $workspace 'output\motion-authoring-20260922'
$results = @()
foreach ($variant in @(@{Name='TRENCH';Target='TRENCH'}, @{Name='TRENCH Dev';Target='TRENCH_Dev'})) {
    $source = Join-Path $workspace ('out\build\vst3\plugin\' + $variant.Target + '_artefacts\Release\VST3\' + $variant.Name + '.vst3')
    $destination = Join-Path ([Environment]::GetFolderPath('CommonProgramFiles')) ('VST3\' + $variant.Name + '.vst3')
    $files = @(Get-ChildItem -LiteralPath $source -File -Recurse)
    $blocked = $null
    foreach ($file in $files) {
        $relative = $file.FullName.Substring($source.Length + 1)
        $target = Join-Path $destination $relative
        if (Test-Path -LiteralPath $target) {
            try {
                $handle = [IO.File]::Open($target, [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
                $handle.Dispose()
            } catch { $blocked = $_.Exception.Message; break }
        }
    }
    if ($blocked) {
        $results += ($variant.Name + ': unchanged; locked: ' + $blocked)
        continue
    }
    $backupRoot = Join-Path $reportRoot ('installed-before-' + $variant.Target + '-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
    foreach ($file in $files) {
        $relative = $file.FullName.Substring($source.Length + 1)
        $target = Join-Path $destination $relative
        if (Test-Path -LiteralPath $target) {
            $backup = Join-Path $backupRoot $relative
            New-Item -ItemType Directory -Force -Path (Split-Path -Parent $backup) | Out-Null
            Copy-Item -LiteralPath $target -Destination $backup
        }
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
        Copy-Item -LiteralPath $file.FullName -Destination $target -Force
        if ((Get-FileHash -LiteralPath $file.FullName).Hash -ne (Get-FileHash -LiteralPath $target).Hash) { throw "Hash mismatch: $target" }
    }
    $results += ($variant.Name + ': installed and SHA256 verified')
}
$results | Set-Content -LiteralPath (Join-Path $reportRoot 'install.txt')
$results
