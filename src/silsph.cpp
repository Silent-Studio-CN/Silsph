// silsph.cpp — Silsph Engine v0.1.0 实现
#include "silsph.h"
#include <cmath>
#include <cstring>
#include <iostream>

namespace silsph {

using std::cout;
using std::endl;

// ================= Mat4 =================
Mat4 Mat4::identity() {
    Mat4 r{}; r.m[0]=1; r.m[5]=1; r.m[10]=1; r.m[15]=1; return r;
}
Mat4 Mat4::perspective(float fov, float aspect, float zn, float zf) {
    Mat4 r{};
    float t = 1.0f / std::tan(fov * 0.5f * 3.14159265f / 180.0f);
    r.m[0] = t / aspect; r.m[5] = t;
    // 行主序：row2 = (0,0,(f+n)/(n-f), 2fn/(n-f))，row3 = (0,0,-1,0)
    r.m[10] = (zf + zn) / (zn - zf);
    r.m[11] = (2 * zf * zn) / (zn - zf);
    r.m[14] = -1;
    return r;
}
Mat4 Mat4::rotationX(float a) {
    Mat4 r = identity();
    float c = std::cos(a), s = std::sin(a);
    r.m[5] = c; r.m[6] = -s; r.m[9] = s; r.m[10] = c;
    return r;
}
Mat4 Mat4::rotationY(float a) {
    Mat4 r = identity();
    float c = std::cos(a), s = std::sin(a);
    r.m[0] = c; r.m[2] = -s; r.m[8] = s; r.m[10] = c;
    return r;
}
Mat4 Mat4::rotationZ(float a) {
    Mat4 r = identity();
    float c = std::cos(a), s = std::sin(a);
    r.m[0] = c; r.m[1] = -s; r.m[4] = s; r.m[5] = c;
    return r;
}
Mat4 Mat4::lookAt(float ex, float ey, float ez, float cx, float cy, float cz,
                  float ux, float uy, float uz) {
    // 行主序视图矩阵
    float fx = cx - ex, fy = cy - ey, fz = cz - ez;
    float fl = std::sqrt(fx*fx + fy*fy + fz*fz);
    fx /= fl; fy /= fl; fz /= fl;
    // s = f x up
    float sx = fy*uz - fz*uy, sy = fz*ux - fx*uz, sz = fx*uy - fy*ux;
    float sl = std::sqrt(sx*sx + sy*sy + sz*sz);
    sx /= sl; sy /= sl; sz /= sl;
    // u = s x f
    float ux2 = sy*fz - sz*fy, uy2 = sz*fx - sx*fz, uz2 = sx*fy - sy*fx;
    Mat4 r = identity();
    r.m[0] = sx; r.m[1] = ux2; r.m[2] = -fx;
    r.m[4] = sy; r.m[5] = uy2; r.m[6] = -fy;
    r.m[8] = sz; r.m[9] = uz2; r.m[10] = -fz;
    r.m[12] = -(sx*ex + sy*ey + sz*ez);
    r.m[13] = -(ux2*ex + uy2*ey + uz2*ez);
    r.m[14] = fx*ex + fy*ey + fz*ez;
    return r;
}

Mat4 Mat4::operator*(const Mat4& o) const {
    Mat4 r{};
    for (int row = 0; row < 4; row++)
        for (int col = 0; col < 4; col++) {
            float sum = 0;
            for (int k = 0; k < 4; k++) sum += m[row*4+k] * o.m[k*4+col];
            r.m[row*4+col] = sum;
        }
    return r;
}

// ================= Window =================
LRESULT CALLBACK Window::wndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    Window* self = (Window*)GetWindowLongPtr(h, GWLP_USERDATA);
    switch (msg) {
        case WM_DESTROY: if (self) self->close_ = true; return 0;
        case WM_ERASEBKGND: return 1;
        case WM_SIZE: return 0;
    }
    return DefWindowProc(h, msg, wp, lp);
}

