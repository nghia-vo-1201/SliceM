<#
.SYNOPSIS
  Makes a built SliceM binary runnable outside the MSYS2 environment.

.DESCRIPTION
  windeployqt copies Qt's own DLLs and its plugins, but MSYS2's Qt is linked against a long tail of
  other MinGW libraries - ICU, PCRE2, zstd, double-conversion, harfbuzz, freetype, brotli and more -
  that windeployqt knows nothing about. Missing any single one of them makes the executable fail
  with 0xC0000135 (STATUS_DLL_NOT_FOUND) before main() runs: no window, no message, no log.

  Hardcoding a list of them does not work either, because the list changes with every toolchain
  update. So this walks the real import table of the executable and of every DLL that ends up
  beside it, recursively, and copies anything that resolves inside the toolchain's bin directory.
  DLLs that do not resolve there are Windows' own and are deliberately left alone.

  Plugins land in subdirectories, but their dependencies are resolved against the executable's
  directory, so everything is copied flat next to the exe.
#>
param(
  [Parameter(Mandatory = $true)][string]$Exe,
  [string]$QtBin = "C:\msys64\ucrt64\bin",
  [switch]$SkipWindeployqt
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $Exe))   { throw "executable not found: $Exe" }
if (-not (Test-Path $QtBin)) { throw "toolchain bin directory not found: $QtBin" }

$Exe = (Resolve-Path $Exe).Path
$target = Split-Path -Parent $Exe

$objdump = Join-Path $QtBin "objdump.exe"
if (-not (Test-Path $objdump)) { throw "objdump not found at $objdump (install binutils)" }

if (-not $SkipWindeployqt) {
  $deploy = Get-ChildItem $QtBin -Filter "windeployqt*.exe" -ErrorAction SilentlyContinue |
            Select-Object -First 1
  if ($deploy) {
    $env:PATH = "$QtBin;" + $env:PATH
    & $deploy.FullName --no-translations --no-system-d3d-compiler $Exe | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "windeployqt failed with exit code $LASTEXITCODE" }
  } else {
    Write-Warning "windeployqt not found in $QtBin; relying on the import walk alone"
  }
}

# Breadth-first over the import tables. Seed with the exe and everything windeployqt just placed,
# plugins included, since a plugin can need a library nothing else does.
$queue = New-Object System.Collections.Queue
$queue.Enqueue($Exe)
foreach ($dll in Get-ChildItem $target -Filter *.dll -Recurse -File) { $queue.Enqueue($dll.FullName) }

$seen = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
$copied = @()

while ($queue.Count -gt 0) {
  $current = $queue.Dequeue()
  if (-not $seen.Add($current)) { continue }

  $imports = & $objdump -p $current 2>$null |
             Select-String 'DLL Name:' |
             ForEach-Object { ($_ -replace '.*DLL Name:\s*', '').Trim() }

  foreach ($name in $imports) {
    $beside = Join-Path $target $name
    if (Test-Path $beside) { continue }

    $source = Join-Path $QtBin $name
    if (-not (Test-Path $source)) { continue }   # a Windows DLL: not ours to ship

    Copy-Item $source $beside -Force
    $copied += $name
    $queue.Enqueue($beside)
  }
}

if ($copied.Count -gt 0) {
  Write-Host ("Deployed {0} MinGW dependencies: {1}" -f $copied.Count, ($copied -join ', '))
}

# A dependency that resolves nowhere is a genuine packaging failure, so say so rather than leaving
# it to be discovered as a silent 0xC0000135 on someone else's machine.
$missing = @()
foreach ($file in @($Exe) + (Get-ChildItem $target -Filter *.dll -Recurse -File |
                             ForEach-Object { $_.FullName })) {
  $imports = & $objdump -p $file 2>$null |
             Select-String 'DLL Name:' |
             ForEach-Object { ($_ -replace '.*DLL Name:\s*', '').Trim() }
  foreach ($name in $imports) {
    if (Test-Path (Join-Path $target $name)) { continue }
    if (Test-Path (Join-Path $QtBin $name))  { $missing += $name }
  }
}
if ($missing.Count -gt 0) {
  throw ("unresolved after deployment: " + (($missing | Sort-Object -Unique) -join ', '))
}
