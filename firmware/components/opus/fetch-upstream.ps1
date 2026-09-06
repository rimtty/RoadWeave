#requires -Version 5.1
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$tag = 'v1.5.2'
$destination = Join-Path $PSScriptRoot 'upstream'
if (-not (Test-Path $destination)) {
    & git -c core.autocrlf=false clone --depth 1 --branch $tag https://github.com/xiph/opus.git $destination
    if ($LASTEXITCODE -ne 0) { throw 'Opus clone failed.' }
}
$actual = & git -C $destination describe --tags --exact-match HEAD
if ($LASTEXITCODE -ne 0 -or $actual -ne $tag) { throw "Expected Opus $tag, got $actual at $destination" }
$changes = & git -C $destination status --porcelain
if ($LASTEXITCODE -ne 0 -or $changes) { throw "Opus upstream has local changes: $destination" }
Write-Output "PASS: Opus $tag at $destination"