bool Window::create(int w, int h, const char* title) {
    w_ = w; h_ = h;
    WNDCLASS wc{};
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = wndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = "SilsphWindow";
    RegisterClass(&wc);

    RECT rc = {0, 0, w, h};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    hwnd_ = CreateWindow("SilsphWindow", title, WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, wc.hInstance, nullptr);
    if (!hwnd_) { cout << "CreateWindow failed" << endl; return false; }
    SetWindowLongPtr(hwnd_, GWLP_USERDATA, (LONG_PTR)this);

    dc_ = GetDC(hwnd_);
    glrc_ = glInit(hwnd_, dc_);
    return glrc_ != nullptr;
}

Window::~Window() {
    if (glrc_) { wglMakeCurrent(nullptr, nullptr); wglDeleteContext(glrc_); }
    if (dc_ && hwnd_) ReleaseDC(hwnd_, dc_);
    if (hwnd_) DestroyWindow(hwnd_);
}

void Window::swap() {
    BOOL ok = SwapBuffers(dc_);
    if (!ok) { cout << "SwapBuffers failed GLE=" << GetLastError() << endl; }
}
bool Window::poll() {
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return !close_;
}

// ================= GL 加载（GL 3.3 Core，全部函数运行时加载） =================
typedef HGLRC (WINAPI* PFNWGLCREATECONTEXTATTRIBS)(HDC, HGLRC, const int*);
PFNWGLCREATECONTEXTATTRIBS s_wglCreateContextAttribs;

PFNGLVIEWPORTPROC sglViewport;
PFNGLCLEARPROC sglClear;
PFNGLCLEARCOLORPROC sglClearColor;
PFNGLENABLEPROC sglEnable;
PFNGLDISABLEPROC sglDisable;
PFNGLDEPTHFUNCPROC sglDepthFunc;
PFNGLDRAWELEMENTSPROC sglDrawElements;
PFNGLGETSTRINGPROC sglGetString;
PFNGLGETERRORPROC sglGetError;
PFNGLGETINTEGERVPROC sglGetIntegerv;
PFNGLCREATESHADERPROC sglCreateShader;
PFNGLSHADERSOURCEPROC sglShaderSource;
PFNGLCOMPILESHADERPROC sglCompileShader;
PFNGLGETSHADERIVPROC sglGetShaderiv;
PFNGLGETSHADERINFOLOGPROC sglGetShaderInfoLog;
PFNGLCREATEPROGRAMPROC sglCreateProgram;
PFNGLATTACHSHADERPROC sglAttachShader;
PFNGLLINKPROGRAMPROC sglLinkProgram;
PFNGLGETPROGRAMIVPROC sglGetProgramiv;
PFNGLGETPROGRAMINFOLOGPROC sglGetProgramInfoLog;
PFNGLUSEPROGRAMPROC sglUseProgram;
PFNGLGENVERTEXARRAYSPROC sglGenVertexArrays;
PFNGLBINDVERTEXARRAYPROC sglBindVertexArray;
PFNGLGENBUFFERSPROC sglGenBuffers;
PFNGLBINDBUFFERPROC sglBindBuffer;
PFNGLBUFFERDATAPROC sglBufferData;
PFNGLVERTEXATTRIBPOINTERPROC sglVertexAttribPointer;
PFNGLENABLEVERTEXATTRIBARRAYPROC sglEnableVertexAttribArray;
PFNGLGETUNIFORMLOCATIONPROC sglGetUniformLocation;
PFNGLUNIFORMMATRIX4FVPROC sglUniformMatrix4fv;
PFNGLUNIFORM3FVPROC sglUniform3fv;
PFNGLDELETESHADERPROC sglDeleteShader;
PFNGLDELETEPROGRAMPROC sglDeleteProgram;
PFNGLDELETEBUFFERSPROC sglDeleteBuffers;
PFNGLDELETEVERTEXARRAYSPROC sglDeleteVertexArrays;

