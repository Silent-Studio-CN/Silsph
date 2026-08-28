// silsph_soft.h — Silsph 软件渲染管线（自研 OPG/VK 同品的第一步）
// 即时模式 API（迷你 OpenGL 风格）：帧缓冲 → 矩阵 → 图元 → 光栅化
#ifndef SILSPH_SOFT_H
#define SILSPH_SOFT_H

#ifdef _WIN32
#  if defined(SP_BUILD_DLL)
#    define SP_API __declspec(dllexport)
#  else
#    define SP_API __declspec(dllimport)
#  endif
#else
#  define SP_API
#endif

#include <stdint.h>

/* ---- 清除标志 ---- */
#define SP_COLOR 0x4000
#define SP_DEPTH 0x0100

/* ---- 图元模式 ---- */
#define SP_TRIANGLES 0x0004
#define SP_LINES     0x0001

/* ---- 矩阵模式 ---- */
#define SP_MODELVIEW  0x1700
#define SP_PROJECTION 0x1701

#ifdef __cplusplus
extern "C" {
#endif

/* ---- 帧缓冲 ---- */
SP_API int  sp_create(int width, int height);            // 分配颜色/深度缓冲，成功返回 1
SP_API void sp_destroy(void);
SP_API void sp_viewport(int x, int y, int w, int h);
SP_API void sp_clear_color(float r, float g, float b, float a);
SP_API void sp_clear(unsigned flags);
SP_API const unsigned char* sp_pixels(int* w, int* h);   // 颜色缓冲，内存序 B,G,R,A

/* ---- 矩阵（行主序，与 silsph.cpp 一致） ---- */
SP_API void sp_matrix_mode(int mode);
SP_API void sp_load_identity(void);
SP_API void sp_perspective(float fovy_deg, float aspect, float znear, float zfar);
SP_API void sp_look_at(float ex, float ey, float ez,
                       float cx, float cy, float cz,
                       float ux, float uy, float uz);
SP_API void sp_rotate(float deg, float ax, float ay, float az); // 当前矩阵 *= 旋转

/* ---- 画质控制 ---- */
SP_API void sp_cull_face(int enable);   /* 背面剔除（默认开，1=开） */
SP_API void sp_depth_test(int enable);  /* 深度测试与写入（默认开，1=开） */

/* ---- 平台工具（跨平台：QPC / clock_gettime / nanosleep） ---- */
SP_API double sp_now_ms(void);
SP_API void   sp_sleep_ms(double ms);

/* ---- 硬件信息（纯系统 API，零第三方依赖） ---- */
typedef struct sp_sysinfo {
    char     cpu_name[96];
    int      cpu_cores;      /* 物理核心数 */
    int      cpu_threads;    /* 逻辑处理器数 */
    int      cpu_mhz;        /* 标称主频 */
    unsigned long long mem_total_mb;
    unsigned long long mem_avail_mb;
    char     gpu_name[128];  /* 主显示适配器（注册表） */
    unsigned long long gpu_vram_mb;
    int      os_major, os_minor, os_build; /* RtlGetVersion */
} sp_sysinfo;
SP_API int sp_get_sysinfo(sp_sysinfo* out);

/* ---- 图元（即时模式） ---- */
SP_API void sp_begin(int mode);
SP_API void sp_color3f(float r, float g, float b);
SP_API void sp_vertex3f(float x, float y, float z);
SP_API void sp_end(void);

#ifdef __cplusplus
}
#endif
#endif /* SILSPH_SOFT_H */
