<#
.SYNOPSIS
  Builds SliceM in Release and produces a self-contained Windows zip in dist/.

.DESCRIPTION
  The GUI links MSYS2/UCRT64 Qt, which is not present on a user's machine, so the Qt runtime has
  to travel with the binary. windeployqt copies the needed Qt DLLs and plugins; the MinGW runtime
  DLLs are copied explicitly because windeployqt does not always pick them up.

  Produces: dist/SliceM-v<version>-win64.zip
#>
param(
  [string]$BuildDir = "build-release",
  [string]$OutDir   = "dist",
  [string]$Msys     = "C:\msys64"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

# The version in CMakeLists.txt is authoritative; everything else derives from it.
$cmakeText = Get-Content (Join-Path $root "CMakeLists.txt") -Raw
if ($cmakeText -notmatch 'project\(\s*SliceM\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)') {
  throw "Could not read the version from CMakeLists.txt"
}
$version = $Matches[1]
Write-Host "SliceM version $version"

$ucrt = Join-Path $Msys "ucrt64\bin"
if (-not (Test-Path $ucrt)) { throw "MSYS2 UCRT64 toolchain not found at $ucrt" }

$posixRoot = "/" + ($root -replace '^([A-Za-z]):', { $_.Groups[1].Value.ToLower() } -replace '\\', '/')
$env:MSYSTEM = 'UCRT64'
& (Join-Path $Msys "usr\bin\bash.exe") -lc "cd '$posixRoot' && cmake -S . -B $BuildDir -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build $BuildDir"
if ($LASTEXITCODE -ne 0) { throw "build failed" }

& (Join-Path $Msys "usr\bin\bash.exe") -lc "cd '$posixRoot' && ctest --test-dir $BuildDir --output-on-failure"
if ($LASTEXITCODE -ne 0) { throw "tests failed - refusing to package" }

$stageName = "SliceM-v$version-win64"
$stage = Join-Path $OutDir $stageName
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Path $stage -Force | Out-Null

Copy-Item (Join-Path $BuildDir "gui\slicem-gui.exe") $stage
Copy-Item (Join-Path $BuildDir "cli\slicem.exe")     $stage
Copy-Item (Join-Path $root "README.md")              $stage

# Runs windeployqt and then walks the real import tables, because MSYS2's Qt pulls in ~25 further
# MinGW libraries that windeployqt does not know about. A hardcoded list was wrong here before and
# would have shipped a zip that cannot start on any machine without MSYS2.
& (Join-Path $PSScriptRoot "deploy_runtime.ps1") -Exe (Join-Path $stage "slicem-gui.exe") -QtBin $ucrt
if ($LASTEXITCODE -ne 0) { throw "runtime deployment failed" }

# The CLI is a console exe from the same toolchain and needs the compiler runtime too. Walking it
# separately costs nothing and does not assume its imports are a subset of the GUI's.
& (Join-Path $PSScriptRoot "deploy_runtime.ps1") -Exe (Join-Path $stage "slicem.exe") -QtBin $ucrt -SkipWindeployqt
if ($LASTEXITCODE -ne 0) { throw "runtime deployment failed for the CLI" }


$zip = Join-Path $OutDir "$stageName.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $zip

$size = [math]::Round((Get-Item $zip).Length / 1MB, 1)
Write-Host ""
Write-Host "Packaged $zip ($size MB)"
Write-Host "Smoke-test it on a machine without MSYS2 before publishing."
