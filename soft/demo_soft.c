// (C) SilentStudio — All Rights Reserved.
// Proprietary license: 未经 SilentStudio 书面许可，禁止复制、分发、修改或使用。
// demo_soft.c — Silsph 软渲染 demo v0.2
// 1) 硬件信息（sp_sysinfo：CPU/内存/GPU/OS，纯 Win32+注册表）
// 2) 帧率/画质控制矩阵：分辨率 x 深度/剔除开关 -> ms/帧 -> FPS
// 3) 帧率控制：限速器（目标 60/30 FPS 实测）
// 4) 画质证据：6 帧 BMP（与 GL demo 同场景）
#include "silsph_soft.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#ifdef _WIN32
#include <windows.h>
#endif

/* 修复 GBK 控制台中文乱码：程序自己声明 UTF-8 输出 */
static void console_utf8(void) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

static double now_ms(void) { return sp_now_ms(); }  /* 跨平台：DLL 内 QPC/clock_gettime */

static void write_bmp(const char* path, const uint32_t* px, int w, int h) {
    int row = w * 4;
    unsigned char hdr[54] = {0};
    unsigned bfSize = 54u + (unsigned)(row * h);
    unsigned si = (unsigned)(row * h);
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
    hdr[34]=(unsigned char)si; hdr[35]=(unsigned char)(si>>8);
    hdr[36]=(unsigned char)(si>>16); hdr[37]=(unsigned char)(si>>24);
    hdr[38]=0x13; hdr[39]=0x0B; hdr[42]=0x13; hdr[43]=0x0B;
    FILE* f = fopen(path, "wb");
    if (!f) { perror(path); return; }
    fwrite(hdr, 1, 54, f);
    for (int y = h - 1; y >= 0; y--)
        fwrite((const unsigned char*)px + (size_t)y * row, 1, (size_t)row, f);
    fclose(f);
}

/* 面法线经模型旋转后 Lambert 光照（GL shader 同公式） */
static void lit_color(const float n[3], const float c[3], float angle, float out[3]) {
    float th = angle, ph = angle * 0.6f;
    float ct = cosf(th), st = sinf(th), cp = cosf(ph), sp = sinf(ph);
    float R[9] = { ct, st*sp, st*cp,  0, cp, -sp,  -st, ct*sp, ct*cp };
    float nr[3] = { R[0]*n[0]+R[1]*n[1]+R[2]*n[2],
                    R[3]*n[0]+R[4]*n[1]+R[5]*n[2],
                    R[6]*n[0]+R[7]*n[1]+R[8]*n[2] };
    float dot = nr[0]*0.5f + nr[1]*0.8f + nr[2]*0.3f;
    float light = 0.22f + 0.78f * (dot > 0 ? dot : 0.0f);
    out[0] = c[0]*light; out[1] = c[1]*light; out[2] = c[2]*light;
}

