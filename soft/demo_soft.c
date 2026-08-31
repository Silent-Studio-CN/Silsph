// (C) SilentStudio — All Rights Reserved.
// Proprietary license: 未经 SilentStudio 书面许可，禁止复制、分发、修改或使用。
// demo_soft.c — Silsph 软渲染 demo v0.3
// 默认：实时动画窗口（立方体自转 + 轨道相机，标题栏 FPS）
// --info   硬件信息 + 帧率/画质矩阵 + 帧限速（控制台）
// --frames 输出 6 帧 BMP 画质证据（控制台）
#include "silsph_soft.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

/* 半透明棋盘格纹理（alpha=128，混合演示） */
static int g_tex_half = 0;
static void init_half_texture(void) {
    const int W = 64, H = 64;
    unsigned char* px = (unsigned char*)malloc((size_t)W * H * 4);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            int c = ((x / 16) + (y / 16)) & 1;
            unsigned char* p = px + ((size_t)y * W + x) * 4;
            if (c) { p[0]=255; p[1]=255; p[2]=255; }
            else   { p[0]=16;  p[1]=19;  p[2]=26; }
            p[3] = 128;   /* 半透明 */
        }
    g_tex_half = sp_gen_texture(W, H, px);
    free(px);
}

/* 拾取交互：g_pick_mode=1 时 draw_scene 写入物体 ID（立方体=1，板=2） */
static int g_pick_mode = 0;
static int g_pick_req = 0;
static int g_pick_x = 0, g_pick_y = 0;
static int g_selected = 0;   /* 当前选中物体 ID（0=无） */

/* 棋盘格纹理（Qraft 青/深底，8px 格）：懒初始化 */
static int g_tex_checker = 0;
static void init_checker_texture(void) {
    const int W = 64, H = 64;
    unsigned char* px = (unsigned char*)malloc((size_t)W * H * 4);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            int c = ((x / 16) + (y / 16)) & 1;
            unsigned char* p = px + ((size_t)y * W + x) * 4;
            if (c) { p[0]=255; p[1]=255; p[2]=255; }  /* 白格：显示光照本色 */
            else   { p[0]=16;  p[1]=19;  p[2]=26; }    /* 深格 #10131A */
            p[3] = 255;
        }
    g_tex_checker = sp_gen_texture(W, H, px);
    free(px);
}

static void draw_cube_outline(void);
static void draw_gizmo(void);

/* 场景：Qraft 主题立方体(棋盘格纹理) + 16x16 网格 + 轨道相机（与 GL demo 同参数）
   angle 递增 → 立方体自转(绕Y + 绕X摆动) + 相机绕圈（"元素转圈"） */
