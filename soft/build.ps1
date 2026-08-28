# build.ps1 — 编译 silsph_soft.dll（软件渲染管线）+ demo_soft.exe
$ErrorActionPreference = "Stop"
$w64 = "D:\JAVA-Qt\w64devkit\w64devkit\bin"
$env:PATH = "$w64;$env:PATH"   # gcc 内部要调用 as/ar 等，必须进 PATH
$gcc = "$w64\gcc.exe"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $here

Write-Host "== 1/2 编译 silsph_soft.dll =="
& $gcc -O2 -std=c11 -Wall -Wextra -DSP_BUILD_DLL -shared -o silsph_soft.dll silsph_soft.c -ladvapi32 "-Wl,--out-implib,silsph_soft.dll.a"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "== 2/2 编译 demo_soft.exe =="
& $gcc -O2 -std=c11 -Wall -Wextra -o demo_soft.exe demo_soft.c -L. -lsilsph_soft
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "== OK: silsph_soft.dll / demo_soft.exe =="
Get-Item silsph_soft.dll, demo_soft.exe | Select-Object Name, Length
