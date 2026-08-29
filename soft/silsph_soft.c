// (C) SilentStudio — All Rights Reserved.
// Proprietary license: 未经 SilentStudio 书面许可，禁止复制、分发、修改或使用。
// silsph_soft.c — Silsph 软件渲染管线（纯 C，零依赖）
// 管线：顶点变换(MVP) → 图元装配 → 背面剔除 → 光栅化(edge function) → 透视校正插值 → 深度测试
#include "silsph_soft.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>   /* SP_DEBUG 调试输出用 */
#ifdef _WIN32
#include <windows.h> /* 硬件信息（注册表/系统 API）+ QPC + Sleep */
#else
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/utsname.h>
#ifdef __APPLE__
#include <sys/sysctl.h>
#include <sys/types.h>
#endif
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MAX_VERTS 8192

/* ================= 数学（行主序 4x4，与 silsph.cpp 约定一致） ================= */
typedef struct { float m[16]; } Mat4;

static void m4_identity(Mat4* o) {
    memset(o->m, 0, sizeof(o->m));
    o->m[0] = o->m[5] = o->m[10] = o->m[15] = 1.0f;
}
static void m4_mul(Mat4* o, const Mat4* a, const Mat4* b) { /* o = a*b */
    Mat4 t;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            float s = 0.0f;
            for (int k = 0; k < 4; k++) s += a->m[i*4+k] * b->m[k*4+j];
            t.m[i*4+j] = s;
        }
    *o = t;
}
/* 标准 OpenGL 透视矩阵（右手系，-z 朝前），列向量约定（v' = M·v）：
   row2 = [0, 0, A, B]，row3 = [0, 0, -1, 0]  →  m[11]=B, m[14]=-1
   （注意：silsph.cpp 是行向量约定，其 m[11]/m[14] 与此相反，勿照搬） */
static void m4_perspective(Mat4* o, float fovy_deg, float aspect, float zn, float zf) {
    float f = 1.0f / tanf(fovy_deg * 0.5f * (float)(M_PI / 180.0));
    memset(o->m, 0, sizeof(o->m));
    o->m[0]  = f / aspect;
    o->m[5]  = f;
    o->m[10] = (zf + zn) / (zn - zf);
    o->m[11] = (2.0f * zf * zn) / (zn - zf);
    o->m[14] = -1.0f;
    /* m[15] = 0 */
}
/* 标准 OpenGL 正交矩阵（行主序，列向量约定）：平移放第 4 列 m[3]/m[7]/m[11]（与 lookAt 一致） */
static void m4_ortho(Mat4* o, float l, float r, float b, float t, float zn, float zf) {
    memset(o->m, 0, sizeof(o->m));
    o->m[0]  = 2.0f / (r - l);
    o->m[5]  = 2.0f / (t - b);
    o->m[10] = -2.0f / (zf - zn);
    o->m[3]  = -(r + l) / (r - l);
    o->m[7]  = -(t + b) / (t - b);
    o->m[11] = -(zf + zn) / (zf - zn);
    o->m[15] = 1.0f;
}
static void m4_look_at(Mat4* o, float ex, float ey, float ez,
                       float cx, float cy, float cz,
                       float ux, float uy, float uz) {
    /* z = eye - center（相机看向 -z） */
    float zx = ex-cx, zy = ey-cy, zz = ez-cz;
    float len = sqrtf(zx*zx + zy*zy + zz*zz);
    zx /= len; zy /= len; zz /= len;
    /* x = up x z */
    float xx = uy*zz - uz*zy, xy = uz*zx - ux*zz, xz = ux*zy - uy*zx;
    len = sqrtf(xx*xx + xy*xy + xz*xz);
    xx /= len; xy /= len; xz /= len;
    /* y = z x x */
    float yx = zy*xz - zz*xy, yy = zz*xx - zx*xz, yz = zx*xy - zy*xx;
    /* 行主序 [R t; 0 1]：R 的行 = 基向量，平移 t = -R*eye 在【第 4 列】m[3]/m[7]/m[11]！ */
    o->m[0]=xx; o->m[1]=xy; o->m[2]=xz;  o->m[3]=-(xx*ex + xy*ey + xz*ez);
    o->m[4]=yx; o->m[5]=yy; o->m[6]=yz;  o->m[7]=-(yx*ex + yy*ey + yz*ez);
    o->m[8]=zx; o->m[9]=zy; o->m[10]=zz; o->m[11]=-(zx*ex + zy*ey + zz*ez);
    o->m[12]=0.0f; o->m[13]=0.0f; o->m[14]=0.0f; o->m[15]=1.0f;
}
static void m4_translate(Mat4* o, float x, float y, float z) {
    memset(o->m, 0, sizeof(o->m));
    o->m[0]=1.0f; o->m[5]=1.0f; o->m[10]=1.0f; o->m[15]=1.0f;
    o->m[3]=x; o->m[7]=y; o->m[11]=z;   /* 平移在第 4 列 */
}
static void m4_scale(Mat4* o, float x, float y, float z) {
    memset(o->m, 0, sizeof(o->m));
    o->m[0]=x; o->m[5]=y; o->m[10]=z; o->m[15]=1.0f;
}
/* 当前矩阵 *= 绕任意轴旋转 */
static void m4_rotate(Mat4* o, float deg, float ax, float ay, float az) {
    float a = deg * (float)(M_PI / 180.0), c = cosf(a), s = sinf(a);
    float l = sqrtf(ax*ax + ay*ay + az*az);
    ax /= l; ay /= l; az /= l;
    Mat4 r;
    r.m[0]=c+ax*ax*(1-c);    r.m[1]=ax*ay*(1-c)-az*s; r.m[2]=ax*az*(1-c)+ay*s; r.m[3]=0;
    r.m[4]=ay*ax*(1-c)+az*s; r.m[5]=c+ay*ay*(1-c);    r.m[6]=ay*az*(1-c)-ax*s; r.m[7]=0;
    r.m[8]=az*ax*(1-c)-ay*s; r.m[9]=az*ay*(1-c)+ax*s; r.m[10]=c+az*az*(1-c);  r.m[11]=0;
    r.m[12]=0; r.m[13]=0; r.m[14]=0; r.m[15]=1;
    Mat4 t; m4_mul(&t, o, &r); *o = t;
}
/* 行主序矩阵变换列向量：o = M * v，o[i] = Σ_j m[i*4+j] * v[j]
   注意：行主序下第 i 行的元素是 m[i*4+0..3]（勿用列主序索引 m[0],m[4],m[8],m[12]！） */
