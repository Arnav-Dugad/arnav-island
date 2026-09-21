param([int]$Seconds=15)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$app=Get-Process NexusIsland -ErrorAction SilentlyContinue | Select-Object -First 1
if(-not $app){$app=Start-Process -FilePath "$projectRoot\build\NexusIsland.exe" -PassThru}
Start-Sleep -Seconds 4
$app.Refresh();$cpuBefore=$app.TotalProcessorTime.TotalSeconds;$start=[Diagnostics.Stopwatch]::StartNew()
Start-Sleep -Seconds $Seconds
$app.Refresh();$elapsed=$start.Elapsed.TotalSeconds;$cpu=$app.TotalProcessorTime.TotalSeconds-$cpuBefore
$result=[ordered]@{scenario='settled idle';elapsed_seconds=$elapsed;cpu_seconds=$cpu;single_core_percent=100*$cpu/$elapsed;working_set_bytes=$app.WorkingSet64;peak_working_set_bytes=$app.PeakWorkingSet64}
$result | ConvertTo-Json | Tee-Object -FilePath "$projectRoot\build\idle-result.json"
