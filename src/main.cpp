// main.cpp — Silsph Engine demo：旋转彩色立方体 (Qraft 主题色)
#include "silsph.h"
#include <cmath>
#include <cstdio>
#include <vector>

using namespace silsph;

static const char* VS =
    "#version 330 core\n"
    "layout(location=0) in vec3 aPos;\n"
    "layout(location=1) in vec3 aColor;\n"
    "layout(location=2) in vec3 aNormal;\n"
    "uniform mat4 uMVP;\n"
    "uniform mat4 uModel;\n"
    "uniform vec3 uLightDir;\n"
    "out vec3 vColor;\n"
    "out vec3 vNormal;\n"
    "out vec3 vLightDir;\n"
    "void main() {\n"
    "    gl_Position = vec4(aPos, 1.0) * uMVP;\n"
    "    vColor = aColor;\n"
    "    vNormal = mat3(transpose(uModel)) * aNormal;\n"
    "    vLightDir = uLightDir;\n"
    "}\n";

static const char* FS =
    "#version 330 core\n"
    "in vec3 vColor;\n"
    "in vec3 vNormal;\n"
    "in vec3 vLightDir;\n"
    "out vec4 frag;\n"
    "void main() {\n"
    "    vec3 n = normalize(vNormal);\n"
    "    vec3 l = normalize(vLightDir);\n"
    "    float diff = max(dot(n, -l), 0.0);\n"
    "    float light = 0.22 + 0.78 * diff;\n"
    "    frag = vec4(vColor * light, 1.0);\n"
    "}\n";

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    // 日志输出到文件（后台环境无交互控制台）
    FILE* f; freopen_s(&f, "silsph.log", "w", stdout);
    FILE* fe; freopen_s(&fe, "silsph.log", "a", stderr);

    Window win;
    if (!win.create(960, 640, "Silsph v0.1.0 — OpenGL 3.3 Core (C++)")) {
        printf("window create failed\n");
        return 1;
    }
    printf("GL vendor  : %s\n", glVendor());
    printf("GL renderer: %s\n", glRenderer());
    printf("GL version : %s\n", glVersion());

    // 写窗口句柄（供宿主嵌入）
    HWND selfHwnd = GetActiveWindow();
    if (!selfHwnd) selfHwnd = FindWindowA("SilsphWindow", NULL);
    if (selfHwnd) {
        FILE* hf = fopen("silsph_hwnd.txt", "w");
        if (hf) { fprintf(hf, "%llu", (unsigned long long)(uintptr_t)selfHwnd); fclose(hf); }
    }

    printf("shader build start\n");
    fflush(stdout);
    Shader shader;
    if (!shader.build(VS, FS)) { printf("shader failed\n"); return 1; }
    printf("shader build ok\n");
    fflush(stdout);
    int uMVP = shader.uniform("uMVP");
    int uModel = shader.uniform("uModel");
    int uLightDir = shader.uniform("uLightDir");
    printf("uniforms uMVP=%d uModel=%d uLightDir=%d\n", uMVP, uModel, uLightDir);
    fflush(stdout);

    // 立方体：6 面 x 2 三角形 = 24 顶点（每面独立颜色）
    // Qraft 主题色：青 #4FFFFD(0.31,1,0.99) 橙 #FE9D3F(1,0.62,0.25)
    struct Face { float n[3]; float c[3]; };
    const Face faces[6] = {
        {{0,0,-1},{1.0f,0.62f,0.25f}}, {{0,0,1},{0.31f,1.0f,0.99f}},
        {{-1,0,0},{0.44f,0.69f,1.0f}}, {{1,0,0},{1.0f,0.44f,0.69f}},
        {{0,-1,0},{0.61f,1.0f,0.44f}}, {{0,1,0},{1.0f,0.85f,0.44f}},
    };
    std::vector<Vertex> verts;
    std::vector<unsigned> idx;
    for (int i = 0; i < 6; i++) {
        const float* n = faces[i].n;
        // 由法线生成 4 个角点（简化：沿两个切线方向）
        float tx[3] = { n[1] ? 1.0f : 0.0f, n[0] ? 0.0f : 1.0f, 0.0f };
        if (n[2] != 0) { tx[0]=1; tx[1]=0; tx[2]=0; }
        float ty[3];
        ty[0] = n[1]*tx[2]-n[2]*tx[1]; ty[1] = n[2]*tx[0]-n[0]*tx[2]; ty[2] = n[0]*tx[1]-n[1]*tx[0];
        unsigned base = (unsigned)verts.size();
        for (int k = 0; k < 4; k++) {
            float sx = (k==1||k==2) ? 1 : -1, sy = (k==2||k==3) ? 1 : -1;
            Vertex v;
            v.x = n[0] + tx[0]*sx*1.0f + ty[0]*sy*1.0f;
            v.y = n[1] + tx[1]*sx*1.0f + ty[1]*sy*1.0f;
            v.z = n[2] + tx[2]*sx*1.0f + ty[2]*sy*1.0f;
            v.r = faces[i].c[0]; v.g = faces[i].c[1]; v.b = faces[i].c[2];
            v.nx = n[0]; v.ny = n[1]; v.nz = n[2]; // 面法线
            verts.push_back(v);
        }
        unsigned qu[4] = {base, base+1, base+2, base+3};
        // 逆时针绕序（GL 默认 CCW 为正面，配合背面剔除）
        idx.push_back(qu[0]); idx.push_back(qu[2]); idx.push_back(qu[1]);
        idx.push_back(qu[0]); idx.push_back(qu[3]); idx.push_back(qu[2]);
    }
    Mesh cube;
    cube.upload(verts, idx);

    // 地面网格（GL_LINES，灰色）
    std::vector<Vertex> gv;
    std::vector<unsigned> gi;
    const float H = 0.0f, S = 8.0f, STEP = 1.0f;
    unsigned n = 0;
    for (float x = -S; x <= S; x += STEP) {
        Vertex a; a.x = x; a.y = H; a.z = -S; a.r = 0.35f; a.g = 0.40f; a.b = 0.55f; a.nx = 0; a.ny = 1; a.nz = 0;
        Vertex b; b.x = x; b.y = H; b.z = S;  b.r = 0.35f; b.g = 0.40f; b.b = 0.55f; b.nx = 0; b.ny = 1; b.nz = 0;
        gv.push_back(a); gv.push_back(b); gi.push_back(n++); gi.push_back(n++);
    }
    for (float z = -S; z <= S; z += STEP) {
        Vertex a; a.x = -S; a.y = H; a.z = z; a.r = 0.35f; a.g = 0.40f; a.b = 0.55f; a.nx = 0; a.ny = 1; a.nz = 0;
        Vertex b; b.x = S;  b.y = H; b.z = z; b.r = 0.35f; b.g = 0.40f; b.b = 0.55f; b.nx = 0; b.ny = 1; b.nz = 0;
        gv.push_back(a); gv.push_back(b); gi.push_back(n++); gi.push_back(n++);
    }
    Mesh grid;
    grid.upload(gv, gi);

    glEnable(0x0B71 /*DEPTH_TEST*/);
    glDepthFunc(0x0201 /*LESS*/);
    glEnable(0x0B44 /*CULL_FACE*/); // 背面剔除（立方体面绕序正确）
    glClearColor(0.063f, 0.075f, 0.102f, 1.0f); // #10131A

    LARGE_INTEGER freq, t0, t1;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t0);
    float angle = 0;
    unsigned frames = 0;

    while (win.poll()) {
        QueryPerformanceCounter(&t1);
        double dt = (double)(t1.QuadPart - t0.QuadPart) / freq.QuadPart;
        t0 = t1;
        angle += (float)(dt * 1.2);

        glViewport(0, 0, win.width(), win.height());
        glClear(0x4000 /*COLOR*/ | 0x100 /*DEPTH*/);

        Mat4 proj = Mat4::perspective(55, (float)win.width()/win.height(), 0.1f, 100.0f);
        // 轨道相机：绕 Y 缓慢旋转，lookAt 原点
        float camA = angle * 0.25f;
        // 轨道相机：半径 17、高度 8.5，立方体占屏约 12%
        Mat4 view = Mat4::lookAt(17.0f * std::sin(camA), 8.5f, 17.0f * std::cos(camA),
                                 0, 0, 0,  0, 1, 0);
        Mat4 model = Mat4::rotationY(angle) * Mat4::rotationX(angle * 0.6f);
        Mat4 mvp = proj * view * model;

        shader.use();
        glUniformMatrix4fv(uMVP, 1, 0, mvp.m);
        glUniformMatrix4fv(uModel, 1, 0, model.m);
        float ldir[3] = { -0.5f, -0.8f, -0.3f };
        glUniform3fv(uLightDir, 1, ldir);

        cube.draw(); // 立方体（光照）

        // 地面网格（GL_LINES）
        glUniformMatrix4fv(uMVP, 1, 0, (proj * view).m);
        Mat4 identM = Mat4::identity();
        glUniformMatrix4fv(uModel, 1, 0, identM.m);
        grid.draw(0x0001 /*LINES*/);
        win.swap();

        frames++;
        if (frames % 30 == 0) {
            char buf[128];
            double fps = frames / dt;
            snprintf(buf, sizeof(buf), "Silsph v0.1.0 - %dx%d | %.1f FPS | GL %s",
                     win.width(), win.height(), fps, glVersion());
            SetWindowText(GetActiveWindow(), buf);
            FILE* fp = fopen("silsph_fps.txt", "w");
            if (fp) { fprintf(fp, "%.1f", fps); fclose(fp); }
            frames = 0;
        }
    }
    printf("silsph exit\n");
    return 0;
}