static void m4_transform(const Mat4* m, float x, float y, float z, float w,
                         float* ox, float* oy, float* oz, float* ow) {
    *ox = m->m[0]*x + m->m[1]*y + m->m[2]*z  + m->m[3]*w;
    *oy = m->m[4]*x + m->m[5]*y + m->m[6]*z  + m->m[7]*w;
    *oz = m->m[8]*x + m->m[9]*y + m->m[10]*z + m->m[11]*w;
    *ow = m->m[12]*x + m->m[13]*y + m->m[14]*z + m->m[15]*w;
}

/* ================= 状态机 ================= */
static struct {
    int      width, height;          /* 帧缓冲尺寸 */
    int      vp_x, vp_y, vp_w, vp_h; /* 视口 */
    uint32_t* color;                 /* 颜色缓冲，值 0xAABBGGRR，内存序 B,G,R,A */
    float*   depth;                  /* 深度缓冲（NDC z，-1..1） */
    uint32_t* idbuf;                 /* 拾取 ID 缓冲 */
    int      cur_id;                 /* 当前拾取 ID */
    float    clear_r, clear_g, clear_b;
    int      matrix_mode;
    Mat4     proj, modelview, mvp;
    int      prim_mode;
    int      vcount;
    int      cull_enable;    /* 背面剔除开关 */
    int      depth_enable;   /* 深度测试开关 */
    int      blend_enable;   /* alpha 混合开关 */
    float    cur_r, cur_g, cur_b;
    float    cur_u, cur_v;   /* 当前纹理坐标 */
    int      cur_tex;        /* 当前纹理 ID（0 = 无纹理） */
    struct Vtx { float x, y, z; float r, g, b; float u, v; } verts[MAX_VERTS];
} S;

/* ---- 纹理存储 ---- */
typedef struct { int w, h; unsigned char* rgba; int filter, wrap; } Tex;
static Tex texs[SP_MAX_TEX];

/* clip 空间顶点（透视除法前）：用于近平面裁剪，属性与几何同步插值 */
typedef struct { float x, y, z, w; float r, g, b; float u, v; } ClipV;
/* 屏幕坐标顶点：y 向上（与 GL 窗口坐标一致）+ clip z/w + 颜色 + UV */
typedef struct { float sx, sy; float zc, wc; float r, g, b; float u, v; } Pv;

static void xform_vertex(const struct Vtx* v, ClipV* o) {
    m4_transform(&S.mvp, v->x, v->y, v->z, 1.0f, &o->x, &o->y, &o->z, &o->w);
    o->r = v->r; o->g = v->g; o->b = v->b;
    o->u = v->u; o->v = v->v;
}

/* clip 空间 -> 屏幕坐标（透视除法 + 视口变换） */
static void to_screen(const ClipV* c, Pv* o) {
    float inv = 1.0f / c->w;
    float nx = c->x * inv, ny = c->y * inv;
    o->sx = (nx * 0.5f + 0.5f) * (float)S.vp_w + (float)S.vp_x;
    o->sy = (ny * 0.5f + 0.5f) * (float)S.vp_h + (float)S.vp_y;
    o->zc = c->z; o->wc = c->w;
    o->r = c->r; o->g = c->g; o->b = c->b;
    o->u = c->u; o->v = c->v;
}

/* Sutherland-Hodgman 视锥裁剪（6 平面：x±w、y±w、z±w 近/远）。
   注意：不能裁剪 w=0 平面——交点 w≈0 会投影到 ±1e6 坐标，edge function 浮点精度崩溃；
   近平面的交点 w = near（几何上 = near 处），数值稳定，且近平面在相机前，自动覆盖相机后顶点。 */