/* 场景：Qraft 主题立方体 + 16x16 网格 + 轨道相机（与 GL demo 同参数） */
static void draw_scene(int W, int H, float angle) {
    float aspect = (float)W / (float)H;
    sp_matrix_mode(SP_PROJECTION); sp_load_identity();
    sp_perspective(55.0f, aspect, 0.1f, 100.0f);
    sp_matrix_mode(SP_MODELVIEW); sp_load_identity();
    float ca = angle * 0.25f;
    sp_look_at(17.0f*sinf(ca), 8.5f, 17.0f*cosf(ca), 0,0,0, 0,1,0);
    sp_rotate(angle * 57.295779513f, 0, 1, 0);
    sp_rotate(angle * 0.6f * 57.295779513f, 1, 0, 0);

    const float faces[6][6] = {
        {0,0,-1, 1.0f,0.62f,0.25f}, {0,0,1, 0.31f,1.0f,0.99f},
        {-1,0,0, 0.44f,0.69f,1.0f}, {1,0,0, 1.0f,0.44f,0.69f},
        {0,-1,0, 0.61f,1.0f,0.44f}, {0,1,0, 1.0f,0.85f,0.44f},
    };
    for (int i = 0; i < 6; i++) {
        const float* n = faces[i]; const float* c = faces[i] + 3;
        float tx[3] = { n[1] ? 1.0f : 0.0f, n[0] ? 0.0f : 1.0f, 0.0f };
        if (n[2] != 0) { tx[0]=1; tx[1]=0; tx[2]=0; }
        float ty[3] = { n[1]*tx[2]-n[2]*tx[1], n[2]*tx[0]-n[0]*tx[2], n[0]*tx[1]-n[1]*tx[0] };
        float col[3]; lit_color(n, c, angle, col);
        sp_color3f(col[0], col[1], col[2]);
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
    sp_color3f(0.35f, 0.40f, 0.55f);
    sp_begin(SP_LINES);
    for (float x = -8.0f; x <= 8.0f; x += 1.0f) { sp_vertex3f(x,0,-8); sp_vertex3f(x,0,8); }
    for (float z = -8.0f; z <= 8.0f; z += 1.0f) { sp_vertex3f(-8,0,z); sp_vertex3f(8,0,z); }
    sp_end();
}

/* 基准：渲染 frames 帧（角度递增），返回平均 ms/帧 */
static double bench(int W, int H, int depth, int cull, int frames) {
    sp_depth_test(depth);
    sp_cull_face(cull);
    double t0 = now_ms();
    for (int i = 0; i < frames; i++) {
        sp_clear(SP_COLOR | SP_DEPTH);
        draw_scene(W, H, (float)i * 0.05f);
    }
    return (now_ms() - t0) / frames;
}

/* 重度 overdraw 基准：300 随机三角形（一半背面、大量重叠） */
#define NTRIS 300
static float tri_buf[NTRIS][9];
static void gen_tris(void) {
    srand(7);
    for (int i = 0; i < NTRIS; i++) {
        for (int k = 0; k < 3; k++) {
            tri_buf[i][k*3+0] = ((float)(rand()%1000)/1000.0f*6.0f) - 3.0f;
            tri_buf[i][k*3+1] = ((float)(rand()%1000)/1000.0f*6.0f) - 3.0f;
            tri_buf[i][k*3+2] = -(((float)(rand()%1000)/1000.0f*4.0f) + 1.0f);
        }
    }
}
static void draw_tris(void) {
    sp_begin(SP_TRIANGLES);
    for (int i = 0; i < NTRIS; i++)
        for (int k = 0; k < 3; k++)
            sp_vertex3f(tri_buf[i][k*3], tri_buf[i][k*3+1], tri_buf[i][k*3+2]);
    sp_end();
}
static double bench_tris(int depth, int cull, int frames) {
    sp_depth_test(depth);
    sp_cull_face(cull);
    sp_matrix_mode(SP_PROJECTION); sp_load_identity();
    sp_perspective(55.0f, 960.0f/640.0f, 0.1f, 100.0f);
    sp_matrix_mode(SP_MODELVIEW); sp_load_identity();
    sp_look_at(0, 0, 6, 0, 0, 0, 0, 1, 0);
    sp_color3f(1.0f, 0.55f, 0.35f);
    double t0 = now_ms();
    for (int i = 0; i < frames; i++) {
        sp_clear(SP_COLOR | SP_DEPTH);
        draw_tris();
    }
    return (now_ms() - t0) / frames;
}

/* 帧限速器：目标 fps，跑 seconds 秒，返回实际 fps */
static double frame_limit_bench(double target_fps, int seconds) {
    double frame_ms = 1000.0 / target_fps;
    double next = now_ms();
    int frames = 0;
    double t0 = now_ms();
    double end = t0 + seconds * 1000.0;
    while (now_ms() < end) {
        sp_clear(SP_COLOR | SP_DEPTH);
        draw_scene(960, 640, (float)frames * 0.05f);
        frames++;
        next += frame_ms;
        double wait = next - now_ms();
        if (wait > 0.1) sp_sleep_ms(wait);
        else next = now_ms() + frame_ms;   /* 落后：重置相位 */
    }
    return (double)frames * 1000.0 / (now_ms() - t0);
}

static long count_drawn(const unsigned char* px, int w, int h) {
    const uint32_t bg = 0xFF10131Au;
    long n = 0;
    for (long i = 0; i < (long)w * h; i++)
        if (((const uint32_t*)px)[i] != bg) n++;
    return n;
}

static void render_frame(int W, int H, float angle, const char* path) {
    sp_clear(SP_COLOR | SP_DEPTH);
    draw_scene(W, H, angle);
    int w = 0, h = 0;
    const unsigned char* px = sp_pixels(&w, &h);
    write_bmp(path, (const uint32_t*)px, w, h);
    printf("%s: 非背景像素 = %ld\n", path, count_drawn(px, w, h));
}

int main(void) {
    console_utf8();
    if (!sp_create(960, 640)) { printf("sp_create failed\n"); return 1; }
    sp_viewport(0, 0, 960, 640);
    sp_clear_color(0.063f, 0.075f, 0.102f, 1.0f);
    gen_tris();

    /* ---- 1) 硬件信息 ---- */
    sp_sysinfo si;
    if (sp_get_sysinfo(&si)) {
        printf("==== 硬件信息（sp_sysinfo，纯 Win32+注册表，零依赖）====\n");
        printf("CPU : %s\n", si.cpu_name);
        printf("       %d 核心 / %d 线程, %d MHz\n", si.cpu_cores, si.cpu_threads, si.cpu_mhz);
        printf("内存: %llu MB (可用 %llu MB)\n", si.mem_total_mb, si.mem_avail_mb);
        if (si.gpu_vram_mb > 0)
            printf("GPU : %s (%llu MB 显存)\n", si.gpu_name, si.gpu_vram_mb);
        else
            printf("GPU : %s (共享显存/未知)\n", si.gpu_name);
        printf("OS  : Windows %d.%d (build %d)\n", si.os_major, si.os_minor, si.os_build);
    }

    /* ---- 2) 帧率/画质控制矩阵 ---- */
    printf("\n==== 帧率/画质控制（各渲染 1000 帧取平均）====\n");
    printf("%-26s %9s %8s\n", "配置", "ms/帧", "FPS");
    struct { int w, h, depth, cull; const char* name; } cfgs[] = {
        {960, 640, 1, 1, "960x640 深度+剔除开"},
        {960, 640, 0, 0, "960x640 深度+剔除关"},
        {1280, 720, 1, 1, "1280x720 全开"},
        {640, 480, 1, 1, "640x480 全开"},
    };
    for (int i = 0; i < 4; i++) {
        double ms = bench(cfgs[i].w, cfgs[i].h, cfgs[i].depth, cfgs[i].cull, 1000);
        printf("%-26s %9.3f %8.0f\n", cfgs[i].name, ms, 1000.0 / ms);
    }

    /* ---- 2.5) 重度 overdraw：画质开关的真实价值 ---- */
    printf("\n==== 画质开关真实价值（300 随机三角形重度重叠，各 500 帧）====\n");
    printf("%-26s %9s %8s\n", "配置", "ms/帧", "FPS");
    struct { int depth, cull; const char* name; } tc[] = {
        {1, 1, "深度+剔除开(默认)"},
        {0, 1, "深度关"},
        {1, 0, "剔除关"},
        {0, 0, "全关"},
    };
    for (int i = 0; i < 4; i++) {
        double ms = bench_tris(tc[i].depth, tc[i].cull, 500);
        printf("%-26s %9.3f %8.0f\n", tc[i].name, ms, 1000.0 / ms);
    }

    /* ---- 3) 帧率控制（限速器） ---- */
    printf("\n==== 帧率控制（限速器实测 1 秒）====\n");
    double f60 = frame_limit_bench(60.0, 1);
    double f30 = frame_limit_bench(30.0, 1);
    printf("目标 60 FPS -> 实际 %.2f FPS\n", f60);
    printf("目标 30 FPS -> 实际 %.2f FPS\n", f30);

    /* ---- 4) 画质证据：6 帧 BMP（全开画质） ---- */
    sp_depth_test(1); sp_cull_face(1);
    printf("\n==== 画质证据（6 帧 BMP，与 GL demo 同场景）====\n");
    for (int i = 0; i < 6; i++) {
        char name[32];
        sprintf(name, "frame%d.bmp", i);
        render_frame(960, 640, (float)i, name);
    }

    sp_destroy();
    printf("demo_soft done\n");
    return 0;
}
