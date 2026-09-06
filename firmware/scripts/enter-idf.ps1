#requires -Version 5.1
# Dot-source this script in each native PowerShell session.
[CmdletBinding()]
param(
    [string] $EspressifRoot = 'D:\Espressif',
    [string] $IdfPath,
    [string] $PythonEnvPath
)

$ErrorActionPreference = 'Stop'
$lockPath = Join-Path $PSScriptRoot '..\toolchain.lock'
$version = ((Get-Content $lockPath | Where-Object { $_ -match '^ESP_IDF_VERSION=' }) -split '=', 2)[1]
if (-not $IdfPath) { $IdfPath = Join-Path $EspressifRoot "esp-idf-$version" }
if (-not $PythonEnvPath) {
    $PythonEnvPath = Join-Path $EspressifRoot 'tools\python_env\idf5.4_py3.12_env'
}
$pythonExe = Join-Path $PythonEnvPath 'Scripts\python.exe'
if (-not (Test-Path $pythonExe)) { throw "ESP-IDF Python missing: $pythonExe. See docs/development-environment.md." }
if (-not (Test-Path (Join-Path $IdfPath 'export.ps1'))) { throw "ESP-IDF missing: $IdfPath" }
$env:IDF_TOOLS_PATH = Join-Path $EspressifRoot 'tools'
$env:IDF_PYTHON_ENV_PATH = $PythonEnvPath
$env:Path = "$(Split-Path $pythonExe);$env:Path"
. (Join-Path $IdfPath 'export.ps1')
if ($LASTEXITCODE -ne 0) { throw 'ESP-IDF environment activation failed.' }

# Avoid Windows .py file associations invoking another installed Python.
function global:idf.py {
    & (Join-Path $env:IDF_PYTHON_ENV_PATH 'Scripts\python.exe') (Join-Path $env:IDF_PATH 'tools\idf.py') @args
}
& (Join-Path $PSScriptRoot 'check-toolchain.ps1')