static float fclip_xm(const ClipV* p) { return p->x + p->w; }
static float fclip_xp(const ClipV* p) { return p->w - p->x; }
static float fclip_ym(const ClipV* p) { return p->y + p->w; }
static float fclip_yp(const ClipV* p) { return p->w - p->y; }
static float fclip_zm(const ClipV* p) { return p->z + p->w; }
static float fclip_zp(const ClipV* p) { return p->w - p->z; }
static ClipV lerp_v(const ClipV* a, const ClipV* b, float t) {
    ClipV o;
    o.x = a->x + (b->x - a->x) * t;
    o.y = a->y + (b->y - a->y) * t;
    o.z = a->z + (b->z - a->z) * t;
    o.w = a->w + (b->w - a->w) * t;
    o.r = a->r + (b->r - a->r) * t;
    o.g = a->g + (b->g - a->g) * t;
    o.b = a->b + (b->b - a->b) * t;
    o.u = a->u + (b->u - a->u) * t;
    o.v = a->v + (b->v - a->v) * t;
    return o;
}
/* 单平面裁剪：凸多边形 in[nin] -> out（in/out 不得重叠） */
static int clip_plane(const ClipV* in, int nin, ClipV* out, float (*f)(const ClipV*)) {
    int n = 0;
    const ClipV* prev = &in[nin - 1];
    float fprev = f(prev);
    int prev_in = fprev >= 0.0f;
    for (int i = 0; i < nin; i++) {
        const ClipV* cur = &in[i];
        float fcur = f(cur);
        int cur_in = fcur >= 0.0f;
        if (cur_in) {
            if (!prev_in) /* 进入：插值交点 f=0 */
                out[n++] = lerp_v(prev, cur, -fprev / (fcur - fprev));
            out[n++] = *cur;
        } else if (prev_in) /* 离开：插值交点 f=0 */
            out[n++] = lerp_v(prev, cur, -fprev / (fcur - fprev));
        prev = cur; fprev = fcur; prev_in = cur_in;
    }
    return n;
}
/* 完整视锥裁剪（6 平面），凸三角形 -> 至多 9 顶点凸多边形 */
static int clip_frustum(const ClipV in[3], ClipV out[12]) {
    /* 快速路径：三个顶点全在视锥内 → 免裁剪（常见情况，零开销） */
    int all_in = 1;
    for (int i = 0; i < 3 && all_in; i++) {
        const ClipV* p = &in[i];
        if (p->x + p->w < 0.0f || p->w - p->x < 0.0f ||
            p->y + p->w < 0.0f || p->w - p->y < 0.0f ||
            p->z + p->w < 0.0f || p->w - p->z < 0.0f)
            all_in = 0;
    }
    if (all_in) { memcpy(out, in, 3 * sizeof(ClipV)); return 3; }
    ClipV buf[12];
    int n = 3;
    memcpy(buf, in, 3 * sizeof(ClipV));
    n = clip_plane(buf, n, out, fclip_xm); if (n < 3) return 0;
    n = clip_plane(out, n, buf, fclip_xp); if (n < 3) return 0;
    n = clip_plane(buf, n, out, fclip_ym); if (n < 3) return 0;
    n = clip_plane(out, n, buf, fclip_yp); if (n < 3) return 0;
    n = clip_plane(buf, n, out, fclip_zm); if (n < 3) return 0;
    n = clip_plane(out, n, buf, fclip_zp);
    if (n < 3) return 0;
    memcpy(out, buf, (size_t)n * sizeof(ClipV));
    return n;
}

static void put_pixel(int x, int y_up, float zndc, float r, float g, float b, float a) {
    if (x < S.vp_x || x >= S.vp_x + S.vp_w) return;
    if (y_up < S.vp_y || y_up >= S.vp_y + S.vp_h) return;
    int row = S.height - 1 - y_up;          /* 窗口 y 向上 -> 帧缓冲行（顶行=0） */
    float* d = &S.depth[row * S.width + x];
    if (S.depth_enable) {
        if (zndc >= *d) return;             /* 深度测试 LESS */
        *d = zndc;
    }
    uint32_t* px = &S.color[row * S.width + x];
    if (S.blend_enable && a < 1.0f) {       /* alpha 混合：src_alpha / 1-src_alpha */
        uint32_t dst = *px;
        float dr = (float)((dst >> 16) & 0xFF) / 255.0f;
        float dg = (float)((dst >> 8)  & 0xFF) / 255.0f;
        float db = (float)(dst & 0xFF) / 255.0f;
        r = r * a + dr * (1.0f - a);
        g = g * a + dg * (1.0f - a);
        b = b * a + db * (1.0f - a);
    }
    uint32_t ri = (uint32_t)((r < 0 ? 0 : (r > 1 ? 1 : r)) * 255.0f);
    uint32_t gi = (uint32_t)((g < 0 ? 0 : (g > 1 ? 1 : g)) * 255.0f);
    uint32_t bi = (uint32_t)((b < 0 ? 0 : (b > 1 ? 1 : b)) * 255.0f);
    S.color[row * S.width + x] = 0xFF000000u | (ri << 16) | (gi << 8) | bi;
    S.idbuf[row * S.width + x] = (uint32_t)S.cur_id;   /* 拾取 ID 与像素同步写入 */
}

static void sample_tex(const Pv* a, const Pv* b, const Pv* c,
                       float b0, float b1, float b2, float iw,
                       float* r, float* g, float* bb, float* al);

/* ================= 光栅化：三角形（edge function + 重心坐标 + 透视校正） ================= */
static void raster_tri(const Pv* a, const Pv* b, const Pv* c) {
    /* 背面剔除：GL 窗口坐标（y 向上）CCW 为正面；signed area > 0 即 CCW，剔除其余（对应 CULL_BACK） */
    float area = (b->sx - a->sx) * (c->sy - a->sy) - (b->sy - a->sy) * (c->sx - a->sx);
    if (S.cull_enable && area <= 0.0f) return;
    /* 顶点已过近平面裁剪（w > 0），此处不再检查 */
    float inv_area = 1.0f / area;
    int minx = (int)ceilf(fminf(a->sx, fminf(b->sx, c->sx)));
    int maxx = (int)floorf(fmaxf(a->sx, fmaxf(b->sx, c->sx)));
    int miny = (int)ceilf(fminf(a->sy, fminf(b->sy, c->sy)));
    int maxy = (int)floorf(fmaxf(a->sy, fmaxf(b->sy, c->sy)));
    if (minx < S.vp_x) minx = S.vp_x;
    if (maxx > S.vp_x + S.vp_w - 1) maxx = S.vp_x + S.vp_w - 1;
    if (miny < S.vp_y) miny = S.vp_y;
    if (maxy > S.vp_y + S.vp_h - 1) maxy = S.vp_y + S.vp_h - 1;
    for (int y = miny; y <= maxy; y++) {
        float py = (float)y + 0.5f;
        for (int x = minx; x <= maxx; x++) {
            float px = (float)x + 0.5f;
            /* Pineda edge functions：CCW 三角形内部全部 >= 0 */
            float w0 = (b->sx - a->sx) * (py - a->sy) - (b->sy - a->sy) * (px - a->sx);
            float w1 = (c->sx - b->sx) * (py - b->sy) - (c->sy - b->sy) * (px - b->sx);
            float w2 = (a->sx - c->sx) * (py - c->sy) - (a->sy - c->sy) * (px - c->sx);
            if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) continue;
            float b0 = w0 * inv_area, b1 = w1 * inv_area, b2 = w2 * inv_area;
            /* 透视正确插值：clip 坐标在屏幕空间线性；深度 = lerp(z_clip)/lerp(w_clip) */
            float denom = b0 * a->wc + b1 * b->wc + b2 * c->wc;
            if (denom <= 0.0f) continue;
            float zndc = (b0 * a->zc + b1 * b->zc + b2 * c->zc) / denom;
            float iw = b0 / a->wc + b1 / b->wc + b2 / c->wc;
            float r, g, bb, al = 1.0f;
            if (S.cur_tex > 0 && S.cur_tex <= SP_MAX_TEX && texs[S.cur_tex-1].rgba) {
                sample_tex(a, b, c, b0, b1, b2, iw, &r, &g, &bb, &al);
            } else {
                r = (b0 * a->r / a->wc + b1 * b->r / b->wc + b2 * c->r / c->wc) / iw;
                g = (b0 * a->g / a->wc + b1 * b->g / b->wc + b2 * c->g / c->wc) / iw;
                bb = (b0 * a->b / a->wc + b1 * b->b / b->wc + b2 * c->b / c->wc) / iw;
            }
            put_pixel(x, y, zndc, r, g, bb, al);
        }
    }
}

