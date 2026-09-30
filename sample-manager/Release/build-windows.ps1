param(
    [string]$Version = '0.1.0',
    [ValidateRange(1,64)][int]$Jobs = 6,
    [string]$SourceRoot = (Split-Path $PSScriptRoot -Parent),
    [string]$BuildDirectory = '',
    [switch]$SkipTests
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($Version -notmatch '^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$') { throw 'Use major.minor.patch' }
$project = [IO.Path]::GetFullPath($SourceRoot)
$build = if ($BuildDirectory) { [IO.Path]::GetFullPath($BuildDirectory) } else { Join-Path $project 'build/windows-x64' }
if (-not (Test-Path "$project/CMakeLists.txt")) { throw 'SourceRoot must contain the tagged sample-manager project' }
$vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if ($LASTEXITCODE -ne 0 -or -not $vs) { throw 'Install Visual Studio C++ x64 build tools' }
$tools = Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake'
$cmake = Join-Path $tools 'CMake/bin/cmake.exe'
$ninja = Join-Path $tools 'Ninja/ninja.exe'
$dev = Join-Path $vs 'Common7/Tools/VsDevCmd.bat'
foreach ($file in @($cmake,$ninja,$dev)) { if (-not (Test-Path $file)) { throw "Missing $file" } }
New-Item -ItemType Directory -Force $build | Out-Null
$cmdFile = Join-Path $build 'native-build.cmd'
$commands = @(
    '@echo off', "call `"$dev`" -arch=x64 -host_arch=x64",
    'if errorlevel 1 exit /b %errorlevel%',
    'set CC=cl', 'set CXX=cl',
    "`"$cmake`" -S `"$project`" -B `"$build`" -G Ninja -DCMAKE_MAKE_PROGRAM=`"$ninja`" -DCMAKE_BUILD_TYPE=Release -DCORE_MANAGER_VERSION=$Version",
    'if errorlevel 1 exit /b %errorlevel%',
    "`"$cmake`" --build `"$build`" --parallel $Jobs --target CoreSampleManager",
    'if errorlevel 1 exit /b %errorlevel%'
)
if (-not $SkipTests) {
    $commands += "`"$(Join-Path (Split-Path $cmake) 'ctest.exe')`" --test-dir `"$build`" --output-on-failure"
    $commands += 'if errorlevel 1 exit /b %errorlevel%'
}
$commands += 'exit /b 0'
$commands | Set-Content $cmdFile -Encoding ascii
& $cmdFile
if ($LASTEXITCODE -ne 0) { throw 'Native build or tests failed' }
# MSVC /MT is set by CMake before targets are created. No VC runtime installer.
Write-Host "Built Core Sample Manager $Version using the static MSVC runtime."