static void draw_scene(int W, int H, float angle) {
    if (!g_tex_checker) init_checker_texture();
    sp_bind_texture(g_tex_checker);
    float aspect = (float)W / (float)H;
    sp_matrix_mode(SP_PROJECTION); sp_load_identity();
    sp_perspective(55.0f, aspect, 0.1f, 100.0f);
    sp_matrix_mode(SP_MODELVIEW); sp_load_identity();
    float ca = angle * 0.25f;
    sp_look_at(17.0f*sinf(ca), 8.5f, 17.0f*cosf(ca), 0,0,0, 0,1,0);
    sp_rotate(angle * 57.295779513f, 0, 1, 0);
    sp_rotate(angle * 0.6f * 57.295779513f, 1, 0, 0);

    if (g_pick_mode) sp_load_id(1);   /* 立方体 */
    const float faces[6][6] = {
        {0,0,-1, 1.0f,0.62f,0.25f}, {0,0,1, 0.31f,1.0f,0.99f},
        {-1,0,0, 0.44f,0.69f,1.0f}, {1,0,0, 1.0f,0.44f,0.69f},
        {0,-1,0, 0.61f,1.0f,0.44f}, {0,1,0, 1.0f,0.85f,0.44f},
    };
    for (int i = 0; i < 6; i++) {
        const float* n = faces[i]; const float* c = faces[i] + 3;
        float tx[3];
        if (n[2] != 0.0f)      { tx[0]=1; tx[1]=0; tx[2]=0; }   /* z 面：x 轴 */
        else if (n[1] != 0.0f) { tx[0]=1; tx[1]=0; tx[2]=0; }   /* y 面：x 轴 */
        else                   { tx[0]=0; tx[1]=0; tx[2]=1; }   /* x 面：z 轴 */
        float ty[3] = { n[1]*tx[2]-n[2]*tx[1], n[2]*tx[0]-n[0]*tx[2], n[0]*tx[1]-n[1]*tx[0] };
        float col[3]; lit_color(n, c, angle, col);
        sp_color3f(col[0], col[1], col[2]);
        float v[4][3]; float uv[4][2];
        for (int k = 0; k < 4; k++) {
            float sx = (k==1||k==2) ? 1 : -1, sy = (k==2||k==3) ? 1 : -1;
            v[k][0] = n[0]+tx[0]*sx+ty[0]*sy;
            v[k][1] = n[1]+tx[1]*sx+ty[1]*sy;
            v[k][2] = n[2]+tx[2]*sx+ty[2]*sy;
            uv[k][0] = (k==1||k==2) ? 1.0f : 0.0f;
            uv[k][1] = (k==2||k==3) ? 1.0f : 0.0f;
        }
        sp_begin(SP_TRIANGLES);
        sp_texcoord2f(uv[0][0],uv[0][1]); sp_vertex3f(v[0][0],v[0][1],v[0][2]);
        sp_texcoord2f(uv[2][0],uv[2][1]); sp_vertex3f(v[2][0],v[2][1],v[2][2]);
        sp_texcoord2f(uv[1][0],uv[1][1]); sp_vertex3f(v[1][0],v[1][1],v[1][2]);
        sp_texcoord2f(uv[0][0],uv[0][1]); sp_vertex3f(v[0][0],v[0][1],v[0][2]);
        sp_texcoord2f(uv[3][0],uv[3][1]); sp_vertex3f(v[3][0],v[3][1],v[3][2]);
        sp_texcoord2f(uv[2][0],uv[2][1]); sp_vertex3f(v[2][0],v[2][1],v[2][2]);
        sp_end();
    }
    /* 半透明板：立在立方体前方 z=2.5，混合后面景物（painter：不透明先画，透明后画） */
    if (g_pick_mode) sp_load_id(2);   /* 半透明板 */
    if (!g_tex_half) init_half_texture();
    sp_blend(1);
    sp_bind_texture(g_tex_half);
    sp_color3f(1.0f, 1.0f, 1.0f);   /* 光照色=白：板色=纹理色 */
    sp_begin(SP_TRIANGLES);
    sp_texcoord2f(0,0); sp_vertex3f(-1.5f, -1.5f, 2.5f);
    sp_texcoord2f(1,0); sp_vertex3f( 1.5f, -1.5f, 2.5f);
    sp_texcoord2f(1,1); sp_vertex3f( 1.5f,  1.5f, 2.5f);
    sp_texcoord2f(0,0); sp_vertex3f(-1.5f, -1.5f, 2.5f);
    sp_texcoord2f(0,1); sp_vertex3f(-1.5f,  1.5f, 2.5f);
    sp_texcoord2f(1,1); sp_vertex3f( 1.5f,  1.5f, 2.5f);
    sp_end();
    sp_blend(0);
    if (g_pick_mode) sp_load_id(0);   /* 网格不可拾取 */
    sp_bind_texture(0);   /* 网格线纯色 */
    sp_color3f(0.35f, 0.40f, 0.55f);
    sp_begin(SP_LINES);
    for (float x = -8.0f; x <= 8.0f; x += 1.0f) { sp_vertex3f(x,0,-8); sp_vertex3f(x,0,8); }
    for (float z = -8.0f; z <= 8.0f; z += 1.0f) { sp_vertex3f(-8,0,z); sp_vertex3f(8,0,z); }
    sp_end();

    /* 选中编辑辅助：描边 + Gizmo（在模型矩阵下绘制，跟随旋转） */
    if (g_selected == 1) {
        draw_cube_outline();
        draw_gizmo();
    } else if (g_selected == 2) {
        sp_push_matrix();
        sp_translate(0, 0, 2.5f);
        draw_gizmo();
        sp_pop_matrix();
    }
}