/* ================= 光栅化：点（1 像素） ================= */
static void raster_point(const Pv* p) {
    int x = (int)p->sx, y = (int)p->sy;
    put_pixel(x, y, p->zc / p->wc, p->r, p->g, p->b, 1.0f);
}

/* ================= 纹理采样 ================= */
static void texel_rgba(const Tex* t, int x, int y, float out[4]) {
    if (t->wrap == SP_TEX_REPEAT) {
        x %= t->w; if (x < 0) x += t->w;
        y %= t->h; if (y < 0) y += t->h;
    } else {
        if (x < 0) x = 0; else if (x >= t->w) x = t->w - 1;
        if (y < 0) y = 0; else if (y >= t->h) y = t->h - 1;
    }
    const unsigned char* p = t->rgba + ((size_t)y * t->w + x) * 4;
    out[0] = p[0] / 255.0f; out[1] = p[1] / 255.0f;
    out[2] = p[2] / 255.0f; out[3] = p[3] / 255.0f;
}
/* 透视校正插值 UV 并采样；输出 = 纹素色 × 顶点光照色（GL 语义 frag = texel * light） */
static void sample_tex(const Pv* a, const Pv* b, const Pv* c,
                       float b0, float b1, float b2, float iw,
                       float* r, float* g, float* bb, float* al) {
    float u = (b0*a->u/a->wc + b1*b->u/b->wc + b2*c->u/c->wc) / iw;
    float v = (b0*a->v/a->wc + b1*b->v/b->wc + b2*c->v/c->wc) / iw;
    float vr = (b0*a->r/a->wc + b1*b->r/b->wc + b2*c->r/c->wc) / iw;  /* 顶点光照色 */
    float vg = (b0*a->g/a->wc + b1*b->g/b->wc + b2*c->g/c->wc) / iw;
    float vb = (b0*a->b/a->wc + b1*b->b/b->wc + b2*c->b/c->wc) / iw;
    const Tex* t = &texs[S.cur_tex - 1];
    if (t->wrap == SP_TEX_REPEAT) { u -= floorf(u); v -= floorf(v); }
    else { if (u < 0) u = 0; else if (u > 1) u = 1; if (v < 0) v = 0; else if (v > 1) v = 1; }
    float fx = u * (float)t->w - 0.5f;
    float fy = v * (float)t->h - 0.5f;
    if (t->filter == SP_TEX_LINEAR) {
        int x0 = (int)floorf(fx), y0 = (int)floorf(fy);
        float tx = fx - x0, ty = fy - y0;
        float c00[4], c10[4], c01[4], c11[4];
        texel_rgba(t, x0,   y0,   c00);
        texel_rgba(t, x0+1, y0,   c10);
        texel_rgba(t, x0,   y0+1, c01);
        texel_rgba(t, x0+1, y0+1, c11);
        float tr = c00[0]*(1-tx) + c10[0]*tx, tg = c00[1]*(1-tx) + c10[1]*tx, tb = c00[2]*(1-tx) + c10[2]*tx;
        float br = c01[0]*(1-tx) + c11[0]*tx, bg = c01[1]*(1-tx) + c11[1]*tx, bb2 = c01[2]*(1-tx) + c11[2]*tx;
        float ta = c00[3]*(1-tx) + c10[3]*tx, ba = c01[3]*(1-tx) + c11[3]*tx;
        *r = (tr*(1-ty) + br*ty) * vr;
        *g = (tg*(1-ty) + bg*ty) * vg;
        *bb = (tb*(1-ty) + bb2*ty) * vb;
        *al = ta*(1-ty) + ba*ty;
    } else {
        float tc[4];
        texel_rgba(t, (int)floorf(fx + 0.5f), (int)floorf(fy + 0.5f), tc);
        *r = tc[0] * vr; *g = tc[1] * vg; *bb = tc[2] * vb;
        *al = tc[3];
    }
}

