param(
    [string]$Version = '0.1.0',
    [switch]$RequireSignature,
    [string]$SourceRoot = (Split-Path $PSScriptRoot -Parent),
    [string]$BuildDirectory = '',
    [string]$OutputDirectory = ''
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($Version -notmatch '^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$') { throw 'Use major.minor.patch' }
$project = [IO.Path]::GetFullPath($SourceRoot)
$build = if ($BuildDirectory) { [IO.Path]::GetFullPath($BuildDirectory) } else { Join-Path $project 'build/windows-x64' }
$productMatch = [regex]::Match((Get-Content (Join-Path $project 'CMakeLists.txt') -Raw), '\bPRODUCT_NAME\s+"([A-Za-z0-9_ -]+)"')
if (-not $productMatch.Success) { throw 'Cannot read the application PRODUCT_NAME from CMakeLists.txt' }
$productName = $productMatch.Groups[1].Value
$source = Join-Path $build "CoreSampleManager_artefacts/Release/$productName.exe"
if (-not (Test-Path $source)) { throw 'Run build-windows.ps1 first' }
$actualVersion = [Diagnostics.FileVersionInfo]::GetVersionInfo($source).ProductVersion
if ($actualVersion -notmatch ('^' + [regex]::Escape($Version) + '(?:\.0)?$')) { throw "Expected $Version, found $actualVersion" }
$signature = Get-AuthenticodeSignature $source
if ($RequireSignature -and ($signature.Status -ne 'Valid' -or -not $signature.TimeStamperCertificate)) {
    throw 'An explicitly requested signed package requires valid Authenticode and timestamp certificates'
}
$out = if ($OutputDirectory) { [IO.Path]::GetFullPath($OutputDirectory) } else { Join-Path $project ('dist/windows-x64-' + $Version + '-' + (Get-Date -Format 'yyyyMMddTHHmmss')) }
if (Test-Path $out) { throw "Package output must be a new directory: $out" }
$payload = Join-Path $out $productName
$notices = Join-Path $payload 'Notices'
New-Item -ItemType Directory -Force $notices | Out-Null
Copy-Item $source $payload
Copy-Item "$project/LICENSE" "$notices/Application-GPLv3.txt"
Copy-Item "$project/Vendor/PROVENANCE.md" $notices
Copy-Item "$project/Vendor/rubberband/COPYING" "$notices/Rubber-Band-GPLv2.txt"
Copy-Item "$project/Vendor/soundtouch/COPYING.TXT" "$notices/SoundTouch-LGPLv2.1.txt"
Copy-Item "$project/.cache/deps/juce-src/LICENSE.md" "$notices/JUCE-LICENSE.md"
foreach ($folder in @('Resources/Fonts', 'Resources/Icons/Lucide')) {
    # Older application tags may predate the optional Lucide resource folder.
    if (-not (Test-Path (Join-Path $project $folder))) { continue }
    Get-ChildItem (Join-Path $project $folder) -File | Where-Object { $_.Extension -in '.txt','.md' } | ForEach-Object {
        $name = if ($_.Name -eq 'README.md') { (Split-Path $folder -Leaf) + '-README.md' } else { $_.Name }
        Copy-Item $_.FullName (Join-Path $notices $name)
    }
}
@"
$productName $Version - Windows x64
Extract this ZIP and open $productName.exe. Choose a local folder and import samples.
Edits save automatically. Keep .core-manager when moving or copying a portable project.
No installer, external audio tools, VC runtime installer, or plugins are needed.
Visualizer Device mode needs opt-in telemetry firmware. Device > Firmware downloads the README-listed UF2 for your hardware to Downloads and includes an installation guide. Flashing requires a separate confirmed action.
Firmware downloads and online drum analysis make network requests only when selected; there are no automatic update checks.
Keep the Notices directory. No assets are published by this script.
"@ | Set-Content "$payload/README.txt" -Encoding utf8
$archive = Join-Path $out "_core-sample-manager-$Version-windows-x64.zip"
Compress-Archive -Path $payload -DestinationPath $archive
$signatureEvidence = [ordered]@{
    status = $signature.Status.ToString()
    publisher = if ($signature.SignerCertificate) { $signature.SignerCertificate.Subject } else { $null }
    certificate = if ($signature.SignerCertificate) { $signature.SignerCertificate.Thumbprint } else { $null }
    timestampCertificate = if ($signature.TimeStamperCertificate) { $signature.TimeStamperCertificate.Thumbprint } else { $null }
}
$files = [ordered]@{}
Get-ChildItem $payload -File -Recurse | ForEach-Object {
    $relative = $_.FullName.Substring($payload.Length + 1).Replace('\', '/')
    $files[$relative] = (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLower()
}
$manifest = [ordered]@{ application=$productName; version=$Version; platform='windows-x64'; runtime='static MSVC'; signature=$signatureEvidence; files=$files; publication='disabled; local artifacts only'; juce='9.0.3'; archiveSHA256=(Get-FileHash $archive -Algorithm SHA256).Hash.ToLower() }
$manifestPath = Join-Path $out 'manifest.json'
$manifest | ConvertTo-Json -Depth 5 | Set-Content $manifestPath -Encoding utf8
@($archive,$manifestPath) | ForEach-Object { (Get-FileHash $_ -Algorithm SHA256).Hash.ToLower() + '  ' + (Split-Path $_ -Leaf) } | Set-Content "$out/SHA256SUMS.txt" -Encoding ascii
Write-Host $out
