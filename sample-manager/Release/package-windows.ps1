param([string]$Version = '0.1.0', [switch]$RequireSignature)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($Version -notmatch '^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$') { throw 'Use major.minor.patch' }
$project = Split-Path $PSScriptRoot -Parent
$source = Join-Path $project 'build/windows-x64/CoreSampleManager_artefacts/Release/Core Sample Manager.exe'
if (-not (Test-Path $source)) { throw 'Run build-windows.ps1 first' }
$actualVersion = [Diagnostics.FileVersionInfo]::GetVersionInfo($source).ProductVersion
if ($actualVersion -notmatch ('^' + [regex]::Escape($Version) + '(?:\.0)?$')) { throw "Expected $Version, found $actualVersion" }
$signature = Get-AuthenticodeSignature $source
if ($RequireSignature -and ($signature.Status -ne 'Valid' -or -not $signature.TimeStamperCertificate)) {
    throw 'An explicitly requested signed package requires valid Authenticode and timestamp certificates'
}
$out = Join-Path $project ('dist/windows-x64-' + $Version + '-' + (Get-Date -Format 'yyyyMMddTHHmmss'))
$payload = Join-Path $out 'Core Sample Manager'
$notices = Join-Path $payload 'Notices'
New-Item -ItemType Directory -Force $notices | Out-Null
Copy-Item $source $payload
Copy-Item "$project/LICENSE" "$notices/Application-GPLv3.txt"
Copy-Item "$project/Vendor/PROVENANCE.md" $notices
Copy-Item "$project/Vendor/rubberband/COPYING" "$notices/Rubber-Band-GPLv2.txt"
Copy-Item "$project/.cache/deps/juce-src/LICENSE.md" "$notices/JUCE-LICENSE.md"
Get-ChildItem "$project/Resources/Fonts" -File | Where-Object { $_.Extension -in '.txt','.md' } | Copy-Item -Destination $notices
@"
Core Sample Manager $Version - Windows x64
Extract this ZIP and open Core Sample Manager.exe. Choose a local folder and import samples.
Edits save automatically. Keep .core-manager when moving or copying a portable project.
No installer, external audio tools, VC runtime installer, or plugins are needed.
Visualizer Device mode needs opt-in telemetry firmware. Online analysis runs only on request.
Keep the Notices directory. No assets are published by this script.
"@ | Set-Content "$payload/README.txt" -Encoding utf8
$archive = Join-Path $out "Core-Sample-Manager-$Version-windows-x64.zip"
Compress-Archive -Path $payload -DestinationPath $archive
$manifest = [ordered]@{ application='Core Sample Manager'; version=$Version; platform='windows-x64'; runtime='static MSVC'; signature=$signature.Status.ToString(); publication='disabled; local artifacts only'; juce='9.0.3'; archiveSHA256=(Get-FileHash $archive -Algorithm SHA256).Hash.ToLower() }
$manifestPath = Join-Path $out 'manifest.json'
$manifest | ConvertTo-Json -Depth 5 | Set-Content $manifestPath -Encoding utf8
@($archive,$manifestPath) | ForEach-Object { (Get-FileHash $_ -Algorithm SHA256).Hash.ToLower() + '  ' + (Split-Path $_ -Leaf) } | Set-Content "$out/SHA256SUMS.txt" -Encoding ascii
Write-Host $out
