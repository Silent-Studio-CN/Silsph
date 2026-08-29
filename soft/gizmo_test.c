// (C) SilentStudio — All Rights Reserved.
// Proprietary license: 未经 SilentStudio 书面许可，禁止复制、分发、修改或使用。
// gizmo_test.c — 选中描边 + 坐标轴 Gizmo 验证
// 场景：深色立方体(选中,亮黄线框描边) + X红/Y绿/Z蓝 Gizmo
#include "silsph_soft.h"
#include <stdio.h>

static void cube(float r, float g, float b) {
    sp_color3f(r, g, b);
    sp_begin(SP_TRIANGLES);
    /* 6 面 x 2 三角（CCW 外表面），简化手动 */
    const float f[6][4][3] = {
        {{-1,-1, 1},{ 1,-1, 1},{ 1, 1, 1},{-1, 1, 1}},  /* z+ */
        {{-1,-1,-1},{-1, 1,-1},{ 1, 1,-1},{ 1,-1,-1}},  /* z- */
        {{-1,-1,-1},{ 1,-1,-1},{ 1,-1, 1},{-1,-1, 1}},  /* y- */
        {{-1, 1,-1},{-1, 1, 1},{ 1, 1, 1},{ 1, 1,-1}},  /* y+ */
        {{-1,-1,-1},{-1,-1, 1},{-1, 1, 1},{-1, 1,-1}},  /* x- */
        {{ 1,-1,-1},{ 1, 1,-1},{ 1, 1, 1},{ 1,-1, 1}},  /* x+ */
    };
    for (int i = 0; i < 6; i++) {
        sp_vertex3f(f[i][0][0],f[i][0][1],f[i][0][2]);
        sp_vertex3f(f[i][1][0],f[i][1][1],f[i][1][2]);
        sp_vertex3f(f[i][2][0],f[i][2][1],f[i][2][2]);
        sp_vertex3f(f[i][0][0],f[i][0][1],f[i][0][2]);
        sp_vertex3f(f[i][2][0],f[i][2][1],f[i][2][2]);
        sp_vertex3f(f[i][3][0],f[i][3][1],f[i][3][2]);
    }
    sp_end();
}

