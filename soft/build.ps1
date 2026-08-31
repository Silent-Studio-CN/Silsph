# (C) SilentStudio — All Rights Reserved. Proprietary license.
# build.ps1 — 编译 silsph_soft.dll（软件渲染管线）+ demo_soft.exe
$ErrorActionPreference = "Stop"
$w64 = "D:\JAVA-Qt\w64devkit\w64devkit\bin"
$env:PATH = "$w64;$env:PATH"   # gcc 内部要调用 as/ar 等，必须进 PATH
$gcc = "$w64\gcc.exe"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $here

Write-Host "== 1/2 编译 silsph_soft.dll =="
& $gcc -O2 -std=c11 -Wall -Wextra -DSP_BUILD_DLL -shared -o silsph_soft.dll silsph_soft.c silsph_res.c silsph_png.c -ladvapi32 "-Wl,--out-implib,silsph_soft.dll.a"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "== 2/2 编译 demo_soft.exe =="
& $gcc -O2 -std=c11 -Wall -Wextra -o demo_soft.exe demo_soft.c -L. -lsilsph_soft -luser32 -lgdi32
& $gcc -O2 -std=c11 -Wall -Wextra -o crop_test.exe crop_test.c -L. -lsilsph_soft
& $gcc -O2 -std=c11 -Wall -Wextra -o perf_soft.exe perf_soft.c -L. -lsilsph_soft
& $gcc -O2 -std=c11 -Wall -Wextra -o prim_test.exe prim_test.c -L. -lsilsph_soft
& $gcc -O2 -std=c11 -Wall -Wextra -o scene_test.exe scene_test.c -L. -lsilsph_soft
& $gcc -O2 -std=c11 -Wall -Wextra -o obj_test.exe obj_test.c -L. -lsilsph_soft
& $gcc -O2 -std=c11 -Wall -Wextra -o bmp_test.exe bmp_test.c -L. -lsilsph_soft
& $gcc -O2 -std=c11 -Wall -Wextra -o pick_test.exe pick_test.c -L. -lsilsph_soft
& $gcc -O2 -std=c11 -Wall -Wextra -o gizmo_test.exe gizmo_test.c -L. -lsilsph_soft
& $gcc -O2 -std=c11 -Wall -Wextra -o regress.exe regress.c -L. -lsilsph_soft
& $gcc -O2 -std=c11 -Wall -Wextra -o cmd_test.exe cmd_test.c -L. -lsilsph_soft
& $gcc -O2 -std=c11 -Wall -Wextra -o png_test.exe png_test.c -L. -lsilsph_soft
# Rust FFI demo（需 rustc；LoadLibrary 动态加载，无需链接库）
if (Get-Command rustc -ErrorAction SilentlyContinue) {
    & rustc -O sp_rust_demo.rs -o sp_rust_demo.exe
}
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "== OK: silsph_soft.dll / demo_soft.exe =="
Get-Item silsph_soft.dll, demo_soft.exe | Select-Object Name, Length
