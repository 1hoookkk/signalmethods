# The release ritual, one command: Gate A (build+install), Gate C (FaceShot
# contract proofs — the face shots land in the evidence folder), Gate B
# (pluginval at strictness 10). Writes a dated evidence folder with every
# log. Green here + a signed docs/UX_ACCEPTANCE.md pass = shippable.
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot
$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$evidence = Join-Path $repo "evidence\$stamp"
New-Item -ItemType Directory -Force $evidence | Out-Null

Write-Host '== GATE A: build + install =='
powershell -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'build_install_vst3.ps1') 2>&1 |
    Tee-Object (Join-Path $evidence 'build.log')
if ($LASTEXITCODE -ne 0) { throw 'GATE A build FAILED' }
cmake --build (Join-Path $repo 'build') --config Release --target TRENCH_FaceShot 2>&1 |
    Tee-Object (Join-Path $evidence 'faceshot_build.log') | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'GATE A FaceShot build FAILED' }

Write-Host '== GATE C: FaceShot contract proofs =='
Push-Location $evidence
& (Join-Path $repo 'build\TRENCH_FaceShot_artefacts\Release\TRENCH_FaceShot.exe') 2>&1 |
    Tee-Object (Join-Path $evidence 'faceshot.log')
$fs = $LASTEXITCODE
Pop-Location
if ($fs -ne 0) { throw "GATE C FaceShot FAILED (exit $fs) - read evidence\$stamp\faceshot.log" }

Write-Host '== GATE B: pluginval strictness 10 =='
powershell -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'run_pluginval.ps1') 2>&1 |
    Tee-Object (Join-Path $evidence 'pluginval.log')
if ($LASTEXITCODE -ne 0) { throw 'GATE B pluginval FAILED' }

"ALL GATES GREEN  $stamp" | Tee-Object (Join-Path $evidence 'VERDICT.txt')
Write-Host "evidence: evidence\$stamp  - now run the human pass (docs\UX_ACCEPTANCE.md) in FL"
