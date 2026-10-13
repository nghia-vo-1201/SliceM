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

$deploy = Get-ChildItem $ucrt -Filter "windeployqt*.exe" | Select-Object -First 1
if (-not $deploy) { throw "windeployqt not found in $ucrt" }

$env:PATH = "$ucrt;" + $env:PATH
& $deploy.FullName --release --no-translations --no-system-d3d-compiler --no-opengl-sw (Join-Path $stage "slicem-gui.exe")
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed" }

# windeployqt handles Qt, not the compiler runtime. Without these the exe fails to start on a
# machine that has never seen MSYS2.
foreach ($dll in @("libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll")) {
  $src = Join-Path $ucrt $dll
  if (Test-Path $src) { Copy-Item $src $stage }
}

$zip = Join-Path $OutDir "$stageName.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $zip

$size = [math]::Round((Get-Item $zip).Length / 1MB, 1)
Write-Host ""
Write-Host "Packaged $zip ($size MB)"
Write-Host "Smoke-test it on a machine without MSYS2 before publishing."