/* 立方体 12 条边（选中描边用）：8 顶点索引 */
static const int CUBE_EDGES[12][2] = {
    {0,1},{1,2},{2,3},{3,0},   /* 底 */
    {4,5},{5,6},{6,7},{7,4},   /* 顶 */
    {0,4},{1,5},{2,6},{3,7},   /* 竖 */
};
static void draw_cube_outline(void) {
    static const float cv[8][3] = {
        {-1,-1,-1},{1,-1,-1},{1,-1,1},{-1,-1,1},
        {-1,1,-1},{1,1,-1},{1,1,1},{-1,1,1}};
    sp_bind_texture(0);
    sp_color3f(1.0f, 0.9f, 0.2f);   /* 亮黄描边 */
    sp_begin(SP_LINES);
    for (int i = 0; i < 12; i++) {
        sp_vertex3f(cv[CUBE_EDGES[i][0]][0], cv[CUBE_EDGES[i][0]][1], cv[CUBE_EDGES[i][0]][2]);
        sp_vertex3f(cv[CUBE_EDGES[i][1]][0], cv[CUBE_EDGES[i][1]][1], cv[CUBE_EDGES[i][1]][2]);
    }
    sp_end();
}

/* 坐标轴 Gizmo：X 红 / Y 绿 / Z 蓝，轴长 1.8 + 末端锥形箭头，always-on-top（关深度） */
static void draw_gizmo(void) {
    sp_depth_test(0);
    sp_cull_face(0);
    sp_bind_texture(0);
    /* 三条轴 */
    sp_begin(SP_LINES);
    sp_color3f(1.0f, 0.30f, 0.30f); sp_vertex3f(0,0,0); sp_vertex3f(1.8f,0,0);
    sp_color3f(0.30f, 1.0f, 0.30f); sp_vertex3f(0,0,0); sp_vertex3f(0,1.8f,0);
    sp_color3f(0.30f, 0.50f, 1.0f); sp_vertex3f(0,0,0); sp_vertex3f(0,0,1.8f);
    sp_end();
    /* 箭头锥体（轴末端） */
    sp_begin(SP_TRIANGLES);
    sp_color3f(1.0f, 0.30f, 0.30f);   /* X 锥 */
    sp_vertex3f(2.4f,0,0);   sp_vertex3f(1.7f, 0.12f, 0.12f); sp_vertex3f(1.7f,-0.12f, 0.12f);
    sp_vertex3f(2.4f,0,0);   sp_vertex3f(1.7f,-0.12f, 0.12f); sp_vertex3f(1.7f,-0.12f,-0.12f);
    sp_vertex3f(2.4f,0,0);   sp_vertex3f(1.7f,-0.12f,-0.12f); sp_vertex3f(1.7f, 0.12f,-0.12f);
    sp_vertex3f(2.4f,0,0);   sp_vertex3f(1.7f, 0.12f,-0.12f); sp_vertex3f(1.7f, 0.12f, 0.12f);
    sp_color3f(0.30f, 1.0f, 0.30f);   /* Y 锥 */
    sp_vertex3f(0,2.4f,0);   sp_vertex3f( 0.12f,1.7f, 0.12f); sp_vertex3f(-0.12f,1.7f, 0.12f);
    sp_vertex3f(0,2.4f,0);   sp_vertex3f(-0.12f,1.7f, 0.12f); sp_vertex3f(-0.12f,1.7f,-0.12f);
    sp_vertex3f(0,2.4f,0);   sp_vertex3f(-0.12f,1.7f,-0.12f); sp_vertex3f( 0.12f,1.7f,-0.12f);
    sp_vertex3f(0,2.4f,0);   sp_vertex3f( 0.12f,1.7f,-0.12f); sp_vertex3f( 0.12f,1.7f, 0.12f);
    sp_color3f(0.30f, 0.50f, 1.0f);   /* Z 锥 */
    sp_vertex3f(0,0,2.4f);   sp_vertex3f( 0.12f, 0.12f,1.7f); sp_vertex3f(-0.12f, 0.12f,1.7f);
    sp_vertex3f(0,0,2.4f);   sp_vertex3f(-0.12f, 0.12f,1.7f); sp_vertex3f(-0.12f,-0.12f,1.7f);
    sp_vertex3f(0,0,2.4f);   sp_vertex3f(-0.12f,-0.12f,1.7f); sp_vertex3f( 0.12f,-0.12f,1.7f);
    sp_vertex3f(0,0,2.4f);   sp_vertex3f( 0.12f,-0.12f,1.7f); sp_vertex3f( 0.12f, 0.12f,1.7f);
    sp_end();
    sp_cull_face(1);
    sp_depth_test(1);
}