/* ================= 光栅化：线段（DDA + 深度测试） ================= */
static void raster_line(const Pv* a, const Pv* b) {
    if (a->wc <= 0.0f || b->wc <= 0.0f) return;
    float dx = b->sx - a->sx, dy = b->sy - a->sy;
    int steps = (int)fmaxf(fabsf(dx), fabsf(dy)) + 1;
    for (int i = 0; i <= steps; i++) {
        float t = (float)i / (float)steps;
        float x = a->sx + dx * t, y = a->sy + dy * t;
        float zc = a->zc + (b->zc - a->zc) * t;
        float wc = a->wc + (b->wc - a->wc) * t;
        float r = a->r + (b->r - a->r) * t;
        float g = a->g + (b->g - a->g) * t;
        float bb = a->b + (b->b - a->b) * t;
        if (wc <= 0.0f) continue;
        put_pixel((int)x, (int)y, zc / wc, r, g, bb, 1.0f);
    }
}

/* ================= 公共 API ================= */
SP_API int sp_create(int width, int height) {
    S.color = (uint32_t*)malloc((size_t)width * height * sizeof(uint32_t));
    S.depth = (float*)malloc((size_t)width * height * sizeof(float));
    S.idbuf = (uint32_t*)malloc((size_t)width * height * sizeof(uint32_t));
    if (!S.color || !S.depth || !S.idbuf) { sp_destroy(); return 0; }
    S.width = width; S.height = height;
    sp_viewport(0, 0, width, height);
    sp_clear_color(0.0f, 0.0f, 0.0f, 1.0f);
    S.matrix_mode = SP_MODELVIEW;
    m4_identity(&S.proj); m4_identity(&S.modelview); m4_identity(&S.mvp);
    S.cur_r = S.cur_g = S.cur_b = 1.0f;
    S.cur_u = S.cur_v = 0.0f;
    S.cur_tex = 0;
    S.prim_mode = SP_TRIANGLES; S.vcount = 0;
    S.cull_enable = 1; S.depth_enable = 1; S.blend_enable = 0;
    S.cur_id = 0;
    return 1;
}
SP_API void sp_cull_face(int enable) { S.cull_enable = enable ? 1 : 0; }
SP_API void sp_depth_test(int enable) { S.depth_enable = enable ? 1 : 0; }
SP_API void sp_blend(int enable) { S.blend_enable = enable ? 1 : 0; }
SP_API void sp_load_id(int id) { S.cur_id = id; }
SP_API int sp_pick_id(int x, int y) {
    if (!S.idbuf || x < 0 || x >= S.width || y < 0 || y >= S.height) return 0;
    return (int)S.idbuf[(size_t)y * S.width + x];
}

/* ================= 纹理 API ================= */
SP_API int sp_gen_texture(int w, int h, const unsigned char* rgba) {
    if (w <= 0 || h <= 0 || !rgba) return 0;
    for (int i = 0; i < SP_MAX_TEX; i++) {
        if (!texs[i].rgba) {
            texs[i].rgba = (unsigned char*)malloc((size_t)w * h * 4);
            if (!texs[i].rgba) return 0;
            memcpy(texs[i].rgba, rgba, (size_t)w * h * 4);
            texs[i].w = w; texs[i].h = h;
            texs[i].filter = SP_TEX_LINEAR; texs[i].wrap = SP_TEX_REPEAT;
            return i + 1;
        }
    }
    return 0;
}
SP_API void sp_delete_texture(int tex) {
    if (tex < 1 || tex > SP_MAX_TEX) return;
    Tex* t = &texs[tex - 1];
    free(t->rgba);
    memset(t, 0, sizeof(*t));
    if (S.cur_tex == tex) S.cur_tex = 0;
}
SP_API void sp_bind_texture(int tex) { S.cur_tex = tex; }
SP_API void sp_tex_filter(int mode) {
    if (S.cur_tex > 0 && S.cur_tex <= SP_MAX_TEX) texs[S.cur_tex - 1].filter = mode;
}
SP_API void sp_tex_wrap(int mode) {
    if (S.cur_tex > 0 && S.cur_tex <= SP_MAX_TEX) texs[S.cur_tex - 1].wrap = mode;
}
SP_API void sp_texcoord2f(float u, float v) { S.cur_u = u; S.cur_v = v; }

