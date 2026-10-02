$ErrorActionPreference = 'Stop'
$workspace = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$packageName = 'TRENCH-0.1.0-Windows-x64'
$packageDir = Join-Path $PSScriptRoot "package/$packageName"
$bundle = Join-Path $workspace 'out/build/vst3/plugin/TRENCH_artefacts/Release/VST3/TRENCH.vst3'
$module = Join-Path $bundle 'Contents/x86_64-win/TRENCH.vst3'
if (!(Test-Path -LiteralPath $module)) { throw 'Built VST3 module is missing.' }

# Copy only the shipping bundle. Keep symbols, development builds, sources,
# local calibration data and audit logs outside the customer ZIP.
foreach ($file in Get-ChildItem -LiteralPath $bundle -Recurse -File) {
    $relative = [IO.Path]::GetRelativePath($bundle, $file.FullName)
    $target = Join-Path (Join-Path $packageDir 'TRENCH.vst3') $relative
    New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
    Copy-Item -LiteralPath $file.FullName -Destination $target -Force
}

$files = @(Get-ChildItem -LiteralPath $packageDir -Recurse -File |
    Where-Object Name -ne 'FILES.sha256' | Sort-Object FullName)
$checksums = foreach ($file in $files) {
    $relative = [IO.Path]::GetRelativePath($packageDir, $file.FullName).Replace('\', '/')
    '{0}  {1}' -f (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash, $relative
}
Set-Content -LiteralPath (Join-Path $packageDir 'FILES.sha256') -Value $checksums -Encoding utf8

$archive = Join-Path $PSScriptRoot "$packageName.zip"
Compress-Archive -LiteralPath $packageDir -DestinationPath $archive -Force
$extracted = Join-Path $PSScriptRoot ('extracted-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
Expand-Archive -LiteralPath $archive -DestinationPath $extracted
foreach ($file in Get-ChildItem -LiteralPath $packageDir -Recurse -File) {
    $relative = [IO.Path]::GetRelativePath($packageDir, $file.FullName)
    $copy = Join-Path (Join-Path $extracted $packageName) $relative
    if ((Get-FileHash -LiteralPath $file.FullName).Hash -ne (Get-FileHash -LiteralPath $copy).Hash) {
        throw "Archive round-trip mismatch: $relative"
    }
}

$identity = [ordered]@{
    product = 'TRENCH'
    version = '0.1.0'
    platform = 'Windows x64 VST3'
    createdUtc = (Get-Date).ToUniversalTime().ToString('o')
    gitHead = (& git -C $workspace rev-parse HEAD)
    sourceState = 'Working tree snapshot; existing user changes retained'
    binarySha256 = (Get-FileHash -LiteralPath $module).Hash
    archiveSha256 = (Get-FileHash -LiteralPath $archive).Hash
    archiveBytes = (Get-Item -LiteralPath $archive).Length
    archiveRoundTrip = 'Every packaged file matched after extraction'
    publication = 'Not published'
}
$identity | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'release-identity.json') -Encoding utf8
$sourceRoots = @('plugin/source', 'plugin/assets', 'plugin/presets/bodies', 'native/core/include', 'native/core/src')
$sourceFiles = @($sourceRoots | ForEach-Object {
    Get-ChildItem -LiteralPath (Join-Path $workspace $_) -Recurse -File
}) + @(Get-Item -LiteralPath (Join-Path $workspace 'CMakeLists.txt'),
    (Join-Path $workspace 'plugin/CMakeLists.txt'),
    (Join-Path $workspace 'plugin/presets/PresetRoster.inc'))
$sourceFiles | Sort-Object FullName | ForEach-Object {
    [ordered]@{ path = [IO.Path]::GetRelativePath($workspace, $_.FullName); sha256 = (Get-FileHash -LiteralPath $_.FullName).Hash }
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'source-hashes.json') -Encoding utf8
Write-Output $archive
Write-Output ('Extracted bundle: ' + (Join-Path (Join-Path $extracted $packageName) 'TRENCH.vst3'))
