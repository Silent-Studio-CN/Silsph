# (C) SilentStudio — All Rights Reserved.
# Proprietary license: 未经 SilentStudio 书面许可，禁止复制、分发、修改或使用。
# pysilsph.py — Silsph 软渲染器 Python ctypes 绑定（零第三方依赖）
# 用法:
#   import pysilsph as sp
#   sp.create(480, 320); sp.clear(sp.SP_COLOR | sp.SP_DEPTH); ...
#   sp.save_bmp("out.bmp")
import ctypes
import os

_dll = ctypes.CDLL(os.path.join(os.path.dirname(os.path.abspath(__file__)), "silsph_soft.dll"))

# ---- 常量 ----
SP_COLOR = 0x4000
SP_DEPTH = 0x0100
SP_ID = 0x0200
SP_POINTS = 0x0000
SP_LINES = 0x0001
SP_LINE_STRIP = 0x0003
SP_TRIANGLES = 0x0004
SP_TRIANGLE_STRIP = 0x0005
SP_TRIANGLE_FAN = 0x0006
SP_MODELVIEW = 0x1700
SP_PROJECTION = 0x1701
SP_TEX_NEAREST = 0
SP_TEX_LINEAR = 1
SP_TEX_REPEAT = 0
SP_TEX_CLAMP = 1

# ---- 签名 ----
_dll.sp_create.argtypes = [ctypes.c_int, ctypes.c_int]
_dll.sp_create.restype = ctypes.c_int
_dll.sp_destroy.argtypes = []
_dll.sp_viewport.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int]
_dll.sp_clear_color.argtypes = [ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_float]
_dll.sp_clear.argtypes = [ctypes.c_uint]
_dll.sp_pixels.argtypes = [ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int)]
_dll.sp_pixels.restype = ctypes.POINTER(ctypes.c_ubyte)
_dll.sp_save_bmp.argtypes = [ctypes.c_char_p]
_dll.sp_save_bmp.restype = ctypes.c_int
_dll.sp_matrix_mode.argtypes = [ctypes.c_int]
_dll.sp_load_identity.argtypes = []
_dll.sp_perspective.argtypes = [ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_float]
_dll.sp_ortho.argtypes = [ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_float]
_dll.sp_look_at.argtypes = [ctypes.c_float] * 9
_dll.sp_rotate.argtypes = [ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_float]
_dll.sp_translate.argtypes = [ctypes.c_float] * 3
_dll.sp_scale.argtypes = [ctypes.c_float] * 3
_dll.sp_push_matrix.argtypes = []
_dll.sp_pop_matrix.argtypes = []
_dll.sp_begin.argtypes = [ctypes.c_int]
_dll.sp_color3f.argtypes = [ctypes.c_float] * 3
_dll.sp_vertex3f.argtypes = [ctypes.c_float] * 3
_dll.sp_end.argtypes = []
_dll.sp_texcoord2f.argtypes = [ctypes.c_float, ctypes.c_float]
_dll.sp_gen_texture.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_char_p]
_dll.sp_gen_texture.restype = ctypes.c_int
_dll.sp_bind_texture.argtypes = [ctypes.c_int]
_dll.sp_blend.argtypes = [ctypes.c_int]
_dll.sp_cull_face.argtypes = [ctypes.c_int]
_dll.sp_depth_test.argtypes = [ctypes.c_int]
_dll.sp_load_id.argtypes = [ctypes.c_int]
_dll.sp_pick_id.argtypes = [ctypes.c_int, ctypes.c_int]
_dll.sp_pick_id.restype = ctypes.c_int
_dll.sp_now_ms.restype = ctypes.c_double
_dll.sp_sleep_ms.argtypes = [ctypes.c_double]

# ---- 便捷封装 ----
def create(w, h):
    return _dll.sp_create(w, h)

def destroy():
    _dll.sp_destroy()

def viewport(x, y, w, h):
    _dll.sp_viewport(x, y, w, h)

def clear_color(r, g, b, a=1.0):
    _dll.sp_clear_color(r, g, b, a)

def clear(flags):
    _dll.sp_clear(flags)

def save_bmp(path):
    return _dll.sp_save_bmp(path.encode("utf-8"))

def matrix_mode(m):
    _dll.sp_matrix_mode(m)

def load_identity():
    _dll.sp_load_identity()

def perspective(fovy, aspect, zn, zf):
    _dll.sp_perspective(fovy, aspect, zn, zf)

def ortho(l, r, b, t, zn, zf):
    _dll.sp_ortho(l, r, b, t, zn, zf)

