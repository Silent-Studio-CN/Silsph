// (C) SilentStudio — All Rights Reserved.
// Proprietary license: 未经 SilentStudio 书面许可，禁止复制、分发、修改或使用。
// regress.c — 确定性回归测试套件（离屏渲染 + 黄金图像比对）
// 用法: regress.exe --gen    生成黄金图像到 golden/（首次运行）
//       regress.exe           渲染并与 golden/ 比对（CI/回归）
// 场景: 1=立方体+板+网格 2=裁剪地面 3=图元组合
#include "silsph_soft.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define RGW 480
#define RGH 320

/* ---------- 场景 1：立方体 + 半透明板 + 网格（同 demo 参数） ---------- */
static void scene_cube(float angle) {
    sp_clear(SP_COLOR | SP_DEPTH);
    sp_matrix_mode(SP_PROJECTION); sp_load_identity();
    sp_perspective(55.0f, (float)RGW / RGH, 0.1f, 100.0f);
    sp_matrix_mode(SP_MODELVIEW); sp_load_identity();
    float ca = angle * 0.25f;
    sp_look_at(17.0f * sinf(ca), 8.5f, 17.0f * cosf(ca), 0, 0, 0, 0, 1, 0);
    sp_rotate(angle * 57.295779513f, 0, 1, 0);
    sp_rotate(angle * 0.6f * 57.295779513f, 1, 0, 0);
    /* 彩色立方体 */
    const float faces[6][6] = {
        {0,0,-1, 1.0f,0.62f,0.25f}, {0,0,1, 0.31f,1.0f,0.99f},
        {-1,0,0, 0.44f,0.69f,1.0f}, {1,0,0, 1.0f,0.44f,0.69f},
        {0,-1,0, 0.61f,1.0f,0.44f}, {0,1,0, 1.0f,0.85f,0.44f},
    };
    for (int i = 0; i < 6; i++) {
        const float* n = faces[i];
        float tx[3];
        if (n[2] != 0.0f)      { tx[0]=1; tx[1]=0; tx[2]=0; }   /* z 面：x 轴 */
        else if (n[1] != 0.0f) { tx[0]=1; tx[1]=0; tx[2]=0; }   /* y 面：x 轴 */
        else                   { tx[0]=0; tx[1]=0; tx[2]=1; }   /* x 面：z 轴 */
        float ty[3] = { n[1]*tx[2]-n[2]*tx[1], n[2]*tx[0]-n[0]*tx[2], n[0]*tx[1]-n[1]*tx[0] };
        sp_color3f(faces[i][3], faces[i][4], faces[i][5]);
        float v[4][3];
        for (int k = 0; k < 4; k++) {
            float sx = (k==1||k==2) ? 1 : -1, sy = (k==2||k==3) ? 1 : -1;
            v[k][0] = n[0]+tx[0]*sx+ty[0]*sy;
            v[k][1] = n[1]+tx[1]*sx+ty[1]*sy;
            v[k][2] = n[2]+tx[2]*sx+ty[2]*sy;
        }
        sp_begin(SP_TRIANGLES);
        sp_vertex3f(v[0][0],v[0][1],v[0][2]); sp_vertex3f(v[2][0],v[2][1],v[2][2]); sp_vertex3f(v[1][0],v[1][1],v[1][2]);
        sp_vertex3f(v[0][0],v[0][1],v[0][2]); sp_vertex3f(v[3][0],v[3][1],v[3][2]); sp_vertex3f(v[2][0],v[2][1],v[2][2]);
        sp_end();
    }
    /* 半透明板 */
    sp_blend(1);
    sp_color3f(1, 1, 1);
    sp_begin(SP_TRIANGLES);
    sp_vertex3f(-1.5f,-1.5f,2.5f); sp_vertex3f(1.5f,-1.5f,2.5f); sp_vertex3f(1.5f,1.5f,2.5f);
    sp_vertex3f(-1.5f,-1.5f,2.5f); sp_vertex3f(1.5f,1.5f,2.5f); sp_vertex3f(-1.5f,1.5f,2.5f);
    sp_end();
    sp_blend(0);
    /* 网格 */
    sp_color3f(0.35f, 0.40f, 0.55f);
    sp_begin(SP_LINES);
    for (float x = -8.0f; x <= 8.0f; x += 1.0f) { sp_vertex3f(x,0,-8); sp_vertex3f(x,0,8); }
    for (float z = -8.0f; z <= 8.0f; z += 1.0f) { sp_vertex3f(-8,0,z); sp_vertex3f(8,0,z); }
    sp_end();
}

/* ---------- 场景 2：裁剪地面（穿近平面） ---------- */
static void scene_ground(void) {
    sp_clear(SP_COLOR | SP_DEPTH);
    sp_matrix_mode(SP_PROJECTION); sp_load_identity();
    sp_perspective(60.0f, (float)RGW / RGH, 0.1f, 100.0f);
    sp_matrix_mode(SP_MODELVIEW); sp_load_identity();
    sp_look_at(0, 0.3f, 0, 0, 0, -1, 0, 1, 0);
    sp_color3f(0.35f, 0.60f, 0.40f);
    sp_begin(SP_TRIANGLES);
    for (float x = -3.0f; x < 3.0f; x += 1.0f)
        for (float z = -3.0f; z < 1.0f; z += 1.0f) {
            sp_vertex3f(x,0,z);   sp_vertex3f(x+1,0,z+1); sp_vertex3f(x+1,0,z);
            sp_vertex3f(x,0,z);   sp_vertex3f(x,0,z+1);   sp_vertex3f(x+1,0,z+1);
        }
    sp_end();
}

