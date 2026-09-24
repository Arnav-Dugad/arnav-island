param([string]$Version='0.14.0-preview.1')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$exe=Join-Path $projectRoot 'build\ArnavIsland.exe'
if(-not(Test-Path $exe)){throw 'Build first with scripts/build.ps1 -Test'}
$staging=Join-Path $projectRoot "out\Arnav-Island-$Version-win-x64"
New-Item -ItemType Directory -Path $staging -Force | Out-Null
Copy-Item -LiteralPath $exe -Destination $staging
Copy-Item -LiteralPath (Join-Path $projectRoot 'LICENSE'),(Join-Path $projectRoot 'THIRD_PARTY_NOTICES.md'),(Join-Path $projectRoot 'docs\QUICK_START.md') -Destination $staging
$archive="$staging.zip"
Compress-Archive -Path "$staging\*" -DestinationPath $archive -Force
$hash=(Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLower()
"$hash  $([IO.Path]::GetFileName($archive))" | Set-Content "$archive.sha256" -Encoding ascii
Get-Item $archive,"$archive.sha256" | Select-Object FullName,Length