static void* getGLProc(const char* name) {
    void* p = (void*)wglGetProcAddress(name);
    if (!p) p = (void*)GetProcAddress(GetModuleHandleA("opengl32.dll"), name);
    return p;
}

#define LOAD(var, api) *(void**)&var = getGLProc(#api); if (!var) { cout << "GL load fail: " << #api << endl; return nullptr; }

HGLRC glInit(HWND hwnd, HDC dc) {
    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24;
    pfd.cDepthBits = 24;
    int pf = ChoosePixelFormat(dc, &pfd);
    SetPixelFormat(dc, pf, &pfd);
    HGLRC tmp = wglCreateContext(dc);
    wglMakeCurrent(dc, tmp);
    PFNWGLCREATECONTEXTATTRIBS create = (PFNWGLCREATECONTEXTATTRIBS)
        wglGetProcAddress("wglCreateContextAttribsARB");
    if (!create) {
        cout << "GL load fail: wglCreateContextAttribsARB" << endl;
        return nullptr;
    }
    int attribs[] = { WGL_CONTEXT_MAJOR_VERSION_ARB, 3, WGL_CONTEXT_MINOR_VERSION_ARB, 3,
                      WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB, 0 };
    SetLastError(0);
    HGLRC gl33 = create(dc, nullptr, attribs);
    if (!gl33) {
        cout << "GL 3.3 core failed, GLE=" << GetLastError() << endl;
        return nullptr;
    }
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(tmp);
    wglMakeCurrent(dc, gl33);
    (void)hwnd;

    LOAD(sglViewport, glViewport) LOAD(sglClear, glClear) LOAD(sglClearColor, glClearColor)
    LOAD(sglEnable, glEnable) LOAD(sglDisable, glDisable) LOAD(sglDepthFunc, glDepthFunc)
    LOAD(sglDrawElements, glDrawElements) LOAD(sglGetString, glGetString)
    LOAD(sglGetError, glGetError) LOAD(sglGetIntegerv, glGetIntegerv)
    LOAD(sglCreateShader, glCreateShader) LOAD(sglShaderSource, glShaderSource)
    LOAD(sglCompileShader, glCompileShader) LOAD(sglGetShaderiv, glGetShaderiv)
    LOAD(sglGetShaderInfoLog, glGetShaderInfoLog) LOAD(sglCreateProgram, glCreateProgram)
    LOAD(sglAttachShader, glAttachShader) LOAD(sglLinkProgram, glLinkProgram)
    LOAD(sglGetProgramiv, glGetProgramiv) LOAD(sglGetProgramInfoLog, glGetProgramInfoLog)
    LOAD(sglUseProgram, glUseProgram) LOAD(sglGenVertexArrays, glGenVertexArrays)
    LOAD(sglBindVertexArray, glBindVertexArray) LOAD(sglGenBuffers, glGenBuffers)
    LOAD(sglBindBuffer, glBindBuffer) LOAD(sglBufferData, glBufferData)
    LOAD(sglVertexAttribPointer, glVertexAttribPointer) LOAD(sglEnableVertexAttribArray, glEnableVertexAttribArray)
    LOAD(sglGetUniformLocation, glGetUniformLocation) LOAD(sglUniformMatrix4fv, glUniformMatrix4fv)
    LOAD(sglUniform3fv, glUniform3fv)
    LOAD(sglDeleteShader, glDeleteShader) LOAD(sglDeleteProgram, glDeleteProgram)
    LOAD(sglDeleteBuffers, glDeleteBuffers) LOAD(sglDeleteVertexArrays, glDeleteVertexArrays)
    cout << "all GL funcs loaded" << endl;
    // 垂直同步（可选扩展）
    typedef BOOL (WINAPI* PFNWGLSWAPINTERVALEXT)(int);
    PFNWGLSWAPINTERVALEXT swapInterval = (PFNWGLSWAPINTERVALEXT)
        wglGetProcAddress("wglSwapIntervalEXT");
    if (swapInterval) swapInterval(1);
    return gl33;
}