/* ---------- 场景 3：图元组合（条带/扇/点/线带） ---------- */
static void scene_prims(void) {
    sp_clear(SP_COLOR | SP_DEPTH);
    sp_matrix_mode(SP_PROJECTION); sp_load_identity();
    sp_perspective(60.0f, (float)RGW / RGH, 0.1f, 100.0f);
    sp_matrix_mode(SP_MODELVIEW); sp_load_identity();
    sp_look_at(0, 0, 4, 0, 0, 0, 0, 1, 0);
    sp_color3f(0.9f, 0.4f, 0.2f);
    sp_begin(SP_TRIANGLE_STRIP);
    sp_vertex3f(-1,-1,-1); sp_vertex3f(1,-1,-1); sp_vertex3f(-1,1,-1); sp_vertex3f(1,1,-1);
    sp_end();
    sp_color3f(0.2f, 0.8f, 0.4f);
    sp_begin(SP_TRIANGLE_FAN);
    sp_vertex3f(0,0,-1);
    for (int i = 0; i <= 6; i++) {
        float a = (float)i * 3.14159265f / 3.0f;
        sp_vertex3f(cosf(a), sinf(a), -1.0f);
    }
    sp_end();
    sp_color3f(1.0f, 1.0f, 0.0f);
    sp_begin(SP_POINTS);
    for (int i = -2; i <= 2; i++) sp_vertex3f((float)i * 0.5f, 0.0f, -1.0f);
    sp_end();
    sp_color3f(1.0f, 0.2f, 0.8f);
    sp_begin(SP_LINE_STRIP);
    sp_vertex3f(-1,-0.5f,-1); sp_vertex3f(-0.3f,0.5f,-1); sp_vertex3f(0.4f,-0.5f,-1); sp_vertex3f(1,0.5f,-1);
    sp_end();
}

/* ---------- 框架 ---------- */
static const char* g_names[3] = { "cube", "ground", "prims" };
static void (*g_scenes[3])(void) = { NULL, scene_ground, scene_prims };
static void scene_cube_wrap(void) { scene_cube(0.0f); }

static int save_raw(const char* name, const void* buf, size_t sz) {
    char path[256];
    sprintf(path, "golden/%s.raw", name);
    FILE* f = fopen(path, "wb");
    if (!f) return 0;
    size_t w = fwrite(buf, 1, sz, f);
    fclose(f);
    return w == sz;
}
static int load_raw(const char* name, void* buf, size_t sz) {
    char path[256];
    sprintf(path, "golden/%s.raw", name);
    FILE* f = fopen(path, "rb");
    if (!f) return -1;
    size_t got = fread(buf, 1, sz, f);
    fclose(f);
    return got == sz ? 0 : -2;
}

int main(int argc, char** argv) {
    int gen = (argc > 1 && strcmp(argv[1], "--gen") == 0);
    g_scenes[0] = scene_cube_wrap;
    if (!sp_create(RGW, RGH)) { printf("create failed\n"); return 1; }
    sp_viewport(0, 0, RGW, RGH);
    sp_clear_color(0.063f, 0.075f, 0.102f, 1.0f);

    size_t sz = (size_t)RGW * RGH * 4;
    unsigned char* a = (unsigned char*)malloc(sz);
    unsigned char* b = (unsigned char*)malloc(sz);
    int fail = 0;

    for (int i = 0; i < 3; i++) {
        g_scenes[i]();
        memcpy(a, sp_pixels(NULL, NULL), sz);
        if (gen) {
            if (!save_raw(g_names[i], a, sz)) { printf("[%s] 黄金图写入失败\n", g_names[i]); fail = 1; }
            else printf("[%s] 黄金图已生成 golden/%s.raw\n", g_names[i], g_names[i]);
            continue;
        }
        /* 双渲染确定性自检：同一场景再渲一次必须逐字节一致 */
        g_scenes[i]();
        memcpy(b, sp_pixels(NULL, NULL), sz);
        int det = memcmp(a, b, sz);
        /* 与黄金图比对 */
        int r = load_raw(g_names[i], b, sz);
        long diff = 0;
        if (r == 0)
            for (size_t k = 0; k < sz; k++) if (a[k] != b[k]) diff++;
        printf("[%s] 确定性自检=%s, 黄金比对差异像素=%ld (%s)\n",
               g_names[i], det == 0 ? "PASS" : "FAIL",
               diff, r != 0 ? "无黄金图" : (diff == 0 ? "PASS" : "FAIL"));
        if (det != 0 || r != 0 || diff != 0) fail = 1;
    }
    free(a); free(b);
    sp_destroy();
    printf("\n%s\n", fail ? "REGRESS FAILED" : "ALL PASS");
    return fail ? 1 : 0;
}