/* ================= 窗口动画模式（默认，仅 Windows） ================= */
#ifdef _WIN32
static const char* kWinClass = "SilsphSoftWindow";
static HWND g_hwnd = NULL;
static BITMAPINFO g_bmi;
static HBITMAP g_dib = NULL;
static void* g_dib_bits = NULL;
static HDC g_memdc = NULL;
static int g_running = 1;

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_DESTROY: g_running = 0; PostQuitMessage(0); return 0;
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) { DestroyWindow(hwnd); return 0; }
        break;
    case WM_LBUTTONDOWN:
        g_pick_x = (short)LOWORD(lp);
        g_pick_y = (short)HIWORD(lp);
        g_pick_req = 1;
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static void window_init(int w, int h) {
    WNDCLASSA wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = kWinClass;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassA(&wc);
    RECT rc = {0, 0, w, h};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    g_hwnd = CreateWindowA(kWinClass, "Silsph Soft Renderer",
                           WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                           rc.right - rc.left, rc.bottom - rc.top,
                           NULL, NULL, wc.hInstance, NULL);
    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);
    /* DIB section：帧缓冲内存序 B,G,R,A 与 32bpp BI_RGB 完全匹配，零转换直接 blit */
    memset(&g_bmi, 0, sizeof(g_bmi));
    g_bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    g_bmi.bmiHeader.biWidth = w;
    g_bmi.bmiHeader.biHeight = -h;   /* top-down：帧缓冲行 0 = 窗口顶行 */
    g_bmi.bmiHeader.biPlanes = 1;
    g_bmi.bmiHeader.biBitCount = 32;
    g_bmi.bmiHeader.biCompression = BI_RGB;
    g_dib = CreateDIBSection(NULL, &g_bmi, DIB_RGB_COLORS, &g_dib_bits, NULL, 0);
    HDC hdc = GetDC(g_hwnd);
    g_memdc = CreateCompatibleDC(hdc);
    ReleaseDC(g_hwnd, hdc);
    SelectObject(g_memdc, g_dib);
}

static void window_blit(int w, int h) {
    memcpy(g_dib_bits, sp_pixels(NULL, NULL), (size_t)w * h * 4);
    HDC hdc = GetDC(g_hwnd);
    BitBlt(hdc, 0, 0, w, h, g_memdc, 0, 0, SRCCOPY);
    ReleaseDC(g_hwnd, hdc);
}

static void window_mode(void) {
    const int W = 960, H = 640;
    if (!sp_create(W, H)) { printf("sp_create failed\n"); return; }
    sp_viewport(0, 0, W, H);
    sp_clear_color(0.063f, 0.075f, 0.102f, 1.0f);
    window_init(W, H);

    double t0 = now_ms();
    double angle = 0.0;
    int frames = 0;
    double fps_t0 = t0;
    MSG msg;
    printf("Silsph Soft Renderer 窗口已打开（ESC/关闭退出）\n");
    while (g_running) {
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (!g_running) break;
        double now = now_ms();
        angle += (now - t0) / 1000.0 * 1.2;   /* 与 GL demo 同速 */
        t0 = now;
        if (g_pick_req) {   /* 点击拾取：ID 帧渲染后读回 */
            g_pick_req = 0;
            sp_clear(SP_COLOR | SP_DEPTH | SP_ID);
            g_pick_mode = 1;
            draw_scene(W, H, (float)angle);
            g_pick_mode = 0;
            int id = sp_pick_id(g_pick_x, g_pick_y);
            g_selected = id;
            printf("拾取 (%d,%d) -> ID %d (%s) %s\n", g_pick_x, g_pick_y, id,
                   id == 1 ? "立方体" : (id == 2 ? "半透明板" : "无"),
                   id ? "已选中" : "取消选中");
        }
        sp_clear(SP_COLOR | SP_DEPTH);
        draw_scene(W, H, (float)angle);
        window_blit(W, H);
        frames++;
        if (now - fps_t0 >= 500.0) {
            double fps = frames * 1000.0 / (now - fps_t0);
            char buf[160];
            sprintf(buf, "Silsph Soft Renderer - %dx%d | %.1f FPS", W, H, fps);
            SetWindowTextA(g_hwnd, buf);
            FILE* fp = fopen("silsph_fps.txt", "w");
            if (fp) { fprintf(fp, "%.1f", fps); fclose(fp); }
            frames = 0; fps_t0 = now;
        }
    }
    DeleteObject(g_dib);
    DeleteDC(g_memdc);
    sp_destroy();
    printf("silsph soft demo exit\n");
}
#endif /* _WIN32 */

