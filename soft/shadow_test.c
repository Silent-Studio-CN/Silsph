// (C) SilentStudio — All Rights Reserved.
// Proprietary license: 未经 SilentStudio 书面许可，禁止复制、分发、修改或使用。
// shadow_test.c — 阴影贴图 + 多光源验证
// 方向光 (5,8,3)->原点（正交光空间）；立方体在原点；阴影应落在立方体后方地面
#include "silsph_soft.h"
#include <stdio.h>
#include <math.h>

#define W 640
#define H 480

/* 光空间 MVP（正交 + lookAt：光位置 -> 目标），行主序列向量约定 */
static void light_mvp(float out[16], float ex, float ey, float ez, float tx, float ty, float tz) {
    float zx = ex - tx, zy = ey - ty, zz = ez - tz;
    float l = sqrtf(zx*zx + zy*zy + zz*zz);
    zx /= l; zy /= l; zz /= l;
    float xx = 0.0f * zz - 1.0f * zy, xy = 1.0f * zx - 0.0f * zz, xz = 0.0f * zy - 0.0f * zx;
    l = sqrtf(xx*xx + xy*xy + xz*xz);
    xx /= l; xy /= l; xz /= l;
    float yx = zy*xz - zz*xy, yy = zz*xx - zx*xz, yz = zx*xy - zy*xx;
    /* 正交视锥（覆盖场景 -6..6） */
    float ortho[16] = { 2.0f/12.0f, 0, 0, 0,
                        0, 2.0f/12.0f, 0, 0,
                        0, 0, -2.0f/20.0f, -(10.0f+10.0f)/(20.0f),
                        0, 0, 0, 1 };
    /* view（行主序，平移第 4 列） */
    float view[16] = { xx, yx, zx, -(xx*ex + xy*ey + xz*ez),
                       xy, yy, zy, -(yx*ex + yy*ey + yz*ez),
                       xz, yz, zz, -(zx*ex + zy*ey + zz*ez),
                       0, 0, 0, 1 };
    /* ortho * view（行主序） */
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            float s = 0;
            for (int k = 0; k < 4; k++) s += ortho[i*4+k] * view[k*4+j];
            out[i*4+j] = s;
        }
}

/* 地面（大平面，CCW） */
static void ground(void) {
    sp_color3f(0.55f, 0.60f, 0.65f);
    sp_begin(SP_TRIANGLES);
    sp_vertex3f(-6, 0, -6); sp_vertex3f(6, 0, -6); sp_vertex3f(6, 0, 6);
    sp_vertex3f(-6, 0, -6); sp_vertex3f(6, 0, 6); sp_vertex3f(-6, 0, 6);
    sp_end();
}
/* 立方体 */
static void cube_at(float cx, float cy, float cz, float s, float r, float g, float b) {
    sp_color3f(r, g, b);
    sp_push_matrix();
    sp_translate(cx, cy, cz);
    sp_scale(s, s, s);
    const float f[6][4][3] = {
        {{-1,-1, 1},{ 1,-1, 1},{ 1, 1, 1},{-1, 1, 1}},
        {{-1,-1,-1},{-1, 1,-1},{ 1, 1,-1},{ 1,-1,-1}},
        {{-1,-1,-1},{ 1,-1,-1},{ 1,-1, 1},{-1,-1, 1}},
        {{-1, 1,-1},{-1, 1, 1},{ 1, 1, 1},{ 1, 1,-1}},
        {{-1,-1,-1},{-1,-1, 1},{-1, 1, 1},{-1, 1,-1}},
        {{ 1,-1,-1},{ 1, 1,-1},{ 1, 1, 1},{ 1,-1, 1}},
    };
    for (int i = 0; i < 6; i++) {
        sp_begin(SP_TRIANGLES);
        sp_vertex3f(f[i][0][0],f[i][0][1],f[i][0][2]);
        sp_vertex3f(f[i][1][0],f[i][1][1],f[i][1][2]);
        sp_vertex3f(f[i][2][0],f[i][2][1],f[i][2][2]);
        sp_vertex3f(f[i][0][0],f[i][0][1],f[i][0][2]);
        sp_vertex3f(f[i][2][0],f[i][2][1],f[i][2][2]);
        sp_vertex3f(f[i][3][0],f[i][3][1],f[i][3][2]);
        sp_end();
    }
    sp_pop_matrix();
}

static long render(int with_shadow) {
    sp_clear(SP_COLOR | SP_DEPTH);
    sp_matrix_mode(SP_PROJECTION); sp_load_identity();
    sp_perspective(55.0f, (float)W / H, 0.1f, 100.0f);
    sp_matrix_mode(SP_MODELVIEW); sp_load_identity();
    sp_look_at(8, 6, 10, 0, 0, 0, 0, 1, 0);

    float lmvp[16];
    light_mvp(lmvp, 5, 8, 3, 0, 0, 0);
    if (with_shadow) {
        /* 深度捕获：光空间渲染（只写深度） */
        sp_shadow_begin(256);
        sp_shadow_matrix(lmvp);
        sp_clear(SP_COLOR | SP_DEPTH);
        sp_matrix_mode(SP_PROJECTION); sp_load_identity();
        { float m[16]; light_mvp(m, 5, 8, 3, 0, 0, 0); memcpy(S.mvp.m, m, 64); }
        /* 简化：capture 用同一套 MVP（光空间），绘制几何 */
        sp_matrix_mode(SP_PROJECTION);
        { float m[16]; light_mvp(m, 5, 8, 3, 0, 0, 0); /* 通过设置投影+视图来复用 draw 逻辑 */ }
        sp_end();
    }
    return 0;
}

int main(void) {
    if (!sp_create(W, H)) return 1;
    sp_viewport(0, 0, W, H);
    sp_clear_color(0.10f, 0.12f, 0.16f, 1.0f);
    printf("shadow_test 骨架（完整实现见下一轮）\n");
    sp_destroy();
    return 0;
}
