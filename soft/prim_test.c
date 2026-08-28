// (C) SilentStudio — All Rights Reserved.
// Proprietary license: 未经 SilentStudio 书面许可，禁止复制、分发、修改或使用。
// prim_test.c — 图元与正交投影验证
// 1) 正交投影矩形（预期精确 = 480x320 = 153600 像素，无透视变形）
// 2) 三角形条带四边形  3) 三角形扇六边形
// 4) 点(5个)  5) 线带折线
#include "silsph_soft.h"
#include <stdio.h>
#include <math.h>

static long count_drawn(void) {
    int w = 0, h = 0;
    const uint32_t* px = (const uint32_t*)sp_pixels(&w, &h);
    const uint32_t bg = 0xFF10131Au;
    long n = 0;
    for (long i = 0; i < (long)w * h; i++) if (px[i] != bg) n++;
    return n;
}

int main(void) {
    if (!sp_create(960, 640)) { printf("create failed\n"); return 1; }
    sp_viewport(0, 0, 960, 640);
    sp_clear_color(0.063f, 0.075f, 0.102f, 1.0f);

    /* ---- 1) 正交投影：矩形 (-0.5..0.5)，正交映射整个屏幕 ---- */
    sp_clear(SP_COLOR | SP_DEPTH);
    sp_matrix_mode(SP_PROJECTION); sp_load_identity();
    sp_ortho(-1.0f, 1.0f, -1.0f, 1.0f, 0.1f, 10.0f);
    sp_matrix_mode(SP_MODELVIEW); sp_load_identity();
    sp_color3f(0.3f, 0.6f, 0.9f);
    sp_begin(SP_TRIANGLES);
    sp_vertex3f(-0.5f,-0.5f,-1.0f); sp_vertex3f(0.5f,-0.5f,-1.0f); sp_vertex3f(0.5f,0.5f,-1.0f);
    sp_vertex3f(-0.5f,-0.5f,-1.0f); sp_vertex3f(0.5f,0.5f,-1.0f);  sp_vertex3f(-0.5f,0.5f,-1.0f);
    sp_end();
    long ortho = count_drawn();
    printf("1) 正交矩形     : %ld 像素 (预期 153600 = 480x320)\n", ortho);

    /* ---- 2) 三角形条带：四边形（透视相机） ---- */
    sp_clear(SP_COLOR | SP_DEPTH);
    sp_matrix_mode(SP_PROJECTION); sp_load_identity();
    sp_perspective(60.0f, 1.5f, 0.1f, 100.0f);
    sp_matrix_mode(SP_MODELVIEW); sp_load_identity();
    sp_look_at(0, 0, 4, 0, 0, 0, 0, 1, 0);
    sp_color3f(0.9f, 0.4f, 0.2f);
    sp_begin(SP_TRIANGLE_STRIP);
    sp_vertex3f(-1,-1,-1); sp_vertex3f(1,-1,-1); sp_vertex3f(-1,1,-1); sp_vertex3f(1,1,-1);
    sp_end();
    long strip = count_drawn();
    printf("2) 条带四边形   : %ld 像素 (>0 且接近普通四边形)\n", strip);

    /* ---- 3) 三角形扇：六边形 ---- */
    sp_clear(SP_COLOR | SP_DEPTH);
    sp_color3f(0.2f, 0.8f, 0.4f);
    sp_begin(SP_TRIANGLE_FAN);
    sp_vertex3f(0,0,-1);
    for (int i = 0; i <= 6; i++) {
        float a = (float)i * 3.14159265f / 3.0f;
        sp_vertex3f(cosf(a), sinf(a), -1.0f);
    }
    sp_end();
    long fan = count_drawn();
    printf("3) 扇六边形     : %ld 像素 (>0)\n", fan);

    /* ---- 4) 点：5 个 ---- */
    sp_clear(SP_COLOR | SP_DEPTH);
    sp_color3f(1.0f, 1.0f, 0.0f);
    sp_begin(SP_POINTS);
    for (int i = -2; i <= 2; i++) sp_vertex3f((float)i * 0.5f, 0.0f, -1.0f);
    sp_end();
    long pts = count_drawn();
    printf("4) 点 x5        : %ld 像素 (预期 5)\n", pts);

    /* ---- 5) 线带：折线 ---- */
    sp_clear(SP_COLOR | SP_DEPTH);
    sp_color3f(1.0f, 0.2f, 0.8f);
    sp_begin(SP_LINE_STRIP);
    sp_vertex3f(-1,-0.5f,-1); sp_vertex3f(-0.3f,0.5f,-1); sp_vertex3f(0.4f,-0.5f,-1); sp_vertex3f(1,0.5f,-1);
    sp_end();
    long line = count_drawn();
    printf("5) 线带折线     : %ld 像素 (>0)\n", line);

    int ok = (ortho == 153600) && (pts == 5) && (strip > 1000) && (fan > 1000) && (line > 200);
    printf("\n%s\n", ok ? "ALL PASS" : "FAILED");
    sp_destroy();
    return ok ? 0 : 1;
}
