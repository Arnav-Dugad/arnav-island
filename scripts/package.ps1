param([string]$Version='0.28.0-preview.1')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$exe=Join-Path $projectRoot 'build\ArnavIsland.exe'
if(-not(Test-Path $exe)){throw 'Build first with scripts/build.ps1 -Test'}
# 0.18.1: every release is signed with the island's publishing certificate (in the publisher's own certificate store),
# and the updater installs nothing else. Its SHA-256 is the one in src/Productivity/UpdateService.h.
$publisher='d4cbca03ce626894a377bfa9da287808dcb20edd1dee2a8eb51c2e2fef0cb7d4'
$sha=[Security.Cryptography.SHA256]::Create()
function Thumbprint($cert){(($sha.ComputeHash($cert.RawData) | ForEach-Object { $_.ToString('x2') }) -join '')}
$certificate=Get-ChildItem Cert:\CurrentUser\My -CodeSigningCert | Where-Object { (Thumbprint $_) -eq $publisher } | Select-Object -First 1
if(-not $certificate){throw 'The publishing certificate is not on this PC, so the release cannot be signed'}
# Signed and timestamped (or signed alone when the timestamp server can't be reached), then checked.
function Test-Signed($file){
  $check=Get-AuthenticodeSignature -FilePath $file
  # Windows doesn't know the certificate (UnknownError: untrusted root); what matters is that the signature is intact and is the publisher's.
  if($check.Status -ne 'Valid' -and $check.Status -ne 'UnknownError'){throw "The signature of $([IO.Path]::GetFileName($file)) did not verify: $($check.Status)"}
  if((Thumbprint $check.SignerCertificate) -ne $publisher){throw "$([IO.Path]::GetFileName($file)) is signed by another certificate"}
}
function Sign($file){
  $signed=Set-AuthenticodeSignature -FilePath $file -Certificate $certificate -HashAlgorithm SHA256 -TimestampServer 'http://timestamp.digicert.com'
  if(-not $signed.SignerCertificate){$signed=Set-AuthenticodeSignature -FilePath $file -Certificate $certificate -HashAlgorithm SHA256}
  Test-Signed $file
}
Sign $exe
$staging=Join-Path $projectRoot "out\Arnav-Island-$Version-win-x64"
New-Item -ItemType Directory -Path $staging -Force | Out-Null
Copy-Item -LiteralPath $exe -Destination $staging -Force
# 0.27: the phone's camera as a Windows camera: its media source ships beside the exe (signed the same way).
$camera=Join-Path $projectRoot 'build\ArnavIslandCamera.dll'
if(Test-Path $camera){Sign $camera; Copy-Item -LiteralPath $camera -Destination $staging -Force}
Copy-Item -LiteralPath (Join-Path $projectRoot 'LICENSE'),(Join-Path $projectRoot 'THIRD_PARTY_NOTICES.md'),(Join-Path $projectRoot 'docs\QUICK_START.md') -Destination $staging -Force
$archive="$staging.zip"
Compress-Archive -Path "$staging\*" -DestinationPath $archive -Force
$hash=(Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLower()
"$hash  $([IO.Path]::GetFileName($archive))" | Set-Content "$archive.sha256" -Encoding ascii

# 0.28: the installer, beside the ZIP (which the island's updater keeps using). It carries the files just signed, so it's
# built now (the build's exe and camera are those very files), then signed itself.
if(-not(Get-Command g++.exe -ErrorAction SilentlyContinue) -and (Test-Path 'C:\mingw64\bin\g++.exe')){$env:PATH='C:\mingw64\bin;'+$env:PATH}
cmake --build (Join-Path $projectRoot 'build') --config Release --target ArnavIslandSetup
if($LASTEXITCODE -ne 0){throw 'The setup did not build'}
# Building it mustn't have rebuilt the island too (the setup would then carry an unsigned one).
Test-Signed $exe; if(Test-Path $camera){Test-Signed $camera}
$built=@((Join-Path $projectRoot 'build\ArnavIslandSetup.exe'),(Join-Path $projectRoot 'build\Release\ArnavIslandSetup.exe')) | Where-Object { Test-Path $_ } | Select-Object -First 1
if(-not $built){throw 'The setup was not built'}
if((Get-Item $built).VersionInfo.ProductVersion -ne $Version){throw "The setup is version $((Get-Item $built).VersionInfo.ProductVersion), not $Version (src/App/Version.h decides)"}
$setup=Join-Path $projectRoot "out\Arnav-Island-Setup-$Version.exe"
Copy-Item -LiteralPath $built -Destination $setup -Force
Sign $setup
# Tried once into a temporary folder only (/sandbox: no island closed, nothing registered, no shortcut or camera): what it
# puts there must be the release in the ZIP, byte for byte, signatures and all.
$trial=Join-Path ([IO.Path]::GetTempPath()) "Arnav-Island-Setup-check-$PID"
$run=Start-Process -FilePath $setup -ArgumentList '/S',"`"/sandbox=$trial`"" -Wait -PassThru
if($run.ExitCode -ne 0){throw "The setup's trial install failed ($($run.ExitCode))"}
foreach($file in Get-ChildItem -LiteralPath $staging -File){
  $placed=Join-Path $trial $file.Name
  if(-not(Test-Path $placed) -or (Get-FileHash -LiteralPath $placed).Hash -ne (Get-FileHash -LiteralPath $file.FullName).Hash){throw "The setup doesn't carry the release's $($file.Name)"}
}
Test-Signed (Join-Path $trial 'ArnavIsland.exe')
Remove-Item -LiteralPath $trial -Recurse -Force
$hash=(Get-FileHash -LiteralPath $setup -Algorithm SHA256).Hash.ToLower()
"$hash  $([IO.Path]::GetFileName($setup))" | Set-Content "$setup.sha256" -Encoding ascii
Get-Item $archive,"$archive.sha256",$setup,"$setup.sha256" | Select-Object FullName,Length