/* ================= 硬件信息（纯 Win32 + 注册表，零第三方依赖） ================= */
SP_API int sp_get_sysinfo(sp_sysinfo* out) {
    if (!out) return 0;
    memset(out, 0, sizeof(*out));
#ifdef _WIN32
    /* CPU：注册表（品牌名 + 标称主频） */
    HKEY key = NULL;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                      "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                      0, KEY_READ, &key) == ERROR_SUCCESS) {
        DWORD sz = sizeof(out->cpu_name);
        RegQueryValueExA(key, "ProcessorNameString", NULL, NULL, (LPBYTE)out->cpu_name, &sz);
        DWORD mhz = 0; sz = sizeof(mhz);
        RegQueryValueExA(key, "~MHz", NULL, NULL, (LPBYTE)&mhz, &sz);
        out->cpu_mhz = (int)mhz;
        RegCloseKey(key);
    }
    /* CPU 核心/线程数 */
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    out->cpu_threads = (int)si.dwNumberOfProcessors;
    DWORD len = 0;
    GetLogicalProcessorInformation(NULL, &len);
    if (len) {
        SYSTEM_LOGICAL_PROCESSOR_INFORMATION* info =
            (SYSTEM_LOGICAL_PROCESSOR_INFORMATION*)malloc(len);
        if (info && GetLogicalProcessorInformation(info, &len)) {
            DWORD n = len / (DWORD)sizeof(*info);
            for (DWORD i = 0; i < n; i++)
                if (info[i].Relationship == RelationProcessorCore) out->cpu_cores++;
        }
        free(info);
    }
    if (out->cpu_cores <= 0) out->cpu_cores = out->cpu_threads;

    /* 内存 */
    MEMORYSTATUSEX ms; ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) {
        out->mem_total_mb = ms.ullTotalPhys / (1024ull * 1024);
        out->mem_avail_mb = ms.ullAvailPhys / (1024ull * 1024);
    }

    /* GPU：显示适配器类注册表，枚举 0000..0009 */
    const char* gpuCls = "SYSTEM\\CurrentControlSet\\Control\\Class\\"
                         "{4d36e968-e325-11ce-bfc1-08002be10318}";
    for (int i = 0; i < 10 && out->gpu_name[0] == 0; i++) {
        char sub[160];
        sprintf(sub, "%s\\%04d", gpuCls, i);
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, sub, 0, KEY_READ, &key) != ERROR_SUCCESS)
            continue;
        DWORD sz = sizeof(out->gpu_name);
        DWORD type = 0;
        LONG qr = RegQueryValueExA(key, "DriverDesc", NULL, &type,
                                   (LPBYTE)out->gpu_name, &sz);
        if (qr == ERROR_SUCCESS && type == REG_SZ && out->gpu_name[0]) {
            BYTE buf[8] = {0}; sz = sizeof(buf);
            if (RegQueryValueExA(key, "HardwareInformation.qwMemorySize",
                                 NULL, NULL, buf, &sz) == ERROR_SUCCESS && sz == 8) {
                unsigned long long vram = 0;
                memcpy(&vram, buf, 8);
                out->gpu_vram_mb = vram / (1024ull * 1024);
            }
        } else {
            out->gpu_name[0] = 0;
        }
        RegCloseKey(key);
    }
    if (out->gpu_name[0] == 0)
        sprintf(out->gpu_name, "(unknown)");

    /* OS 版本：RtlGetVersion（ntdll，GetProcAddress 获取，无需链接） */
    typedef LONG(WINAPI* RtlGetVersionFn)(void*);
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (ntdll) {
        RtlGetVersionFn fn = (RtlGetVersionFn)(void*)GetProcAddress(ntdll, "RtlGetVersion");
        if (fn) {
            struct { ULONG sz; ULONG major, minor, build, rev; } ovi;
            memset(&ovi, 0, sizeof(ovi)); ovi.sz = sizeof(ovi);
            if (fn(&ovi) == 0) {
                out->os_major   = (int)ovi.major;
                out->os_minor   = (int)ovi.minor;
                out->os_build   = (int)ovi.build;
            }
        }
    }
#elif defined(__APPLE__)
    /* macOS：sysctl（零依赖） */
    size_t slen = sizeof(out->cpu_name);
    if (sysctlbyname("machdep.cpu.brand_string", out->cpu_name, &slen, NULL, 0) != 0)
        out->cpu_name[0] = 0;
    int ncpu = 0;
    slen = sizeof(ncpu);
    if (sysctlbyname("hw.logicalcpu", &ncpu, &slen, NULL, 0) == 0) out->cpu_threads = ncpu;
    if (sysctlbyname("hw.physicalcpu", &ncpu, &slen, NULL, 0) == 0) out->cpu_cores = ncpu;
    uint64_t mem = 0;
    slen = sizeof(mem);
    if (sysctlbyname("hw.memsize", &mem, &slen, NULL, 0) == 0)
        out->mem_total_mb = mem / (1024ull * 1024);
    /* Apple Silicon/Intel 统一内存：GPU = SoC 集成（品牌名即芯片名） */
    if (out->cpu_name[0])
        snprintf(out->gpu_name, sizeof(out->gpu_name), "%s (integrated)", out->cpu_name);
    else
        snprintf(out->gpu_name, sizeof(out->gpu_name), "Apple (integrated)");
    char osver[32] = {0};
    slen = sizeof(osver);
    if (sysctlbyname("kern.osproductversion", osver, &slen, NULL, 0) == 0)
        sscanf(osver, "%d.%d", &out->os_major, &out->os_minor);
#else
    /* Linux：/proc/cpuinfo + /proc/meminfo + /sys/class/drm + uname（零依赖） */
    FILE* f = fopen("/proc/cpuinfo", "r");
    if (f) {
        char line[512];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "model name", 10) == 0 && !out->cpu_name[0]) {
                char* p = strchr(line, ':');
                if (p) {
                    p++;
                    while (*p == ' ') p++;
                    char* e = p + strlen(p);
                    while (e > p && (e[-1] == '\n' || e[-1] == ' ')) e--;
                    *e = 0;
                    strncpy(out->cpu_name, p, sizeof(out->cpu_name) - 1);
                }
            } else if (strncmp(line, "cpu MHz", 7) == 0 && out->cpu_mhz == 0) {
                float mhz = 0;
                sscanf(line, "cpu MHz : %f", &mhz);
                out->cpu_mhz = (int)mhz;
            } else if (strncmp(line, "cpu cores", 9) == 0 && out->cpu_cores == 0) {
                sscanf(line, "cpu cores : %d", &out->cpu_cores);
            }
        }
        fclose(f);
    }
    out->cpu_threads = (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (out->cpu_cores <= 0) out->cpu_cores = out->cpu_threads;
    f = fopen("/proc/meminfo", "r");
    if (f) {
        char line[256];
        unsigned long long kb = 0;
        while (fgets(line, sizeof(line), f)) {
            if (sscanf(line, "MemTotal: %llu kB", &kb) == 1)
                out->mem_total_mb = kb / 1024;
            else if (sscanf(line, "MemAvailable: %llu kB", &kb) == 1)
                out->mem_avail_mb = kb / 1024;
        }
        fclose(f);
    }
    /* GPU：/sys/class/drm 第一个 card 的驱动名（i915/amdgpu/nvidia...） */
    DIR* d = opendir("/sys/class/drm");
    if (d) {
        struct dirent* e;
        while ((e = readdir(d)) != NULL && !out->gpu_name[0]) {
            if (strncmp(e->d_name, "card", 4) == 0 && !strchr(e->d_name, '-')) {
                char path[256], ue[256];
                snprintf(path, sizeof(path), "/sys/class/drm/%s/device/uevent", e->d_name);
                FILE* uf = fopen(path, "r");
                if (uf) {
                    while (fgets(ue, sizeof(ue), uf))
                        if (sscanf(ue, "DRIVER=%127s", out->gpu_name) == 1) break;
                    fclose(uf);
                }
                snprintf(path, sizeof(path), "/sys/class/drm/%s/device/mem_info_vram_total",
                         e->d_name);
                uf = fopen(path, "r");
                if (uf) {
                    unsigned long long vram = 0;
                    if (fscanf(uf, "%llu", &vram) == 1)
                        out->gpu_vram_mb = vram / (1024ull * 1024);
                    fclose(uf);
                }
            }
        }
        closedir(d);
    }
    if (!out->gpu_name[0])
        snprintf(out->gpu_name, sizeof(out->gpu_name), "(unknown)");
    struct utsname u;
    if (uname(&u) == 0)
        sscanf(u.release, "%d.%d", &out->os_major, &out->os_minor);
#endif
    return 1;
}
SP_API void sp_destroy(void) {
    free(S.color); free(S.depth); free(S.idbuf);
    for (int i = 0; i < SP_MAX_TEX; i++) { free(texs[i].rgba); memset(&texs[i], 0, sizeof(texs[i])); }
    memset(&S, 0, sizeof(S));
}

