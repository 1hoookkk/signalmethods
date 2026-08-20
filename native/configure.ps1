[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw 'Visual Studio Installer vswhere.exe was not found.'
}

$vsRoot = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsRoot) {
    throw 'A Visual Studio installation with the x64 C++ tools was not found.'
}

$devShell = Join-Path $vsRoot 'Common7\Tools\VsDevCmd.bat'
$vcpkgRoot = Join-Path $vsRoot 'VC\vcpkg'
$ninja = (Get-Command ninja.exe -ErrorAction Stop).Source
$repoRoot = Split-Path -Parent $PSScriptRoot

$command = '"{0}" -arch=x64 -host_arch=x64 && set "VCPKG_ROOT={1}" && cmake --preset windows-msvc-debug --fresh "-DCMAKE_MAKE_PROGRAM={2}"' -f $devShell, $vcpkgRoot, $ninja
Push-Location $repoRoot
try {
    & $env:ComSpec /d /s /c $command
    if ($LASTEXITCODE -ne 0) {
        throw "Native configure failed with exit code $LASTEXITCODE."
    }
}
finally {
    Pop-Location
}
