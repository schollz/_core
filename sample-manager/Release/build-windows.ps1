param([string]$Version = '0.1.0', [int]$Jobs = 6)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($Version -notmatch '^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$') { throw 'Use major.minor.patch' }
$project = Split-Path $PSScriptRoot -Parent
$build = Join-Path $project 'build/windows-x64'
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
@(
    '@echo off', "call `"$dev`" -arch=x64 -host_arch=x64",
    'if errorlevel 1 exit /b %errorlevel%',
    "`"$cmake`" -S `"$project`" -B `"$build`" -G Ninja -DCMAKE_MAKE_PROGRAM=`"$ninja`" -DCMAKE_BUILD_TYPE=Release -DCORE_MANAGER_VERSION=$Version",
    'if errorlevel 1 exit /b %errorlevel%',
    "`"$cmake`" --build `"$build`" --parallel $Jobs",
    'if errorlevel 1 exit /b %errorlevel%',
    "`"$(Join-Path (Split-Path $cmake) 'ctest.exe')`" --test-dir `"$build`" --output-on-failure",
    'exit /b %errorlevel%'
) | Set-Content $cmdFile -Encoding ascii
& $cmdFile
if ($LASTEXITCODE -ne 0) { throw 'Native build or tests failed' }
# MSVC /MT is set by CMake before targets are created. No VC runtime installer.
Write-Host "Built Core Sample Manager $Version using the static MSVC runtime."