int main(void) {
    if (!sp_create(960, 640)) return 1;
    sp_viewport(0, 0, 960, 640);
    sp_clear_color(0.05f, 0.06f, 0.09f, 1.0f);
    sp_clear(SP_COLOR | SP_DEPTH);
    sp_matrix_mode(SP_PROJECTION); sp_load_identity();
    sp_perspective(55.0f, 1.5f, 0.1f, 100.0f);
    sp_matrix_mode(SP_MODELVIEW); sp_load_identity();
    sp_look_at(0, 0.5f, 6, 0, 0, 0, 0, 1, 0);

    /* 立方体（深色，避免与 Gizmo 颜色混淆） */
    cube(0.25f, 0.25f, 0.45f);

    /* 选中描边：亮黄线框（12 条边） */
    static const int E[12][2] = {{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
    static const float V[8][3] = {{-1,-1,-1},{1,-1,-1},{1,-1,1},{-1,-1,1},{-1,1,-1},{1,1,-1},{1,1,1},{-1,1,1}};
    sp_color3f(1.0f, 0.9f, 0.2f);
    sp_begin(SP_LINES);
    for (int i = 0; i < 12; i++) {
        sp_vertex3f(V[E[i][0]][0],V[E[i][0]][1],V[E[i][0]][2]);
        sp_vertex3f(V[E[i][1]][0],V[E[i][1]][1],V[E[i][1]][2]);
    }
    sp_end();

    /* Gizmo：轴 + 锥头（关深度 always-on-top） */
    sp_depth_test(0);
    sp_begin(SP_LINES);
    sp_color3f(1.0f,0.30f,0.30f); sp_vertex3f(0,0,0); sp_vertex3f(1.8f,0,0);
    sp_color3f(0.30f,1.0f,0.30f); sp_vertex3f(0,0,0); sp_vertex3f(0,1.8f,0);
    sp_color3f(0.30f,0.50f,1.0f); sp_vertex3f(0,0,0); sp_vertex3f(0,0,1.8f);
    sp_end();
    sp_begin(SP_TRIANGLES);
    sp_color3f(1.0f,0.30f,0.30f);
    sp_vertex3f(2.4f,0,0); sp_vertex3f(1.7f, 0.12f, 0.12f); sp_vertex3f(1.7f,-0.12f, 0.12f);
    sp_vertex3f(2.4f,0,0); sp_vertex3f(1.7f,-0.12f, 0.12f); sp_vertex3f(1.7f,-0.12f,-0.12f);
    sp_vertex3f(2.4f,0,0); sp_vertex3f(1.7f,-0.12f,-0.12f); sp_vertex3f(1.7f, 0.12f,-0.12f);
    sp_vertex3f(2.4f,0,0); sp_vertex3f(1.7f, 0.12f,-0.12f); sp_vertex3f(1.7f, 0.12f, 0.12f);
    sp_color3f(0.30f,1.0f,0.30f);
    sp_vertex3f(0,2.4f,0); sp_vertex3f( 0.12f,1.7f, 0.12f); sp_vertex3f(-0.12f,1.7f, 0.12f);
    sp_vertex3f(0,2.4f,0); sp_vertex3f(-0.12f,1.7f, 0.12f); sp_vertex3f(-0.12f,1.7f,-0.12f);
    sp_vertex3f(0,2.4f,0); sp_vertex3f(-0.12f,1.7f,-0.12f); sp_vertex3f( 0.12f,1.7f,-0.12f);
    sp_vertex3f(0,2.4f,0); sp_vertex3f( 0.12f,1.7f,-0.12f); sp_vertex3f( 0.12f,1.7f, 0.12f);
    sp_color3f(0.30f,0.50f,1.0f);
    sp_vertex3f(0,0,2.4f); sp_vertex3f( 0.12f, 0.12f,1.7f); sp_vertex3f(-0.12f, 0.12f,1.7f);
    sp_vertex3f(0,0,2.4f); sp_vertex3f(-0.12f, 0.12f,1.7f); sp_vertex3f(-0.12f,-0.12f,1.7f);
    sp_vertex3f(0,0,2.4f); sp_vertex3f(-0.12f,-0.12f,1.7f); sp_vertex3f( 0.12f,-0.12f,1.7f);
    sp_vertex3f(0,0,2.4f); sp_vertex3f( 0.12f,-0.12f,1.7f); sp_vertex3f( 0.12f, 0.12f,1.7f);
    sp_end();
    sp_depth_test(1);

    /* 颜色统计（帧缓冲内存序 B,G,R,A → uint32 = A<<24|R<<16|G<<8|B） */
    const uint32_t* px = (const uint32_t*)sp_pixels(NULL, NULL);
    long red = 0, green = 0, blue = 0, yellow = 0;
    for (int i = 0; i < 960 * 640; i++) {
        uint32_t c = px[i];
        int r = (int)((c >> 16) & 0xFF), g = (int)((c >> 8) & 0xFF), b = (int)(c & 0xFF);
        if (r > 180 && g < 110 && b < 110) red++;
        else if (g > 180 && r < 110 && b < 130) green++;
        else if (b > 180 && r < 110 && g < 150) blue++;
        else if (r > 220 && g > 190 && b < 130) yellow++;
    }
    printf("红轴像素=%ld 绿轴像素=%ld 蓝轴像素=%ld 黄描边像素=%ld\n", red, green, blue, yellow);
    int ok = red > 200 && green > 200 && blue > 200 && yellow > 300;
    printf("\n%s\n", ok ? "ALL PASS" : "FAILED");
    sp_destroy();
    return ok ? 0 : 1;
}
