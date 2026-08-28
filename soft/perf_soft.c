// (C) SilentStudio — All Rights Reserved.
// Proprietary license: 未经 SilentStudio 书面许可，禁止复制、分发、修改或使用。
// perf_soft.c — Silsph 软渲染器性能基准（QueryPerformanceCounter 计时）
// 场景 A：demo 场景（立方体 12 三角 + 网格 34 线）
// 场景 B：1000 随机三角形（透视投影铺满屏）
// 场景 C：全屏大三角形（纯像素吞吐）
#include "silsph_soft.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#ifdef _WIN32
#include <windows.h>
#endif

static void console_utf8(void) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

static double now_ms(void) { return sp_now_ms(); }  /* 跨平台 */

/* 场景 A：立方体（同 demo，无网格，简化） */
static void draw_cube(float angle) {
    sp_matrix_mode(SP_PROJECTION); sp_load_identity();
    sp_perspective(55.0f, 960.0f/640.0f, 0.1f, 100.0f);
    sp_matrix_mode(SP_MODELVIEW); sp_load_identity();
    sp_look_at(17.0f*sinf(angle*0.25f), 8.5f, 17.0f*cosf(angle*0.25f), 0,0,0, 0,1,0);
    sp_rotate(angle*57.295779513f, 0,1,0);
    sp_rotate(angle*0.6f*57.295779513f, 1,0,0);
    const float faces[6][6] = {
        {0,0,-1, 1.0f,0.62f,0.25f}, {0,0,1, 0.31f,1.0f,0.99f},
        {-1,0,0, 0.44f,0.69f,1.0f}, {1,0,0, 1.0f,0.44f,0.69f},
        {0,-1,0, 0.61f,1.0f,0.44f}, {0,1,0, 1.0f,0.85f,0.44f},
    };
    for (int i = 0; i < 6; i++) {
        const float* n = faces[i];
        float tx[3] = { n[1] ? 1.0f : 0.0f, n[0] ? 0.0f : 1.0f, 0.0f };
        if (n[2] != 0) { tx[0]=1; tx[1]=0; tx[2]=0; }
        float ty[3] = { n[1]*tx[2]-n[2]*tx[1], n[2]*tx[0]-n[0]*tx[2], n[0]*tx[1]-n[1]*tx[0] };
        sp_color3f(faces[i][3], faces[i][4], faces[i][5]);
        float v[4][3];
        for (int k = 0; k < 4; k++) {
            float sx = (k==1||k==2) ? 1 : -1, sy = (k==2||k==3) ? 1 : -1;
            v[k][0] = n[0]+tx[0]*sx+ty[0]*sy; v[k][1] = n[1]+tx[1]*sx+ty[1]*sy; v[k][2] = n[2]+tx[2]*sx+ty[2]*sy;
        }
        sp_begin(SP_TRIANGLES);
        sp_vertex3f(v[0][0],v[0][1],v[0][2]); sp_vertex3f(v[2][0],v[2][1],v[2][2]); sp_vertex3f(v[1][0],v[1][1],v[1][2]);
        sp_vertex3f(v[0][0],v[0][1],v[0][2]); sp_vertex3f(v[3][0],v[3][1],v[3][2]); sp_vertex3f(v[2][0],v[2][1],v[2][2]);
        sp_end();
    }
}

int main(void) {
    console_utf8();
    if (!sp_create(960, 640)) { printf("create failed\n"); return 1; }
    sp_viewport(0,0,960,640);
    sp_clear_color(0.063f,0.075f,0.102f,1.0f);
    srand(42);

    /* ---- 场景 A：demo 立方体，2000 帧 ---- */
    int N = 2000; double t0 = now_ms();
    for (int i = 0; i < N; i++) {
        sp_clear(SP_COLOR|SP_DEPTH);
        draw_cube((float)i * 0.01f);
    }
    double dtA = (now_ms() - t0) / N;
    printf("A demo立方体  : %.3f ms/帧  (~%.0f FPS 等效, 12 三角形+~2万像素)\n", dtA, 1000.0/dtA);

    /* ---- 场景 B：1000 随机三角形（世界坐标在相机前随机），500 帧 ---- */
    enum { TRI = 1000 };
    float tv[TRI][9];
    for (int i = 0; i < TRI; i++) {
        for (int k = 0; k < 3; k++) {
            /* 相机看原点附近，三角形分布在 [-4,4]^3，z 偏负（相机前） */
            tv[i][k*3+0] = (float)(rand()%1000)/1000.0f*8.0f-4.0f;
            tv[i][k*3+1] = (float)(rand()%1000)/1000.0f*8.0f-4.0f;
            tv[i][k*3+2] = -((float)(rand()%1000)/1000.0f*4.0f+1.0f);
        }
    }
    sp_matrix_mode(SP_PROJECTION); sp_load_identity();
    sp_perspective(55.0f, 960.0f/640.0f, 0.1f, 100.0f);
    sp_matrix_mode(SP_MODELVIEW); sp_load_identity();
    sp_look_at(0,0,6, 0,0,0, 0,1,0);
    sp_color3f(1,0.5f,0.3f);
    N = 500; t0 = now_ms();
    for (int i = 0; i < N; i++) {
        sp_clear(SP_COLOR|SP_DEPTH);
        sp_begin(SP_TRIANGLES);
        for (int j = 0; j < TRI; j++)
            for (int k = 0; k < 3; k++)
                sp_vertex3f(tv[j][k*3], tv[j][k*3+1], tv[j][k*3+2]);
        sp_end();
    }
    double dtB = (now_ms() - t0) / N;
    printf("B 1000三角形  : %.3f ms/帧  (~%.0f 帧/s, %.2f M三角形/s)\n", dtB, 1000.0/dtB, TRI/dtB/1000.0);

    /* ---- 场景 C：全屏大三角形（覆盖 ~60 万像素），200 帧 ---- */
    sp_matrix_mode(SP_PROJECTION); sp_load_identity();
    sp_perspective(55.0f, 960.0f/640.0f, 0.1f, 100.0f);
    sp_matrix_mode(SP_MODELVIEW); sp_load_identity();
    sp_look_at(0,0,2, 0,0,0, 0,1,0);
    sp_color3f(0.9f,0.3f,0.2f);
    N = 200; t0 = now_ms();
    for (int i = 0; i < N; i++) {
        sp_clear(SP_COLOR|SP_DEPTH);
        sp_begin(SP_TRIANGLES);
        sp_vertex3f(-3,-3,-1); sp_vertex3f(3,-3,-1); sp_vertex3f(0,3,-1);
        sp_end();
    }
    double dtC = (now_ms() - t0) / N;
    double pixels = 960.0*640.0;
    printf("C 全屏三角形  : %.3f ms/帧  (~%.0f 帧/s, %.2f M像素/s 含深度测试+透视插值)\n",
           dtC, 1000.0/dtC, pixels/dtC/1000.0);

    sp_destroy();
    return 0;
}
