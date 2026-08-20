[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$vcpkgCommit = '45f9f39362a4c52e2b1fbe57b7e649db7f3d96d4'
$qtVersion = '6.11.2'
$qtArchiveSha512 = '8DC4DBB4B3AAC9E478361193ED8F8540DC70F1C22F25C91BDC5B6E6E574EA7F8C228800BC54E9A527BAAA3334D106B4ECD882D05DC61AB7DCC4CDBE7B4FDB715'
$cmakeVersion = '4.4.2'
$cmakeArchiveSha256 = 'E8139D85B3813BC38833142AE1940472E9A587E9B5D2718AC1804C60F4E57A64'

$repoRoot = Split-Path -Parent $PSScriptRoot
$vcpkgParent = Join-Path $repoRoot 'out\vp'
$overlayParent = Join-Path $repoRoot 'out\ov'
$toolParent = Join-Path $repoRoot 'out\tools'
$vcpkgRoot = Join-Path $vcpkgParent $vcpkgCommit.Substring(0, 8)
$overlayRoot = Join-Path $overlayParent "q6112-p313-r6"

New-Item -ItemType Directory -Force -Path $vcpkgParent, $overlayParent, $toolParent | Out-Null

if (-not (Test-Path -LiteralPath (Join-Path $vcpkgRoot '.git'))) {
    New-Item -ItemType Directory -Force -Path $vcpkgRoot | Out-Null
    git -C $vcpkgRoot init
    git -C $vcpkgRoot remote add origin https://github.com/microsoft/vcpkg.git
    git -C $vcpkgRoot fetch --depth 1 origin $vcpkgCommit
    git -C $vcpkgRoot checkout --detach FETCH_HEAD
    if ($LASTEXITCODE -ne 0) {
        throw "Could not fetch pinned vcpkg commit $vcpkgCommit."
    }
}

$resolvedVcpkgCommit = git -C $vcpkgRoot rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $resolvedVcpkgCommit.Trim() -ne $vcpkgCommit) {
    throw "Pinned vcpkg checkout mismatch: expected $vcpkgCommit, got $resolvedVcpkgCommit"
}

$vcpkgExe = Join-Path $vcpkgRoot 'vcpkg.exe'
if (-not (Test-Path -LiteralPath $vcpkgExe)) {
    & (Join-Path $vcpkgRoot 'bootstrap-vcpkg.bat') -disableMetrics
    if ($LASTEXITCODE -ne 0) {
        throw "Could not bootstrap pinned vcpkg (exit $LASTEXITCODE)."
    }
}

$cmakeArchive = Join-Path $toolParent "cmake-$cmakeVersion-windows-x86_64.zip"
$cmakeRoot = Join-Path $toolParent "cmake-$cmakeVersion-windows-x86_64"
$cmakeExe = Join-Path $cmakeRoot 'bin\cmake.exe'
if (-not (Test-Path -LiteralPath $cmakeExe)) {
    if (-not (Test-Path -LiteralPath $cmakeArchive)) {
        Invoke-WebRequest -Uri "https://github.com/Kitware/CMake/releases/download/v$cmakeVersion/cmake-$cmakeVersion-windows-x86_64.zip" -OutFile $cmakeArchive
    }
    $resolvedCmakeHash = (Get-FileHash -LiteralPath $cmakeArchive -Algorithm SHA256).Hash
    if ($resolvedCmakeHash -ne $cmakeArchiveSha256) {
        throw "CMake archive hash mismatch: expected $cmakeArchiveSha256, got $resolvedCmakeHash"
    }
    Expand-Archive -LiteralPath $cmakeArchive -DestinationPath $toolParent
}

$ninjaExe = (& $vcpkgExe fetch ninja | Select-Object -Last 1).Trim()
if (-not (Test-Path -LiteralPath $cmakeExe) -or -not (Test-Path -LiteralPath $ninjaExe)) {
    throw 'Could not resolve the pinned CMake and Ninja toolchain.'
}
if ((& $cmakeExe --version | Select-Object -First 1) -ne "cmake version $cmakeVersion") {
    throw "Expected CMake $cmakeVersion."
}
if ((& $ninjaExe --version) -ne '1.13.2') {
    throw 'Expected Ninja 1.13.2.'
}

if (-not (Test-Path -LiteralPath (Join-Path $overlayRoot 'qtbase\vcpkg.json'))) {
    New-Item -ItemType Directory -Force -Path $overlayRoot | Out-Null
    Copy-Item -LiteralPath (Join-Path $vcpkgRoot 'ports\qtbase') -Destination $overlayRoot -Recurse
    Copy-Item -LiteralPath (Join-Path $vcpkgRoot 'ports\pybind11') -Destination $overlayRoot -Recurse

    $qtDataPath = Join-Path $overlayRoot 'qtbase\port.data.cmake'
    $qtData = Get-Content -LiteralPath $qtDataPath -Raw
    $qtData = $qtData.Replace('6.11.1', $qtVersion)
    $qtData = [regex]::Replace($qtData, 'set\(qtbase_HASH "[0-9a-f]+"\)', "set(qtbase_HASH `"$qtArchiveSha512`")")
    [IO.File]::WriteAllText($qtDataPath, $qtData)

    $qtManifestPath = Join-Path $overlayRoot 'qtbase\vcpkg.json'
    $qtManifest = Get-Content -LiteralPath $qtManifestPath -Raw
    $qtManifest = $qtManifest.Replace('"version": "6.11.1",', '"version": "6.11.2",').Replace('"port-version": 1,', '"port-version": 0,')
    [IO.File]::WriteAllText($qtManifestPath, $qtManifest)

    $qtPortfilePath = Join-Path $overlayRoot 'qtbase\portfile.cmake'
    $qtPortfile = Get-Content -LiteralPath $qtPortfilePath -Raw
    $qtPortfile = [regex]::Replace(
        $qtPortfile,
        '(?m)^\s*QTBUG-145703\.patch[^\r\n]*\r?\n',
        ''
    )
    [IO.File]::WriteAllText($qtPortfilePath, $qtPortfile)

    $pybindManifestPath = Join-Path $overlayRoot 'pybind11\vcpkg.json'
    $pybindManifest = Get-Content -LiteralPath $pybindManifestPath -Raw | ConvertFrom-Json
    $pybindManifest.dependencies = @($pybindManifest.dependencies | Where-Object {
        -not ($_.name -eq 'python3')
    })
    [IO.File]::WriteAllText(
        $pybindManifestPath,
        ($pybindManifest | ConvertTo-Json -Depth 20) + [Environment]::NewLine
    )
}

$python313 = & py -3.13 -c 'import sys; print(sys.executable)'
if ($LASTEXITCODE -ne 0 -or -not $python313) {
    throw 'Python 3.13 is required for the research bridge.'
}

$env:VCPKG_ROOT = $vcpkgRoot
$env:TRENCH_VCPKG_OVERLAYS = $overlayRoot
$env:TRENCH_PYTHON313 = $python313.Trim()
$env:TRENCH_CMAKE = $cmakeExe
$env:TRENCH_NINJA = $ninjaExe

Write-Output "vcpkg $vcpkgCommit"
Write-Output "Qt $qtVersion overlay"
Write-Output "Python $(& $env:TRENCH_PYTHON313 --version 2>&1)"
Write-Output "CMake $(& $env:TRENCH_CMAKE --version | Select-Object -First 1)"
Write-Output "Ninja $(& $env:TRENCH_NINJA --version)"
