$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
# Azure's pinned composite action requires pwsh. Use Tape's portable runtime,
# without relying on another runner service's PATH or changing a system install.
$cache = Join-Path $env:RUNNER_TOOL_CACHE 'core-sample-manager-release'
New-Item -ItemType Directory -Force $cache | Out-Null
$archive = Join-Path $cache 'powershell-7.4.13.zip'
$runtime = Join-Path $cache 'powershell-7.4.13'
if (-not (Test-Path $archive)) {
    Invoke-WebRequest -UseBasicParsing 'https://github.com/PowerShell/PowerShell/releases/download/v7.4.13/PowerShell-7.4.13-win-x64.zip' -OutFile $archive
}
if ((Get-FileHash $archive -Algorithm SHA256).Hash -ne '8fb52d2172d285b230c2857a90ba4dd28ecf6477ba4a91f91b6854a647b33b65') {
    throw 'Portable PowerShell download checksum mismatch'
}
if (-not (Test-Path "$runtime/pwsh.exe")) { Expand-Archive $archive $runtime -Force }
$runtime | Out-File $env:GITHUB_PATH -Append -Encoding utf8
