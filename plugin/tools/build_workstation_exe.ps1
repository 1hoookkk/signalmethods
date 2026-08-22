# One-shot: build the TRENCH Workstation (Release) and drop a runnable exe on the Desktop.
# The Rust core is linked statically, so the exe is self-contained.
#   powershell -ExecutionPolicy Bypass -File tools\build_workstation_exe.ps1
$ErrorActionPreference = "Stop"
$repo = Split-Path $PSScriptRoot -Parent

cmake --build "$repo\build" --config Release --target TRENCH_Workstation
if ($LASTEXITCODE -ne 0) { throw "build failed" }

$src = "$repo\build\TRENCH_Workstation_artefacts\Release\TRENCH Workstation.exe"
if (-not (Test-Path $src)) { throw "exe not found: $src" }

$dst = Join-Path ([Environment]::GetFolderPath("Desktop")) "TRENCH Workstation.exe"
if (Test-Path $dst) {
    try { Remove-Item $dst -Force -ErrorAction Stop }
    catch {
        $aside = "$dst.inuse-old-$(Get-Date -Format yyyyMMdd-HHmmss)"
        Rename-Item $dst $aside
        Write-Host "old exe in use -> parked as $(Split-Path $aside -Leaf)"
    }
}
Copy-Item $src $dst
$mb = [math]::Round((Get-Item $dst).Length / 1MB, 1)
Write-Host "installed: $dst  ($mb MB, self-contained)"
