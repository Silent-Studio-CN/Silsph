#!/bin/sh
# (C) SilentStudio — All Rights Reserved. Proprietary license.
# build.sh — Linux/macOS 构建（gcc/clang，零依赖）
# 产物: libsilsph_soft.so (Linux) / libsilsph_soft.dylib (macOS) + demo/crop/perf
set -e
CC="${CC:-cc}"
DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$DIR"

case "$(uname -s)" in
  Darwin*) EXT="dylib"; RPATH="-Wl,-rpath,@loader_path" ;;
  *)       EXT="so";    RPATH="-Wl,-rpath,\$ORIGIN" ;;
esac

echo "== 1/2 编译 libsilsph_soft.$EXT =="
$CC -O2 -std=c11 -Wall -Wextra -shared -fPIC -o libsilsph_soft.$EXT silsph_soft.c silsph_res.c

echo "== 2/2 编译 demo/crop/perf =="
for t in demo_soft crop_test perf_soft prim_test scene_test obj_test bmp_test pick_test gizmo_test; do
  $CC -O2 -std=c11 -Wall -Wextra -o $t $t.c -L. -lsilsph_soft $RPATH
done

echo "== OK =="
ls -l libsilsph_soft.$EXT demo_soft crop_test perf_soft
