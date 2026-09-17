$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$version = $env:RELEASE_INPUT_VERSION
if ($version -notmatch '^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$' -or
    @($version.Split('.') | Where-Object { [int64]$_ -gt 255 }).Count -ne 0) {
    throw 'Version must be major.minor.patch, each component 0..255.'
}
$appRoot = Split-Path $PSScriptRoot -Parent
# Unique output avoids stale executables and version resources on the persistent runner.
$root = Join-Path $appRoot ('release-output/Windows-x64/' + $version + '-' + [guid]::NewGuid().ToString('N'))
$build = Join-Path $root 'build'
$payload = Join-Path $root 'payload'
New-Item -ItemType Directory -Force $root, $payload, "$root/assets" | Out-Null
"RELEASE_ROOT=$root" | Out-File $env:GITHUB_ENV -Append -Encoding utf8
"RELEASE_VERSION=$version" | Out-File $env:GITHUB_ENV -Append -Encoding utf8
"version=$version" | Out-File $env:GITHUB_OUTPUT -Append -Encoding utf8
# The Azure action needs PowerShell 7; use Tape's pinned, verified runtime.
$cache = Join-Path $env:RUNNER_TOOL_CACHE 'visualizer-release'
New-Item -ItemType Directory -Force $cache | Out-Null
$ps = Join-Path $cache 'powershell-7.4.13'
if (-not (Test-Path "$ps/pwsh.exe")) {
    $zip = Join-Path $cache 'powershell-7.4.13.zip'
    if (-not (Test-Path $zip)) {
        Invoke-WebRequest -UseBasicParsing 'https://github.com/PowerShell/PowerShell/releases/download/v7.4.13/PowerShell-7.4.13-win-x64.zip' -OutFile $zip
    }
    if ((Get-FileHash $zip -Algorithm SHA256).Hash -ne '8fb52d2172d285b230c2857a90ba4dd28ecf6477ba4a91f91b6854a647b33b65') {
        throw 'PowerShell download checksum mismatch'
    }
    Expand-Archive $zip $ps -Force
}
$ps | Out-File $env:GITHUB_PATH -Append -Encoding utf8
"DOTNET_INSTALL_DIR=$(Join-Path $env:RUNNER_TOOL_CACHE 'visualizer-dotnet')" | Out-File $env:GITHUB_ENV -Append -Encoding utf8
# Same Visual Studio tools and runner as Tape; no installer tools are needed.
$vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if ($LASTEXITCODE -ne 0 -or -not $vs) { throw 'MSVC x64 tools not found' }
$cmakeRoot = Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake'
$cmake = Join-Path $cmakeRoot 'CMake/bin/cmake.exe'
$ninja = Join-Path $cmakeRoot 'Ninja/ninja.exe'
$vsdev = Join-Path $vs 'Common7/Tools/VsDevCmd.bat'
foreach ($tool in @($cmake, $ninja, $vsdev)) {
    if (-not (Test-Path $tool)) { throw "Missing build tool: $tool" }
}
$cmd = Join-Path $root 'build.cmd'
@(
    '@echo off',
    "call `"$vsdev`" -arch=x64 -host_arch=x64",
    'if errorlevel 1 exit /b %errorlevel%',
    'set CC=cl', 'set CXX=cl',
    "`"$cmake`" -S `"$appRoot`" -B `"$build`" -G Ninja -DCMAKE_MAKE_PROGRAM=`"$ninja`" -DCMAKE_BUILD_TYPE=Release -DVISUALIZER_VERSION=$version -DVISUALIZER_STANDALONE_ONLY=ON",
    'if errorlevel 1 exit /b %errorlevel%',
    "`"$cmake`" --build `"$build`" --config Release --parallel 4 --target zeptocore_visualizer_Standalone",
    'exit /b %errorlevel%'
) | Set-Content $cmd -Encoding ascii
& $cmd
if ($LASTEXITCODE -ne 0) { throw 'Standalone build failed' }
Copy-Item "$build/zeptocore_visualizer_artefacts/Release/Standalone/zeptocore visualizer.exe" $payload
$notices = Join-Path $payload 'Notices'
New-Item -ItemType Directory $notices | Out-Null
Copy-Item "$appRoot/LICENSE" "$notices/LICENSE.txt"
Copy-Item "$appRoot/Resources/Fonts/IBM-Plex-LICENSE.txt" $notices
Copy-Item "$appRoot/Resources/Fonts/Font-Awesome-LICENSE.txt" $notices
Copy-Item "$appRoot/.cache/deps/juce-src/LICENSE.md" "$notices/JUCE-LICENSE.md"
@"
Zeptocore Visualizer $version - Windows x64

Extract this ZIP and open zeptocore visualizer.exe.
Choose the reference folder containing bank1, bank2, etc., then connect via MIDI.
Visualizer-enabled firmware is required for slice tracking.
Keep the accompanying Notices directory. No plugin or installer is included.
"@ | Set-Content "$payload/README.txt" -Encoding utf8
