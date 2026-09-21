param([switch]$Test)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$compiler=Get-Command g++.exe -ErrorAction SilentlyContinue
if (-not $compiler -and (Test-Path 'C:\mingw64\bin\g++.exe')) { $env:PATH='C:\mingw64\bin;'+$env:PATH; $compiler=Get-Command g++.exe }
if ($compiler) {
  cmake -S $projectRoot -B "$projectRoot\build" -G 'MinGW Makefiles' -DCMAKE_BUILD_TYPE=Release
} else {
  cmake -S $projectRoot -B "$projectRoot\build" -A x64
}
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed. Install the Visual Studio Desktop development with C++ workload or MinGW-w64.' }
cmake --build "$projectRoot\build" --config Release --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
if ($Test) { ctest --test-dir "$projectRoot\build" -C Release --output-on-failure; if ($LASTEXITCODE -ne 0) { throw 'Tests failed' } }