def look_at(ex, ey, ez, cx, cy, cz, ux, uy, uz):
    _dll.sp_look_at(ex, ey, ez, cx, cy, cz, ux, uy, uz)

def rotate(deg, ax, ay, az):
    _dll.sp_rotate(deg, ax, ay, az)

def translate(x, y, z):
    _dll.sp_translate(x, y, z)

def scale(x, y, z):
    _dll.sp_scale(x, y, z)

def push_matrix():
    _dll.sp_push_matrix()

def pop_matrix():
    _dll.sp_pop_matrix()

def begin(mode):
    _dll.sp_begin(mode)

def color3f(r, g, b):
    _dll.sp_color3f(r, g, b)

def vertex3f(x, y, z):
    _dll.sp_vertex3f(x, y, z)

def end():
    _dll.sp_end()

def texcoord2f(u, v):
    _dll.sp_texcoord2f(u, v)

def gen_texture(w, h, rgba):
    return _dll.sp_gen_texture(w, h, rgba)

def bind_texture(t):
    _dll.sp_bind_texture(t)

def blend(on):
    _dll.sp_blend(1 if on else 0)

def cull_face(on):
    _dll.sp_cull_face(1 if on else 0)

def depth_test(on):
    _dll.sp_depth_test(1 if on else 0)

def load_id(i):
    _dll.sp_load_id(i)

def pick_id(x, y):
    return _dll.sp_pick_id(x, y)

def pixels():
    w, h = ctypes.c_int(), ctypes.c_int()
    p = _dll.sp_pixels(ctypes.byref(w), ctypes.byref(h))
    return p, w.value, h.value

def now_ms():
    return _dll.sp_now_ms()

# ---- 一键渲染场景（与 regress.c 的 cube 场景同参数，用于跨语言确定性验证） ----
def render_cube_scene(w, h, angle=0.0):
    clear(SP_COLOR | SP_DEPTH)
    matrix_mode(SP_PROJECTION); load_identity()
    perspective(55.0, w / h, 0.1, 100.0)
    matrix_mode(SP_MODELVIEW); load_identity()
    ca = angle * 0.25
    look_at(17.0 * math.sin(ca), 8.5, 17.0 * math.cos(ca), 0, 0, 0, 0, 1, 0)
    rotate(angle * 57.295779513, 0, 1, 0)
    rotate(angle * 0.6 * 57.295779513, 1, 0, 0)
    faces = [
        (0,0,-1, 1.0,0.62,0.25), (0,0,1, 0.31,1.0,0.99),
        (-1,0,0, 0.44,0.69,1.0), (1,0,0, 1.0,0.44,0.69),
        (0,-1,0, 0.61,1.0,0.44), (0,1,0, 1.0,0.85,0.44),
    ]
    for (nx, ny, nz, cr, cg, cb) in faces:
        # 与 C 相同：z 面 x 轴 / y 面 x 轴 / x 面 z 轴（单位长度，所有面正常）
        if nz != 0.0:
            tx = [1.0, 0.0, 0.0]
        elif ny != 0.0:
            tx = [1.0, 0.0, 0.0]
        else:
            tx = [0.0, 0.0, 1.0]
        ty = [ny*tx[2]-nz*tx[1], nz*tx[0]-nx*tx[2], nx*tx[1]-ny*tx[0]]
        color3f(cr, cg, cb)
        begin(SP_TRIANGLES)
        for (sa, sb, sc) in [(0,2,1), (0,3,2)]:
            for k in (sa, sb, sc):
                sx = 1 if (k == 1 or k == 2) else -1
                sy = 1 if (k == 2 or k == 3) else -1
                vertex3f(nx + tx[0]*sx + ty[0]*sy,
                         ny + tx[1]*sx + ty[1]*sy,
                         nz + tx[2]*sx + ty[2]*sy)
        end()
    # 半透明板
    blend(True)
    color3f(1, 1, 1)
    begin(SP_TRIANGLES)
    vertex3f(-1.5,-1.5,2.5); vertex3f(1.5,-1.5,2.5); vertex3f(1.5,1.5,2.5)
    vertex3f(-1.5,-1.5,2.5); vertex3f(1.5,1.5,2.5); vertex3f(-1.5,1.5,2.5)
    end()
    blend(False)
    # 网格
    color3f(0.35, 0.40, 0.55)
    begin(SP_LINES)
    x = -8.0
    while x <= 8.0:
        vertex3f(x, 0, -8); vertex3f(x, 0, 8)
        x += 1.0
    z = -8.0
    while z <= 8.0:
        vertex3f(-8, 0, z); vertex3f(8, 0, z)
        z += 1.0
    end()

import math