/* ================= 平台工具（跨平台） ================= */
SP_API double sp_now_ms(void) {
#ifdef _WIN32
    static LARGE_INTEGER freq; static int once = 0;
    if (!once) { QueryPerformanceFrequency(&freq); once = 1; }
    LARGE_INTEGER t; QueryPerformanceCounter(&t);
    return (double)t.QuadPart * 1000.0 / (double)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
#endif
}
SP_API void sp_sleep_ms(double ms) {
    if (ms <= 0.0) return;
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    struct timespec req, rem;
    req.tv_sec = (time_t)(ms / 1000.0);
    req.tv_nsec = (long)((ms - (double)req.tv_sec * 1000.0) * 1e6);
    while (nanosleep(&req, &rem) == -1 && rem.tv_nsec >= 0) req = rem; /* EINTR 重试 */
#endif
}
SP_API void sp_viewport(int x, int y, int w, int h) {
    S.vp_x = x; S.vp_y = y; S.vp_w = w; S.vp_h = h;
}
SP_API void sp_clear_color(float r, float g, float b, float a) {
    (void)a; S.clear_r = r; S.clear_g = g; S.clear_b = b;
}
SP_API void sp_clear(unsigned flags) {
    if (flags & SP_COLOR) {
        uint32_t c = 0xFF000000u
                   | ((uint32_t)(S.clear_r * 255.0f) << 16)
                   | ((uint32_t)(S.clear_g * 255.0f) << 8)
                   | (uint32_t)(S.clear_b * 255.0f);
        for (int i = 0; i < S.width * S.height; i++) S.color[i] = c;
    }
    if (flags & SP_DEPTH)
        for (int i = 0; i < S.width * S.height; i++) S.depth[i] = 1.0f; /* NDC 最远 */
    if (flags & SP_ID)
        for (int i = 0; i < S.width * S.height; i++) S.idbuf[i] = 0;
}
SP_API const unsigned char* sp_pixels(int* w, int* h) {
    if (w) *w = S.width;
    if (h) *h = S.height;
    return (const unsigned char*)S.color;
}
/* 离屏输出：帧缓冲 -> BMP 文件（32bpp BI_RGB，内存序 B,G,R,A 直接写） */
SP_API int sp_save_bmp(const char* path) {
    if (!S.color || S.width <= 0 || S.height <= 0) return 0;
    int w = S.width, h = S.height, row = w * 4;
    unsigned char hdr[54] = {0};
    unsigned bfSize = 54u + (unsigned)(row * h);
    hdr[0]='B'; hdr[1]='M';
    hdr[2]=(unsigned char)bfSize; hdr[3]=(unsigned char)(bfSize>>8);
    hdr[4]=(unsigned char)(bfSize>>16); hdr[5]=(unsigned char)(bfSize>>24);
    hdr[10]=54;
    hdr[14]=40;
    hdr[18]=(unsigned char)w; hdr[19]=(unsigned char)(w>>8);
    hdr[20]=(unsigned char)(w>>16); hdr[21]=(unsigned char)(w>>24);
    hdr[22]=(unsigned char)h; hdr[23]=(unsigned char)(h>>8);
    hdr[24]=(unsigned char)(h>>16); hdr[25]=(unsigned char)(h>>24);
    hdr[26]=1; hdr[28]=32;
    unsigned si = (unsigned)(row * h);
    hdr[34]=(unsigned char)si; hdr[35]=(unsigned char)(si>>8);
    hdr[36]=(unsigned char)(si>>16); hdr[37]=(unsigned char)(si>>24);
    FILE* f = fopen(path, "wb");
    if (!f) return 0;
    fwrite(hdr, 1, 54, f);
    for (int y = h - 1; y >= 0; y--)   /* BMP bottom-up */
        fwrite((const unsigned char*)S.color + (size_t)y * row, 1, (size_t)row, f);
    fclose(f);
    return 1;
}
SP_API void sp_matrix_mode(int mode) { S.matrix_mode = mode; }
SP_API void sp_load_identity(void) {
    if (S.matrix_mode == SP_PROJECTION) m4_identity(&S.proj);
    else m4_identity(&S.modelview);
}
SP_API void sp_perspective(float fovy_deg, float aspect, float znear, float zfar) {
    m4_perspective(&S.proj, fovy_deg, aspect, znear, zfar);
}
SP_API void sp_ortho(float left, float right, float bottom, float top, float znear, float zfar) {
    m4_ortho(&S.proj, left, right, bottom, top, znear, zfar);
}
SP_API void sp_look_at(float ex, float ey, float ez,
                       float cx, float cy, float cz,
                       float ux, float uy, float uz) {
    m4_look_at(&S.modelview, ex, ey, ez, cx, cy, cz, ux, uy, uz);
}
SP_API void sp_rotate(float deg, float ax, float ay, float az) {
    if (S.matrix_mode == SP_PROJECTION) m4_rotate(&S.proj, deg, ax, ay, az);
    else m4_rotate(&S.modelview, deg, ax, ay, az);
}
SP_API void sp_translate(float x, float y, float z) {
    if (S.matrix_mode == SP_PROJECTION) { Mat4 t; m4_translate(&t, x, y, z); Mat4 r; m4_mul(&r, &S.proj, &t); S.proj = r; }
    else { Mat4 t; m4_translate(&t, x, y, z); Mat4 r; m4_mul(&r, &S.modelview, &t); S.modelview = r; }
}
SP_API void sp_scale(float x, float y, float z) {
    if (S.matrix_mode == SP_PROJECTION) { Mat4 t; m4_scale(&t, x, y, z); Mat4 r; m4_mul(&r, &S.proj, &t); S.proj = r; }
    else { Mat4 t; m4_scale(&t, x, y, z); Mat4 r; m4_mul(&r, &S.modelview, &t); S.modelview = r; }
}

