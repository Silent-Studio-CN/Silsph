# -*- coding: utf-8 -*-
# py_test.py — 跨语言确定性验证：Python ctypes 渲染 == C 黄金图
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pysilsph as sp

W, H = 480, 320
assert sp.create(W, H), "create failed"
sp.viewport(0, 0, W, H)
sp.clear_color(0.063, 0.075, 0.102, 1.0)
sp.render_cube_scene(W, H, 0.0)
p, w, h = sp.pixels()
data = bytes((p[i] for i in range(w * h * 4)))

with open("golden/cube.raw", "rb") as f:
    golden = f.read()

diff = sum(1 for i in range(min(len(data), len(golden))) if data[i] != golden[i])
print(f"Python 渲染 {w}x{h}, 与 C 黄金图差异字节 = {diff} / {len(golden)}")
sp.save_bmp("py_out.bmp")
ok = diff == 0 and len(data) == len(golden)
print("ALL PASS" if ok else "FAILED")
sp.destroy()
sys.exit(0 if ok else 1)
