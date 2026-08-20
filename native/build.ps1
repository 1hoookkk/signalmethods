[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'bootstrap-dependencies.ps1')

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vsRoot = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsRoot) {
    throw 'A Visual Studio installation with the x64 C++ tools was not found.'
}

$devShell = Join-Path $vsRoot 'Common7\Tools\VsDevCmd.bat'
$repoRoot = Split-Path -Parent $PSScriptRoot
$command = '"{0}" -arch=x64 -host_arch=x64 && "{1}" --build --preset windows-msvc-release --parallel 2' -f $devShell, $env:TRENCH_CMAKE

Push-Location $repoRoot
try {
    & $env:ComSpec /d /s /c $command
    if ($LASTEXITCODE -ne 0) {
        throw "Native build failed with exit code $LASTEXITCODE."
    }
}
finally {
    Pop-Location
}
