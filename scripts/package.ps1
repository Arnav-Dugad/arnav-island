param([string]$Version='0.18.1-preview.1')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$exe=Join-Path $projectRoot 'build\ArnavIsland.exe'
if(-not(Test-Path $exe)){throw 'Build first with scripts/build.ps1 -Test'}
# 0.18.1: every release is signed with the island's publishing certificate (in the publisher's own certificate store),
# and the updater installs nothing else. Its SHA-256 is the one in src/Productivity/UpdateService.h.
$publisher='d4cbca03ce626894a377bfa9da287808dcb20edd1dee2a8eb51c2e2fef0cb7d4'
$sha=[Security.Cryptography.SHA256]::Create()
$certificate=Get-ChildItem Cert:\CurrentUser\My -CodeSigningCert | Where-Object { (($sha.ComputeHash($_.RawData) | ForEach-Object { $_.ToString('x2') }) -join '') -eq $publisher } | Select-Object -First 1
if(-not $certificate){throw 'The publishing certificate is not on this PC, so the release cannot be signed'}
$signed=Set-AuthenticodeSignature -FilePath $exe -Certificate $certificate -HashAlgorithm SHA256 -TimestampServer 'http://timestamp.digicert.com'
if(-not $signed.SignerCertificate){$signed=Set-AuthenticodeSignature -FilePath $exe -Certificate $certificate -HashAlgorithm SHA256}
$check=Get-AuthenticodeSignature -FilePath $exe
# Windows doesn't know the certificate (UnknownError: untrusted root); what matters is that the signature is intact and is the publisher's.
if($check.Status -ne 'Valid' -and $check.Status -ne 'UnknownError'){throw "The signature did not verify: $($check.Status)"}
if((($sha.ComputeHash($check.SignerCertificate.RawData) | ForEach-Object { $_.ToString('x2') }) -join '') -ne $publisher){throw 'The program is signed by another certificate'}
$staging=Join-Path $projectRoot "out\Arnav-Island-$Version-win-x64"
New-Item -ItemType Directory -Path $staging -Force | Out-Null
Copy-Item -LiteralPath $exe -Destination $staging -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'LICENSE'),(Join-Path $projectRoot 'THIRD_PARTY_NOTICES.md'),(Join-Path $projectRoot 'docs\QUICK_START.md') -Destination $staging -Force
$archive="$staging.zip"
Compress-Archive -Path "$staging\*" -DestinationPath $archive -Force
$hash=(Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLower()
"$hash  $([IO.Path]::GetFileName($archive))" | Set-Content "$archive.sha256" -Encoding ascii
Get-Item $archive,"$archive.sha256" | Select-Object FullName,Length
