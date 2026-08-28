// silsph.h — Silsph Engine v0.1.0 (Win32 + OpenGL 3.3 Core)
// 纯 C++ 最小 3D 引擎：窗口、渲染上下文、着色器、网格、矩阵
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>
#include <GL/wglext.h>
#include <cstddef>
#include <string>
#include <vector>

// w64devkit 精简 gl.h 缺少的类型
typedef char GLchar;
typedef ptrdiff_t GLsizeiptr;

// 屏蔽 gl.h 函数声明，统一走运行时加载的 sgl* 指针（GLFW 同款方案）
#define glViewport sglViewport
#define glClear sglClear
#define glClearColor sglClearColor
#define glEnable sglEnable
#define glDisable sglDisable
#define glDepthFunc sglDepthFunc
#define glDrawElements sglDrawElements
#define glGetString sglGetString
#define glGetError sglGetError
#define glGetIntegerv sglGetIntegerv
#define glCreateShader sglCreateShader
#define glShaderSource sglShaderSource
#define glCompileShader sglCompileShader
#define glGetShaderiv sglGetShaderiv
#define glGetShaderInfoLog sglGetShaderInfoLog
#define glCreateProgram sglCreateProgram
#define glAttachShader sglAttachShader
#define glLinkProgram sglLinkProgram
#define glGetProgramiv sglGetProgramiv
#define glGetProgramInfoLog sglGetProgramInfoLog
#define glUseProgram sglUseProgram
#define glGenVertexArrays sglGenVertexArrays
#define glBindVertexArray sglBindVertexArray
#define glGenBuffers sglGenBuffers
#define glBindBuffer sglBindBuffer
#define glBufferData sglBufferData
#define glVertexAttribPointer sglVertexAttribPointer
#define glEnableVertexAttribArray sglEnableVertexAttribArray
#define glGetUniformLocation sglGetUniformLocation
#define glUniformMatrix4fv sglUniformMatrix4fv
#define glUniform3fv sglUniform3fv
#define glDeleteShader sglDeleteShader
#define glDeleteProgram sglDeleteProgram
#define glDeleteBuffers sglDeleteBuffers
#define glDeleteVertexArrays sglDeleteVertexArrays

