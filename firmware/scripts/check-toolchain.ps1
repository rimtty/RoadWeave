#requires -Version 5.1

[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Fail([string] $Message) {
    [Console]::Error.WriteLine("ERROR: $Message")
    exit 1
}

$scriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Definition
$firmwareDirectory = Split-Path -Parent $scriptDirectory
$lockPath = Join-Path -Path $firmwareDirectory -ChildPath "toolchain.lock"

if (-not (Test-Path -LiteralPath $lockPath -PathType Leaf)) {
    Fail "toolchain.lock was not found at $lockPath"
}

$lockValues = @{}
foreach ($line in Get-Content -LiteralPath $lockPath) {
    $trimmedLine = $line.Trim()
    if ([string]::IsNullOrWhiteSpace($trimmedLine) -or $trimmedLine.StartsWith("#")) {
        continue
    }

    $assignment = $trimmedLine -split "=", 2
    if ($assignment.Count -ne 2) {
        Fail "invalid assignment in toolchain.lock: $trimmedLine"
    }

    $key = $assignment[0].Trim()
    $value = $assignment[1].Trim()
    if ([string]::IsNullOrWhiteSpace($key) -or [string]::IsNullOrWhiteSpace($value)) {
        Fail "invalid assignment in toolchain.lock: $trimmedLine"
    }

    $lockValues[$key] = $value
}

$expectedVersion = $lockValues["ESP_IDF_VERSION"]
if ([string]::IsNullOrWhiteSpace($expectedVersion)) {
    Fail "ESP_IDF_VERSION is missing from $lockPath"
}

if ($null -eq (Get-Command idf.py -ErrorAction SilentlyContinue)) {
    Fail "idf.py is not available. Install and export ESP-IDF $expectedVersion."
}

if ($null -eq (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Fail "cmake is not available. Use the ESP-IDF Tools Installer environment."
}

if ($null -eq (Get-Command ninja -ErrorAction SilentlyContinue)) {
    Fail "ninja is not available. Use the ESP-IDF Tools Installer environment."
}

$actualVersion = (& idf.py --version 2>&1 | Out-String).Trim()
if ($LASTEXITCODE -ne 0) {
    Fail "idf.py --version failed with exit code $LASTEXITCODE"
}

$expectedVersionNumber = $expectedVersion
if ($expectedVersionNumber.StartsWith("v")) {
    $expectedVersionNumber = $expectedVersionNumber.Substring(1)
}
if ([string]::IsNullOrWhiteSpace($expectedVersionNumber) -or
    $actualVersion -notlike "*$expectedVersionNumber*") {
    Fail "expected ESP-IDF $expectedVersion, got $actualVersion"
}

Write-Output "PASS: $actualVersion"
Write-Output "Pinned HaLow component: $($lockValues['MORSE_HALOW_COMPONENT'])=$($lockValues['MORSE_HALOW_VERSION'])"
Write-Output "Pinned board profile: $($lockValues['MORSE_BOARD_PROFILE'])"
$cmakeVersion = (& cmake --version 2>&1 | Select-Object -First 1)
$ninjaVersion = (& ninja --version 2>&1 | Select-Object -First 1)
Write-Output "Host CMake: $cmakeVersion (macOS verified: $($lockValues['HOST_CMAKE_VERIFIED_MACOS']))"
Write-Output "Host Ninja: $ninjaVersion (macOS verified: $($lockValues['HOST_NINJA_VERIFIED_MACOS']))"