/* ================= 控制台模式 ================= */
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
        else next = now_ms() + frame_ms;
    }
    return (double)frames * 1000.0 / (now_ms() - t0);
}

/* --info：硬件信息 + 帧率/画质矩阵 + 帧限速 */
static void console_info(void) {
    if (!sp_create(960, 640)) { printf("sp_create failed\n"); return; }
    sp_viewport(0, 0, 960, 640);
    sp_clear_color(0.063f, 0.075f, 0.102f, 1.0f);

    sp_sysinfo si;
    if (sp_get_sysinfo(&si)) {
        printf("==== 硬件信息（sp_get_sysinfo，纯系统 API，零依赖）====\n");
        printf("CPU : %s\n", si.cpu_name);
        printf("       %d 核心 / %d 线程, %d MHz\n", si.cpu_cores, si.cpu_threads, si.cpu_mhz);
        printf("内存: %llu MB (可用 %llu MB)\n", si.mem_total_mb, si.mem_avail_mb);
        if (si.gpu_vram_mb > 0)
            printf("GPU : %s (%llu MB 显存)\n", si.gpu_name, si.gpu_vram_mb);
        else
            printf("GPU : %s (共享显存/未知)\n", si.gpu_name);
        printf("OS  : Windows %d.%d (build %d)\n", si.os_major, si.os_minor, si.os_build);
    }

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

    printf("\n==== 画质开关真实价值（300 随机三角形重度重叠，各 500 帧）====\n");
    printf("%-26s %9s %8s\n", "配置", "ms/帧", "FPS");
    struct { int depth, cull; const char* name; } tc[] = {
        {1, 1, "深度+剔除开(默认)"},
        {0, 1, "深度关"},
        {1, 0, "剔除关"},
        {0, 0, "全关"},
    };
    gen_tris();
    sp_matrix_mode(SP_PROJECTION); sp_load_identity();
    sp_perspective(55.0f, 960.0f/640.0f, 0.1f, 100.0f);
    sp_matrix_mode(SP_MODELVIEW); sp_load_identity();
    sp_look_at(0, 0, 6, 0, 0, 0, 0, 1, 0);
    sp_color3f(1.0f, 0.55f, 0.35f);
    for (int i = 0; i < 4; i++) {
        double ms = bench_tris(tc[i].depth, tc[i].cull, 500);
        printf("%-26s %9.3f %8.0f\n", tc[i].name, ms, 1000.0 / ms);
    }

    printf("\n==== 帧率控制（限速器实测 1 秒）====\n");
    printf("目标 60 FPS -> 实际 %.2f FPS\n", frame_limit_bench(60.0, 1));
    printf("目标 30 FPS -> 实际 %.2f FPS\n", frame_limit_bench(30.0, 1));

    sp_destroy();
}

/* --frames：6 帧 BMP 画质证据 */
static void console_frames(void) {
    if (!sp_create(960, 640)) { printf("sp_create failed\n"); return; }
    sp_viewport(0, 0, 960, 640);
    sp_clear_color(0.063f, 0.075f, 0.102f, 1.0f);
    for (int i = 0; i < 6; i++) {
        char name[32];
        sprintf(name, "frame%d.bmp", i);
        render_frame(960, 640, (float)i, name);
    }
    sp_destroy();
}

int main(int argc, char** argv) {
    console_utf8();
    int mode = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--info") == 0) mode = 1;
        else if (strcmp(argv[i], "--frames") == 0) mode = 2;
    }
    if (mode == 1) { console_info(); return 0; }
    if (mode == 2) { console_frames(); return 0; }
#ifdef _WIN32
    window_mode();   /* 默认：实时动画窗口 */
#else
    console_info();  /* 非 Windows 回退控制台模式 */
#endif
    return 0;
}