namespace silsph {

// ---------- 数学 ----------
struct Vec3 { float x, y, z; };
struct Mat4 {
    float m[16]; // 行主序
    static Mat4 identity();
    static Mat4 perspective(float fovDeg, float aspect, float znear, float zfar);
    static Mat4 rotationX(float rad);
    static Mat4 rotationY(float rad);
    static Mat4 rotationZ(float rad);
    static Mat4 lookAt(float ex, float ey, float ez, float cx, float cy, float cz, float ux, float uy, float uz);
    Mat4 operator*(const Mat4& o) const;
};

// ---------- 窗口 ----------
class Window {
public:
    bool create(int w, int h, const char* title);
    ~Window();
    void swap();
    bool poll();
    bool shouldClose() const { return close_; }
    int width() const { return w_; }
    int height() const { return h_; }
    HDC dc() const { return dc_; }
private:
    HWND hwnd_ = nullptr;
    HDC dc_ = nullptr;
    HGLRC glrc_ = nullptr;
    int w_ = 800, h_ = 600;
    bool close_ = false;
    static LRESULT CALLBACK wndProc(HWND, UINT, WPARAM, LPARAM);
};

// ---------- 渲染上下文 ----------
HGLRC glInit(HWND hwnd, HDC dc);
const char* glVendor();
const char* glRenderer();
const char* glVersion();

// GL 3.3 函数指针类型（w64devkit 的 gl.h 无 PFNGL typedef，自行定义）
typedef void (APIENTRY* PFNGLVIEWPORTPROC)(GLint, GLint, GLsizei, GLsizei);
typedef void (APIENTRY* PFNGLCLEARPROC)(GLbitfield);
typedef void (APIENTRY* PFNGLCLEARCOLORPROC)(GLfloat, GLfloat, GLfloat, GLfloat);
typedef GLuint (APIENTRY* PFNGLCREATESHADERPROC)(GLenum);
typedef void (APIENTRY* PFNGLSHADERSOURCEPROC)(GLuint, GLsizei, const GLchar* const*, const GLint*);
typedef void (APIENTRY* PFNGLCOMPILESHADERPROC)(GLuint);
typedef void (APIENTRY* PFNGLGETSHADERIVPROC)(GLuint, GLenum, GLint*);
typedef void (APIENTRY* PFNGLGETSHADERINFOLOGPROC)(GLuint, GLsizei, GLsizei*, GLchar*);
typedef GLuint (APIENTRY* PFNGLCREATEPROGRAMPROC)(void);
typedef void (APIENTRY* PFNGLATTACHSHADERPROC)(GLuint, GLuint);
typedef void (APIENTRY* PFNGLLINKPROGRAMPROC)(GLuint);
typedef void (APIENTRY* PFNGLGETPROGRAMIVPROC)(GLuint, GLenum, GLint*);
typedef void (APIENTRY* PFNGLGETPROGRAMINFOLOGPROC)(GLuint, GLsizei, GLsizei*, GLchar*);
typedef void (APIENTRY* PFNGLUSEPROGRAMPROC)(GLuint);
typedef void (APIENTRY* PFNGLGENVERTEXARRAYSPROC)(GLsizei, GLuint*);
typedef void (APIENTRY* PFNGLBINDVERTEXARRAYPROC)(GLuint);
typedef void (APIENTRY* PFNGLGENBUFFERSPROC)(GLsizei, GLuint*);
typedef void (APIENTRY* PFNGLBINDBUFFERPROC)(GLenum, GLuint);
typedef void (APIENTRY* PFNGLBUFFERDATAPROC)(GLenum, GLsizeiptr, const void*, GLenum);
typedef void (APIENTRY* PFNGLVERTEXATTRIBPOINTERPROC)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
typedef void (APIENTRY* PFNGLENABLEVERTEXATTRIBARRAYPROC)(GLuint);
typedef void (APIENTRY* PFNGLDRAWELEMENTSPROC)(GLenum, GLsizei, GLenum, const void*);
typedef GLint (APIENTRY* PFNGLGETUNIFORMLOCATIONPROC)(GLuint, const GLchar*);
typedef void (APIENTRY* PFNGLUNIFORMMATRIX4FVPROC)(GLint, GLsizei, GLboolean, const GLfloat*);
typedef void (APIENTRY* PFNGLUNIFORM3FVPROC)(GLint, GLsizei, const GLfloat*);
typedef void (APIENTRY* PFNGLENABLEPROC)(GLenum);
typedef void (APIENTRY* PFNGLDISABLEPROC)(GLenum);
typedef void (APIENTRY* PFNGLDEPTHFUNCPROC)(GLenum);
typedef GLenum (APIENTRY* PFNGLGETERRORPROC)(void);
typedef const GLubyte* (APIENTRY* PFNGLGETSTRINGPROC)(GLenum);
typedef void (APIENTRY* PFNGLGETINTEGERVPROC)(GLenum, GLint*);
typedef void (APIENTRY* PFNGLDELETESHADERPROC)(GLuint);
typedef void (APIENTRY* PFNGLDELETEPROGRAMPROC)(GLuint);
typedef void (APIENTRY* PFNGLDELETEBUFFERSPROC)(GLsizei, const GLuint*);
typedef void (APIENTRY* PFNGLDELETEVERTEXARRAYSPROC)(GLsizei, const GLuint*);

// 运行时加载的函数指针
extern PFNGLVIEWPORTPROC sglViewport;
extern PFNGLCLEARPROC sglClear;
extern PFNGLCLEARCOLORPROC sglClearColor;
extern PFNGLENABLEPROC sglEnable;
extern PFNGLDISABLEPROC sglDisable;
extern PFNGLDEPTHFUNCPROC sglDepthFunc;
extern PFNGLDRAWELEMENTSPROC sglDrawElements;
extern PFNGLGETSTRINGPROC sglGetString;
extern PFNGLGETERRORPROC sglGetError;
extern PFNGLGETINTEGERVPROC sglGetIntegerv;
extern PFNGLCREATESHADERPROC sglCreateShader;
extern PFNGLSHADERSOURCEPROC sglShaderSource;
extern PFNGLCOMPILESHADERPROC sglCompileShader;
extern PFNGLGETSHADERIVPROC sglGetShaderiv;
extern PFNGLGETSHADERINFOLOGPROC sglGetShaderInfoLog;
extern PFNGLCREATEPROGRAMPROC sglCreateProgram;
extern PFNGLATTACHSHADERPROC sglAttachShader;
extern PFNGLLINKPROGRAMPROC sglLinkProgram;
extern PFNGLGETPROGRAMIVPROC sglGetProgramiv;
extern PFNGLGETPROGRAMINFOLOGPROC sglGetProgramInfoLog;
extern PFNGLUSEPROGRAMPROC sglUseProgram;
extern PFNGLGENVERTEXARRAYSPROC sglGenVertexArrays;
extern PFNGLBINDVERTEXARRAYPROC sglBindVertexArray;
extern PFNGLGENBUFFERSPROC sglGenBuffers;
extern PFNGLBINDBUFFERPROC sglBindBuffer;
extern PFNGLBUFFERDATAPROC sglBufferData;
extern PFNGLVERTEXATTRIBPOINTERPROC sglVertexAttribPointer;
extern PFNGLENABLEVERTEXATTRIBARRAYPROC sglEnableVertexAttribArray;
extern PFNGLGETUNIFORMLOCATIONPROC sglGetUniformLocation;
extern PFNGLUNIFORMMATRIX4FVPROC sglUniformMatrix4fv;
extern PFNGLUNIFORM3FVPROC sglUniform3fv;
extern PFNGLDELETESHADERPROC sglDeleteShader;
extern PFNGLDELETEPROGRAMPROC sglDeleteProgram;
extern PFNGLDELETEBUFFERSPROC sglDeleteBuffers;
extern PFNGLDELETEVERTEXARRAYSPROC sglDeleteVertexArrays;

// ---------- 着色器 ----------
class Shader {
public:
    bool build(const char* vsSrc, const char* fsSrc);
    ~Shader();
    void use() const;
    int uniform(const char* name) const;
    bool ok() const { return prog_ != 0; }
private:
    unsigned prog_ = 0;
};

// ---------- 网格 ----------
struct Vertex { float x, y, z; float r, g, b; float nx, ny, nz; };
class Mesh {
public:
    void upload(const std::vector<Vertex>& verts, const std::vector<unsigned>& idx);
    void draw(unsigned mode = 0x0004 /*TRIANGLES*/) const;
    ~Mesh();
private:
    unsigned vao_ = 0, vbo_ = 0, ibo_ = 0;
    unsigned count_ = 0;
};

} // namespace silsph
