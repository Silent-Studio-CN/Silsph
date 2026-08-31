// (C) SilentStudio — All Rights Reserved.
// Proprietary license: 未经 SilentStudio 书面许可，禁止复制、分发、修改或使用。
// shadow_test.c — 阴影贴图验证：方向光下立方体在后方地面投影
// 流程: 光空间(ortho+lookAt)深度捕获 -> 主渲染(相机) + 片元阴影比较
#include "silsph_soft.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define W 640
#define H 480
#define SM 256

static void ground(void) {
    sp_color3f(0.55f, 0.60f, 0.65f);
    sp_begin(SP_TRIANGLES);
    sp_vertex3f(-6, 0, -6); sp_vertex3f(6, 0, -6); sp_vertex3f(6, 0, 6);
    sp_vertex3f(-6, 0, -6); sp_vertex3f(6, 0, 6); sp_vertex3f(-6, 0, 6);
    sp_end();
}
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

static void scene(void) {
    ground();
    cube_at(0, 0.6f, 0, 0.6f, 0.9f, 0.3f, 0.2f);
    cube_at(2.2f, 0.5f, 0.5f, 0.5f, 0.2f, 0.6f, 0.9f);
}

static void light_space(void) {
    /* 光空间：正交视锥 + lookAt(光位置 5,8,3 -> 原点) */
    sp_matrix_mode(SP_PROJECTION); sp_load_identity();
    sp_ortho(-6, 6, -6, 6, 1.0f, 30.0f);
    sp_matrix_mode(SP_MODELVIEW); sp_load_identity();
    sp_look_at(5, 8, 3, 0, 0, 0, 0, 1, 0);
}

static void camera_space(void) {
    sp_matrix_mode(SP_PROJECTION); sp_load_identity();
    sp_perspective(55.0f, (float)W / H, 0.1f, 100.0f);
    sp_matrix_mode(SP_MODELVIEW); sp_load_identity();
    sp_look_at(8, 6, 10, 0, 0, 0, 0, 1, 0);
}

int main(void) {
    if (!sp_create(W, H)) return 1;
    sp_viewport(0, 0, W, H);
    sp_clear_color(0.10f, 0.12f, 0.16f, 1.0f);
    sp_set_threads(1);

    /* ---- 无阴影参考帧 ---- */
    sp_shadow_enable(0);
    camera_space();
    sp_clear(SP_COLOR | SP_DEPTH);
    scene();
    const unsigned char* px0 = sp_pixels(NULL, NULL);
    size_t sz = (size_t)W * H * 4;
    unsigned char* ref = (unsigned char*)malloc(sz);
    memcpy(ref, px0, sz);

    /* ---- 阴影帧：光空间深度捕获 -> 主渲染 + 阴影 ---- */
    sp_shadow_enable(0);
    sp_shadow_begin(SM);
    sp_viewport(0, 0, SM, SM);      /* capture 视口 = shadow 尺寸 */
    light_space();
    sp_clear(SP_COLOR | SP_DEPTH);
    scene();
    sp_shadow_end();
    sp_viewport(0, 0, W, H);        /* 恢复主视口 */
    sp_shadow_enable(1);
    camera_space();
    sp_clear(SP_COLOR | SP_DEPTH);
    scene();
    const unsigned char* px1 = sp_pixels(NULL, NULL);

    /* ---- 验证：阴影区域内像素变暗 ---- */
    long darker = 0;
    for (int i = 0; i < W * H; i++) {
        const uint32_t* a = (const uint32_t*)ref + i;
        const uint32_t* b = (const uint32_t*)px1 + i;
        int ra = (int)((*a >> 16) & 0xFF), ga = (int)((*a >> 8) & 0xFF), ba = (int)(*a & 0xFF);
        int rb = (int)((*b >> 16) & 0xFF), gb = (int)((*b >> 8) & 0xFF), bb = (int)(*b & 0xFF);
        if (ra + ga + ba > 60 && rb + gb + bb < ra + ga + ba - 80) darker++;
    }
    printf("阴影变暗像素 = %ld (预期 > 3000：立方体后方地面)\n", darker);
    int ok = darker > 3000;
    printf("\n%s\n", ok ? "ALL PASS" : "FAILED");
    free(ref);
    sp_destroy();
    return ok ? 0 : 1;
}
