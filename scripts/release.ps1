<#
.SYNOPSIS
  Tags the current commit and publishes a GitHub release with the packaged zip.

.DESCRIPTION
  Publishing is irreversible in practice: once a release is public, people may have downloaded it.
  So this script checks everything first and requires -Confirm to actually push anything.

  Requires the GitHub CLI (gh) authenticated against github.com/nghia-vo-1201/SliceM.

.EXAMPLE
  .\scripts\release.ps1                 # dry run: validates and shows what it would do
  .\scripts\release.ps1 -Confirm        # actually tags, pushes and publishes
#>
param(
  [switch]$Confirm,
  [string]$Notes = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$cmakeText = Get-Content (Join-Path $root "CMakeLists.txt") -Raw
if ($cmakeText -notmatch 'project\(\s*SliceM\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)') {
  throw "Could not read the version from CMakeLists.txt"
}
$version = $Matches[1]
$tag = "v$version"

$git = "C:\msys64\usr\bin\git.exe"
if (-not (Test-Path $git)) { $git = "git" }

# A release built from a dirty tree cannot be reproduced from the tag, which makes a bug report
# against it unanswerable.
$dirty = & $git status --porcelain
if ($dirty) {
  throw "Working tree is not clean. Commit or stash before releasing:`n$dirty"
}

$existing = & $git tag --list $tag
if ($existing) { throw "Tag $tag already exists. Bump the version in CMakeLists.txt first." }

if (-not (Get-Command gh -ErrorAction SilentlyContinue)) {
  throw "GitHub CLI 'gh' not found. Install it and run 'gh auth login' first."
}

$zip = Join-Path $root "dist\SliceM-v$version-win64.zip"
if (-not (Test-Path $zip)) {
  throw "Package not found: $zip`nRun .\scripts\package.ps1 first."
}

Write-Host "Version : $version"
Write-Host "Tag     : $tag"
Write-Host "Asset   : $zip"
Write-Host "Repo    : https://github.com/nghia-vo-1201/SliceM"

if (-not $Confirm) {
  Write-Host ""
  Write-Host "Dry run. Nothing was pushed or published."
  Write-Host "Re-run with -Confirm to tag, push and publish this release."
  return
}

if (-not $Notes) {
  $Notes = "SliceM $tag"
}

& $git tag -a $tag -m "SliceM $tag"
& $git push origin $tag
gh release create $tag $zip --repo "nghia-vo-1201/SliceM" --title "SliceM $tag" --notes $Notes
if ($LASTEXITCODE -ne 0) { throw "gh release create failed" }

Write-Host ""
Write-Host "Published $tag. The in-app update check will now offer it to existing installs."