const char* glVendor()   { return (const char*)sglGetString(0x1F00); }
const char* glRenderer() { return (const char*)sglGetString(0x1F01); }
const char* glVersion()  { return (const char*)sglGetString(0x1F02); }

// ================= Shader =================
static bool checkShader(unsigned id, bool isProg) {
    int ok = 0;
    if (isProg) sglGetProgramiv(id, 0x8B82 /*LINK_STATUS*/, &ok);
    else sglGetShaderiv(id, 0x8B81 /*COMPILE_STATUS*/, &ok);
    if (ok) return true;
    char log[1024] = {};
    if (isProg) sglGetProgramInfoLog(id, 1024, nullptr, log);
    else sglGetShaderInfoLog(id, 1024, nullptr, log);
    cout << "shader error: " << log << endl;
    return false;
}

bool Shader::build(const char* vsSrc, const char* fsSrc) {
    unsigned vs = sglCreateShader(0x8B31 /*VERTEX*/);
    const GLchar* vsArr[] = { vsSrc };
    sglShaderSource(vs, 1, vsArr, nullptr);
    sglCompileShader(vs);
    if (!checkShader(vs, false)) return false;
    unsigned fs = sglCreateShader(0x8B30 /*FRAGMENT*/);
    const GLchar* fsArr[] = { fsSrc };
    sglShaderSource(fs, 1, fsArr, nullptr);
    sglCompileShader(fs);
    if (!checkShader(fs, false)) { sglDeleteShader(vs); return false; }
    prog_ = sglCreateProgram();
    sglAttachShader(prog_, vs);
    sglAttachShader(prog_, fs);
    sglLinkProgram(prog_);
    if (!checkShader(prog_, true)) { sglDeleteProgram(prog_); prog_ = 0; }
    sglDeleteShader(vs);
    sglDeleteShader(fs);
    return ok();
}
Shader::~Shader() { if (prog_) sglDeleteProgram(prog_); }
void Shader::use() const { sglUseProgram(prog_); }
int Shader::uniform(const char* n) const { return sglGetUniformLocation(prog_, n); }

// ================= Mesh =================
void Mesh::upload(const std::vector<Vertex>& verts, const std::vector<unsigned>& idx) {
    count_ = (unsigned)idx.size();
    sglGenVertexArrays(1, &vao_);
    sglBindVertexArray(vao_);
    sglGenBuffers(1, &vbo_);
    sglBindBuffer(0x8892 /*ARRAY*/, vbo_);
    sglBufferData(0x8892, verts.size() * sizeof(Vertex), verts.data(), 0x88E4 /*STATIC*/);
    sglGenBuffers(1, &ibo_);
    sglBindBuffer(0x8893 /*ELEMENT*/, ibo_);
    sglBufferData(0x8893, idx.size() * sizeof(unsigned), idx.data(), 0x88E4);
    sglVertexAttribPointer(0, 3, 0x1406 /*FLOAT*/, 0, sizeof(Vertex), (void*)0);
    sglEnableVertexAttribArray(0);
    sglVertexAttribPointer(1, 3, 0x1406, 0, sizeof(Vertex), (void*)12);
    sglEnableVertexAttribArray(1);
    sglVertexAttribPointer(2, 3, 0x1406, 0, sizeof(Vertex), (void*)24);
    sglEnableVertexAttribArray(2);
    sglBindVertexArray(0);
}
void Mesh::draw(unsigned mode) const {
    sglBindVertexArray(vao_);
    sglDrawElements(mode, count_, 0x1405 /*UNSIGNED_INT*/, nullptr);
    sglBindVertexArray(0);
}
Mesh::~Mesh() {
    if (ibo_) sglDeleteBuffers(1, &ibo_);
    if (vbo_) sglDeleteBuffers(1, &vbo_);
    if (vao_) sglDeleteVertexArrays(1, &vao_);
}

} // namespace silsph
