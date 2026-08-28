// (C) SilentStudio — All Rights Reserved.
// Proprietary license: 未经 SilentStudio 书面许可，禁止复制、分发、修改或使用。
// crop_test.c — 近平面裁剪验证
// S1: 三角形穿近平面（1 顶点在相机后）→ 裁剪后应显示被截断的三角形
// S2: 三角形完全在相机后 → 应 0 像素
// S3: 三角形完全在相机前 → 应完整显示
#include "silsph_soft.h"
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#endif

static void console_utf8(void) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

static long count_drawn(void) {
    int w = 0, h = 0;
    const uint32_t* px = (const uint32_t*)sp_pixels(&w, &h);
    const uint32_t bg = 0xFF10131Au;
    long n = 0;
    for (long i = 0; i < (long)w * h; i++) if (px[i] != bg) n++;
    return n;
}

static void scene(const char* name, float ax,float ay,float az, float bx,float by,float bz, float cx,float cy,float cz) {
    sp_clear(SP_COLOR | SP_DEPTH);
    sp_matrix_mode(SP_PROJECTION); sp_load_identity();
    sp_perspective(60.0f, 960.0f/640.0f, 0.1f, 100.0f);
    sp_matrix_mode(SP_MODELVIEW); sp_load_identity();
    sp_look_at(0, 0, 0, 0, 0, -1, 0, 1, 0);   /* 相机在原点，看 -z */
    sp_color3f(0.9f, 0.3f, 0.2f);
    sp_begin(SP_TRIANGLES);
    sp_vertex3f(ax,ay,az); sp_vertex3f(bx,by,bz); sp_vertex3f(cx,cy,cz);
    sp_end();
    printf("%s: 非背景像素 = %ld\n", name, count_drawn());
}

int main(void) {
    console_utf8();
    if (!sp_create(960, 640)) { printf("create failed\n"); return 1; }
    sp_viewport(0, 0, 960, 640);
    sp_clear_color(0.063f, 0.075f, 0.102f, 1.0f);

    /* S1: 顶点 a 在相机后 (z=+0.05 → w<0)，b、c 在相机前 */
    scene("S1 穿近平面(应>0, 截断显示)", -2,-2, 0.05f, 2,-2,-0.5f, 0,2,-0.5f);
    /* S2: 全部在相机后 */
    scene("S2 全在相机后(应=0)", 0,0,0.5f, 1,0,0.5f, 0,1,0.5f);
    /* S3: 全部在相机前 */
    scene("S3 全在相机前(应>0 且>S1)", -2,-2,-0.5f, 2,-2,-0.5f, 0,2,-0.5f);

    /* S4: 地面平面 z 从 -3(前) 到 +1(相机后)，y=0，相机低视角看 -z
       旧逻辑：跨相机的三角形全部消失；裁剪后：连续地面从脚下延伸到远处 */
    sp_clear(SP_COLOR | SP_DEPTH);
    sp_matrix_mode(SP_PROJECTION); sp_load_identity();
    sp_perspective(60.0f, 960.0f/640.0f, 0.1f, 100.0f);
    sp_matrix_mode(SP_MODELVIEW); sp_load_identity();
    sp_look_at(0, 0.3f, 0, 0, 0, -1, 0, 1, 0);
    sp_color3f(0.35f, 0.60f, 0.40f);
    sp_begin(SP_TRIANGLES);
    for (float x = -3; x < 3; x += 1.0f)
        for (float z = -3; z < 1; z += 1.0f) {
            /* CCW（从上方看，法线朝上） */
            sp_vertex3f(x,   0, z);   sp_vertex3f(x+1, 0, z+1); sp_vertex3f(x+1, 0, z);
            sp_vertex3f(x,   0, z);   sp_vertex3f(x,   0, z+1); sp_vertex3f(x+1, 0, z+1);
        }
    sp_end();
    printf("S4 地面穿相机(裁剪后应大片连续): 非背景像素 = %ld\n", count_drawn());

    sp_destroy();
    return 0;
}
