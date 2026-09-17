$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = $env:RELEASE_ROOT
$version = $env:RELEASE_VERSION
$exe = Join-Path $root 'payload/zeptocore visualizer.exe'
if ((Get-AuthenticodeSignature $exe).Status -ne 'Valid') {
    throw 'Standalone Authenticode signature is invalid; refusing to package'
}
$prefix = "zeptocore-visualizer-$version-Windows-x64"
$assets = Join-Path $root 'assets'
$archive = Join-Path $assets "$prefix-Standalone.zip"
Compress-Archive -Path "$root/payload/*" -DestinationPath $archive -CompressionLevel Optimal
$manifest = Join-Path $assets "$prefix-manifest.json"
@{
    version = $version
    upload_target = 'latest existing GitHub release at publication time'
    commit = $env:GITHUB_SHA
    architecture = 'x86_64'
    system = 'Windows'
    signed = $true
    tests_run = $false
    built_at = [DateTime]::UtcNow.ToString('o')
    assets = @{ "$prefix-Standalone.zip" = (Get-FileHash $archive -Algorithm SHA256).Hash.ToLowerInvariant() }
} | ConvertTo-Json -Depth 5 | Set-Content $manifest -Encoding utf8
@($archive, $manifest) | ForEach-Object {
    (Get-FileHash $_ -Algorithm SHA256).Hash.ToLowerInvariant() + '  ' + (Split-Path $_ -Leaf)
} | Set-Content (Join-Path $assets "$prefix-SHA256SUMS.txt") -Encoding ascii
