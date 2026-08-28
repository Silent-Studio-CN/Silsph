// (C) SilentStudio — All Rights Reserved.
// Proprietary license: 未经 SilentStudio 书面许可，禁止复制、分发、修改或使用。
// scene_test.c — 场景图/矩阵栈验证（太阳-地球-月亮父子层级）
// 验证：地球/月亮质心随公转移动；月亮围绕地球；太阳固定在原点
#include "silsph_soft.h"
#include <stdio.h>
#include <math.h>

#define W 960
#define H 640

/* 画 xy 平面圆盘（TRIANGLE_FAN，法线 +z） */
static void disk(float radius, float r, float g, float b) {
    sp_color3f(r, g, b);
    sp_begin(SP_TRIANGLE_FAN);
    sp_vertex3f(0, 0, 0);
    for (int i = 0; i <= 24; i++) {
        float a = (float)i * 6.2831853f / 24.0f;
        sp_vertex3f(cosf(a) * radius, sinf(a) * radius, 0);
    }
    sp_end();
}

static void render(float earth_a) {
    sp_clear(SP_COLOR | SP_DEPTH);
    sp_matrix_mode(SP_PROJECTION); sp_load_identity();
    sp_perspective(60.0f, (float)W / H, 0.1f, 100.0f);
    sp_matrix_mode(SP_MODELVIEW); sp_load_identity();
    sp_look_at(0, 3, 8, 0, 0, 0, 0, 1, 0);
    /* 太阳（原点，自转演示） */
    sp_push_matrix();
    sp_rotate(earth_a * 2, 0, 0, 1);
    disk(1.2f, 0.95f, 0.85f, 0.20f);
    /* 地球（绕太阳公转 + 自转） */
    sp_push_matrix();
    sp_rotate(earth_a, 0, 1, 0);
    sp_translate(3, 0, 0);
    sp_rotate(earth_a * 4, 0, 0, 1);
    disk(0.5f, 0.20f, 0.45f, 0.95f);
    /* 月亮（绕地球公转） */
    sp_push_matrix();
    sp_rotate(earth_a * 5, 0, 1, 0);
    sp_translate(0.9f, 0, 0);
    disk(0.22f, 0.60f, 0.60f, 0.62f);
    sp_pop_matrix();
    sp_pop_matrix();
    sp_pop_matrix();
}

/* 颜色匹配质心：RGB 与目标各分量差 <= tol 的像素求平均 */
static long centroid(const uint32_t* px, int tr, int tg, int tb, int tol,
                     int* ox, int* oy) {
    long sx = 0, sy = 0, n = 0;
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            uint32_t c = px[(size_t)y * W + x];
            int r = (int)((c >> 16) & 0xFF), g = (int)((c >> 8) & 0xFF), b = (int)(c & 0xFF);
            if (abs(r - tr) <= tol && abs(g - tg) <= tol && abs(b - tb) <= tol) {
                sx += x; sy += y; n++;
            }
        }
    }
    *ox = n ? (int)(sx / n) : -1;
    *oy = n ? (int)(sy / n) : -1;
    return n;
}

int main(void) {
    if (!sp_create(W, H)) return 1;
    sp_viewport(0, 0, W, H);
    sp_clear_color(0.063f, 0.075f, 0.102f, 1.0f);

    int sun0x, sun0y, earth0x, earth0y, earth1x, earth1y, moon0x, moon0y;
    long n0, n1;

    render(0.0f);
    {
        const uint32_t* px = (const uint32_t*)sp_pixels(NULL, NULL);
        n0 = centroid(px, 242, 217, 51, 30, &sun0x, &sun0y);   /* 太阳黄 */
        centroid(px, 51, 115, 242, 30, &earth0x, &earth0y);    /* 地球蓝 */
        centroid(px, 153, 153, 158, 20, &moon0x, &moon0y);     /* 月亮灰 */
    }
    render(60.0f);
    {
        const uint32_t* px = (const uint32_t*)sp_pixels(NULL, NULL);
        n1 = centroid(px, 51, 115, 242, 30, &earth1x, &earth1y);
    }

    printf("太阳质心      : (%d, %d) 像素=%ld (预期屏幕中心附近 480,~350)\n", sun0x, sun0y, n0);
    printf("地球质心 0 度 : (%d, %d) 像素=%ld\n", earth0x, earth0y, n0);
    printf("地球质心 60 度: (%d, %d) 像素=%ld\n", earth1x, earth1y, n1);
    printf("月亮质心      : (%d, %d) 像素=%ld\n", moon0x, moon0y, n1);

    int moved = (abs(earth0x - earth1x) + abs(earth0y - earth1y)) > 40;   /* 公转移动 */
    int sun_ok = abs(sun0x - 480) < 40 && abs(sun0y - 350) < 60;
    int moon_ok = (moon0x >= 0) && (abs(moon0x - earth0x) + abs(moon0y - earth0y)) < 200; /* 月亮在地球附近 */

    printf("\n%s\n", (moved && sun_ok && moon_ok && n0 > 1000) ? "ALL PASS" : "FAILED");
    sp_destroy();
    return (moved && sun_ok && moon_ok && n0 > 1000) ? 0 : 1;
}
