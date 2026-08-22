[CmdletBinding()]
param([ValidateSet('dsp','wrapper','roster','ui','all')][string]$Layer = 'all')

$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$plugin = Join-Path $root 'plugin'
$env:PATH = "$env:USERPROFILE\.cargo\bin;C:\Windows\System32;C:\Windows;C:\Program Files\PowerShell\7;C:\Program Files\Git\cmd;C:\Program Files\CMake\bin;$env:USERPROFILE\AppData\Local\Programs\Python\Python313"
$fail = @()

function Step($name, [scriptblock]$body) {
    Write-Host "== $name"
    $out = & $body 2>&1
    $text = ($out | Out-String)
    if ($LASTEXITCODE -ne 0 -or $text -cmatch 'FAIL|error\[|panicked|tests failed') {
        $script:fail += $name
        Write-Host $text
    } else {
        Write-Host ($text.Trim().Split("`n") | Select-Object -Last 1)
    }
}

function Dsp {
    Step 'cargo test'   { Set-Location $plugin; cargo test --release -p trench-core --lib 2>&1 | Select-String 'test result' }
    Step 'cargo build'  { Set-Location $plugin; cargo build --release -p trench-core 2>&1 | Select-Object -Last 1 }
    Step 'x3 null gate' { Set-Location $plugin; py -3.13 tools\x3_null_gate.py 2>&1 | Select-Object -Last 1 }
}
function Wrapper {
    Step 'plugin build' { Set-Location $plugin; cmake --build build-juce9 --config Release --target TRENCH_LifecycleTests --target TRENCH_VST3 --target TRENCH_RenderNull --parallel 3 2>&1 | Select-String ' error |error C| -> ' }
    Step 'lifecycle'    { Set-Location $plugin; & build-juce9\TRENCH_LifecycleTests_artefacts\Release\TRENCH_LifecycleTests.exe 2>&1 | Select-String 'FAIL|PASS' }
}
function Roster {
    Step 'trench gate'  { Set-Location $plugin; py -3.13 trench.py gate 2>&1 | Select-Object -Last 1 }
    Step 'lifecycle'    { Set-Location $plugin; & build-juce9\TRENCH_LifecycleTests_artefacts\Release\TRENCH_LifecycleTests.exe 2>&1 | Select-String 'FAIL|PASS' }
}
function Ui {
    Step 'native build' { Set-Location $root; pwsh -NoProfile -File .\native\build.ps1 2>&1 | Select-String 'error|FAILED|Linking CXX executable' }
    Step 'native tests' { Set-Location $root; pwsh -NoProfile -File .\native\test.ps1 2>&1 | Select-String 'tests passed|tests failed' }
}

switch ($Layer) {
    'dsp'     { Dsp }
    'wrapper' { Wrapper }
    'roster'  { Roster }
    'ui'      { Ui }
    'all'     { Dsp; Wrapper; Roster; Ui }
}
Set-Location $root
if ($fail.Count) { Write-Host "FAILED: $($fail -join ', ')"; exit 1 }
Write-Host "OK ($Layer)"
