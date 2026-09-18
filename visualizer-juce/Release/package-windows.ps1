$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = $env:RELEASE_ROOT
$version = $env:RELEASE_VERSION
$exe = Join-Path $root 'payload/zeptocore visualizer.exe'
if (-not (Test-Path $exe -PathType Leaf)) {
    throw "Signed standalone is missing: $exe"
}
$signature = Get-AuthenticodeSignature -LiteralPath $exe
if ($signature.Status -ne 'Valid' -or $null -eq $signature.SignerCertificate) {
    throw "Standalone Authenticode signature is invalid; refusing to package ($($signature.Status))"
}
if ($null -eq $signature.TimeStamperCertificate) {
    throw 'Standalone Authenticode signature is not timestamped; refusing to package'
}
$signatureEvidence = [ordered]@{
    status = [string]$signature.Status
    publisher = $signature.SignerCertificate.Subject
    certificate = $signature.SignerCertificate.Thumbprint
    timestamp_authority = $signature.TimeStamperCertificate.Subject
    timestamp_certificate = $signature.TimeStamperCertificate.Thumbprint
    executable_sha256 = (Get-FileHash $exe -Algorithm SHA256).Hash.ToLowerInvariant()
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
    timestamped = $true
    signature = $signatureEvidence
    tests_run = $false
    built_at = [DateTime]::UtcNow.ToString('o')
    assets = @{ "$prefix-Standalone.zip" = (Get-FileHash $archive -Algorithm SHA256).Hash.ToLowerInvariant() }
} | ConvertTo-Json -Depth 5 | Set-Content $manifest -Encoding utf8
@($archive, $manifest) | ForEach-Object {
    (Get-FileHash $_ -Algorithm SHA256).Hash.ToLowerInvariant() + '  ' + (Split-Path $_ -Leaf)
} | Set-Content (Join-Path $assets "$prefix-SHA256SUMS.txt") -Encoding ascii