/* ---- MODELVIEW 矩阵栈（父子变换/场景图） ---- */
#define SP_MAX_STACK 16
static Mat4 mv_stack[SP_MAX_STACK];
static int mv_depth = 0;
SP_API void sp_push_matrix(void) {
    if (mv_depth < SP_MAX_STACK) mv_stack[mv_depth++] = S.modelview;
}
SP_API void sp_pop_matrix(void) {
    if (mv_depth > 0) S.modelview = mv_stack[--mv_depth];
}
SP_API void sp_begin(int mode) { S.prim_mode = mode; S.vcount = 0; }
SP_API void sp_color3f(float r, float g, float b) { S.cur_r = r; S.cur_g = g; S.cur_b = b; }
SP_API void sp_vertex3f(float x, float y, float z) {
    if (S.vcount >= MAX_VERTS) return;
    struct Vtx* v = &S.verts[S.vcount++];
    v->x = x; v->y = y; v->z = z;
    v->r = S.cur_r; v->g = S.cur_g; v->b = S.cur_b;
    v->u = S.cur_u; v->v = S.cur_v;
}
/* ---- 图元发射器（共用的装配/裁剪/光栅化路径） ---- */
static void emit_tri(const struct Vtx* v0, const struct Vtx* v1, const struct Vtx* v2) {
    ClipV cv[3];
    xform_vertex(v0, &cv[0]);
    xform_vertex(v1, &cv[1]);
    xform_vertex(v2, &cv[2]);
    ClipV poly[12];
    int np = clip_frustum(cv, poly);   /* 6 平面视锥裁剪 */
#ifdef SP_DEBUG
    if (np >= 3) {
        fprintf(stderr, "tri: np=%d", np);
        for (int k = 0; k < np; k++) {
            Pv s; to_screen(&poly[k], &s);
            fprintf(stderr, " (%.1f,%.1f w=%.3f)", s.sx, s.sy, s.wc);
        }
        fprintf(stderr, "\n");
    }
#endif
    if (np < 3) return;                /* 完全在视锥外 */
    Pv a, b, c;
    to_screen(&poly[0], &a);
    for (int k = 1; k + 1 < np; k++) { /* 扇形三角化（裁剪后至多 9 边形） */
        to_screen(&poly[k],   &b);
        to_screen(&poly[k+1], &c);
        raster_tri(&a, &b, &c);
    }
}
static void emit_line(const struct Vtx* v0, const struct Vtx* v1) {
    ClipV cva, cvb;
    xform_vertex(v0, &cva);
    xform_vertex(v1, &cvb);
    Pv a, b;
    to_screen(&cva, &a);
    to_screen(&cvb, &b);
    raster_line(&a, &b);
}
static void emit_point(const struct Vtx* v0) {
    ClipV cv;
    xform_vertex(v0, &cv);
    if (cv.w <= 0.0f) return;
    Pv p;
    to_screen(&cv, &p);
    raster_point(&p);
}

SP_API void sp_end(void) {
    /* MVP = P * MV */
    m4_mul(&S.mvp, &S.proj, &S.modelview);
    int n = S.vcount;
    const struct Vtx* v = S.verts;
    switch (S.prim_mode) {
    case SP_POINTS:
        for (int i = 0; i < n; i++) emit_point(&v[i]);
        break;
    case SP_LINES:
        for (int i = 0; i + 1 < n; i += 2) emit_line(&v[i], &v[i+1]);
        break;
    case SP_LINE_STRIP:
        for (int i = 0; i + 1 < n; i++) emit_line(&v[i], &v[i+1]);
        break;
    case SP_TRIANGLES:
        for (int i = 0; i + 2 < n; i += 3) emit_tri(&v[i], &v[i+1], &v[i+2]);
        break;
    case SP_TRIANGLE_STRIP:
        /* 条带：每 3 个连续顶点一个三角形，奇数位翻转绕序保持 CCW */
        for (int i = 0; i + 2 < n; i++) {
            if (i & 1) emit_tri(&v[i+1], &v[i], &v[i+2]);
            else       emit_tri(&v[i],   &v[i+1], &v[i+2]);
        }
        break;
    case SP_TRIANGLE_FAN:
        for (int i = 1; i + 1 < n; i++) emit_tri(&v[0], &v[i], &v[i+1]);
        break;
    default:
        break;
    }
    S.vcount = 0;
}
