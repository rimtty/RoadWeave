#requires -Version 5.1
[CmdletBinding()]
param(
    [ValidateSet('Run', 'Build', 'Test', 'Capture', 'AcceptBaselines')]
    [string] $Action = 'Run',
    [ValidateSet('group','radar','convoy','empty','max_group','max_convoy','stale',
        'radar_east','radar_far','rx','tx','busy','link_down')]
    [string] $Scenario = 'group',
    [ValidateRange(1,3)][int] $Zoom = 2,
    [switch] $Paused,
    [switch] $All
)
$ErrorActionPreference = 'Stop'
$env:VSLANG = '1033' # Ninja must recognize MSVC's include-dependency messages.
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$source = Join-Path $repo 'firmware\experiments\ui_lvgl\pc'
$build = Join-Path $source 'build'
$candidates = Join-Path $build 'candidates'
$scenarios = @('group','radar','convoy','empty','max_group','max_convoy','stale',
    'radar_east','radar_far','rx','tx','busy','link_down')
$selected = if ($All) { $scenarios } else { @($Scenario) }

# This is an explicit, separate operation. Test/Capture never update baselines.
if ($Action -eq 'AcceptBaselines') {
    foreach ($name in $selected) {
        $candidate = Join-Path $candidates "$name.png"
        if (-not (Test-Path -LiteralPath $candidate)) { throw "Capture and visually review $candidate first." }
    }
    New-Item -ItemType Directory -Force (Join-Path $source 'baselines') | Out-Null
    foreach ($name in $selected) {
        Copy-Item -LiteralPath (Join-Path $candidates "$name.png") -Destination (Join-Path $source "baselines\$name.png")
    }
    Write-Output 'Reviewed candidates copied to baselines. Review the Git diff and run -Action Test.'
    return
}

# Windows locks a running executable. A new build request replaces only this
# checkout's simulator, making edit -> Run possible without manual cleanup.
$simulatorPath = Join-Path $build 'roadweave_ui_sim.exe'
foreach ($runningSim in @(Get-Process -Name roadweave_ui_sim -ErrorAction SilentlyContinue)) {
    if ($runningSim.Path -eq $simulatorPath) {
        Write-Output 'Closing the previous simulator before rebuilding (simulated state resets).'
        Stop-Process -Id $runningSim.Id
        $runningSim.WaitForExit()
    }
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Install Visual Studio C++ Build Tools (Desktop development with C++).' }
$vs = (& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1)
if (-not $vs) { throw 'Visual Studio x64 C++ tools not found.' }
Import-Module (Join-Path $vs 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
$env:VSLANG = '1033'

# Prefer tools already on PATH, otherwise use the installed ESP-IDF host tools.
foreach ($tool in @('cmake','ninja')) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        $found = Get-ChildItem "D:\Espressif\tools\$tool" -Recurse -Filter "$tool.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
        if (-not $found) { throw "$tool not found. Add its bin directory to PATH." }
        $env:Path = "$($found.DirectoryName);$env:Path"
    }
}

function Ensure-Source([string]$Folder, [string]$Url, [string]$Tag, [string]$Commit) {
    $path = Join-Path $repo ".tools\$Folder"
    if (-not (Test-Path -LiteralPath $path)) {
        & git -c core.autocrlf=false clone --depth 1 --branch $Tag $Url $path
        if ($LASTEXITCODE -ne 0) { throw "Download failed: $Folder" }
    }
    $actual = (& git -C $path rev-parse HEAD)
    if ($LASTEXITCODE -ne 0 -or $actual -ne $Commit) { throw "Unexpected dependency revision in $path; expected $Commit" }
    $dirty = & git -C $path status --porcelain --untracked-files=no
    if ($dirty) { throw "Dependency has local modifications: $path" }
    return $path
}
$lvgl = Ensure-Source 'lvgl-9.5.0' 'https://github.com/lvgl/lvgl.git' 'v9.5.0' '85aa60d18b3d5e5588d7b247abf90198f07c8a63'
$sdl = Ensure-Source 'sdl-2.32.10' 'https://github.com/libsdl-org/SDL.git' 'release-2.32.10' '5d249570393f7a37e037abf22cd6012a4cc56a71'
& cmake -S $source -B $build -G Ninja -DCMAKE_BUILD_TYPE=Debug "-DFETCHCONTENT_SOURCE_DIR_LVGL=$lvgl" "-DFETCHCONTENT_SOURCE_DIR_SDL2=$sdl"
if ($LASTEXITCODE -ne 0) { throw 'Simulator configuration failed.' }
& cmake --build $build --parallel 8
if ($LASTEXITCODE -ne 0) { throw 'Simulator build failed.' }
$exe = Join-Path $build 'roadweave_ui_sim.exe'
switch ($Action) {
    'Run' {
        New-Item -ItemType Directory -Force (Join-Path $build 'captures') | Out-Null
        Push-Location (Join-Path $build 'captures')
        try {
            $simArgs = @('--scenario', $Scenario, '--zoom', "$Zoom")
            if ($Paused) { $simArgs += '--paused' }
            & $exe @simArgs
            if ($LASTEXITCODE -ne 0) { throw 'Simulator exited with an error.' }
        } finally { Pop-Location }
    }
    'Test' {
        & ctest --test-dir $build --output-on-failure --output-junit (Join-Path $build 'results\junit.xml')
        if ($LASTEXITCODE -ne 0) { throw "UI tests failed. See $build\results" }
    }
    'Capture' {
        New-Item -ItemType Directory -Force $candidates | Out-Null
        foreach ($name in $selected) {
            & $exe --scenario $name --capture (Join-Path $candidates "$name.png")
            if ($LASTEXITCODE -ne 0) { throw "Capture failed: $name" }
        }
        Write-Output "Review candidate images in $candidates. Baselines have not been changed."
    }
}
