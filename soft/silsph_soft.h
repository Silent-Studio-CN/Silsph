// (C) SilentStudio — All Rights Reserved.
// Proprietary license: 未经 SilentStudio 书面许可，禁止复制、分发、修改或使用。
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
#define SP_ID    0x0200

/* ---- 图元模式（GL 风格值） ---- */
#define SP_POINTS         0x0000
#define SP_LINES          0x0001
#define SP_LINE_STRIP     0x0003
#define SP_TRIANGLES      0x0004
#define SP_TRIANGLE_STRIP 0x0005
#define SP_TRIANGLE_FAN   0x0006

/* ---- 矩阵模式 ---- */
#define SP_MODELVIEW  0x1700
#define SP_PROJECTION 0x1701

#ifdef __cplusplus
extern "C" {
#endif

/* ---- 帧缓冲 ---- */
SP_API int  sp_create(int width, int height);            // 分配颜色/深度缓冲，成功返回 1
SP_API void sp_destroy(void);
SP_API void sp_flush(void);                          // 回放已提交图元（多线程光栅化；sp_pixels/save 自动调用）
SP_API void sp_set_threads(int n);                   // 光栅化线程数：0=自动(默认)，1=串行

/* ---- 命令缓冲（统一 API：后端无关记录，软后端回放；未来 VK/GL 后端翻译同一命令流） ---- */
SP_API void sp_cmd_begin(void);                      // 开始记录（此后 sp_clear/sp_begin..sp_end 只记录不执行）
SP_API void sp_cmd_end(void);                        // 结束记录并按顺序回放全部命令
SP_API void sp_viewport(int x, int y, int w, int h);
SP_API void sp_clear_color(float r, float g, float b, float a);
SP_API void sp_clear(unsigned flags);
SP_API const unsigned char* sp_pixels(int* w, int* h);   // 颜色缓冲，内存序 B,G,R,A
SP_API int sp_save_bmp(const char* path);                   // 帧缓冲存 BMP（32bpp），成功 1

/* ---- 矩阵（行主序，与 silsph.cpp 一致） ---- */
SP_API void sp_matrix_mode(int mode);
SP_API void sp_load_identity(void);
SP_API void sp_perspective(float fovy_deg, float aspect, float znear, float zfar);
SP_API void sp_ortho(float left, float right, float bottom, float top, float znear, float zfar);
SP_API void sp_look_at(float ex, float ey, float ez,
                       float cx, float cy, float cz,
                       float ux, float uy, float uz);
SP_API void sp_rotate(float deg, float ax, float ay, float az); // 当前矩阵 *= 旋转
SP_API void sp_translate(float x, float y, float z);              // 当前矩阵 *= 平移
SP_API void sp_scale(float x, float y, float z);                  // 当前矩阵 *= 缩放
SP_API void sp_push_matrix(void);   /* 保存当前 MODELVIEW 到栈（深度上限 16） */
SP_API void sp_pop_matrix(void);    /* 恢复栈顶 */

/* ---- 纹理 ---- */
#define SP_TEX_NEAREST 0
#define SP_TEX_LINEAR  1
#define SP_TEX_REPEAT  0
#define SP_TEX_CLAMP   1
#define SP_MAX_TEX     16

SP_API int  sp_gen_texture(int w, int h, const unsigned char* rgba); /* 返回纹理 ID(1..16)，失败 0 */
SP_API void sp_delete_texture(int tex);
SP_API void sp_bind_texture(int tex);     /* 0 = 关闭纹理（纯色） */
SP_API void sp_texcoord2f(float u, float v);
SP_API void sp_tex_filter(int mode);      /* SP_TEX_NEAREST / SP_TEX_LINEAR */
SP_API void sp_tex_wrap(int mode);        /* SP_TEX_REPEAT / SP_TEX_CLAMP */

/* ---- 画质控制 ---- */
SP_API void sp_blend(int enable);      /* alpha 混合（src_alpha / 1-src_alpha，默认关） */

/* ---- 阴影贴图 ---- */
SP_API void sp_shadow_begin(int size);              /* 开始深度捕获：此后绘制只写光空间深度（串行） */
SP_API void sp_shadow_end(void);                   /* 结束捕获，生成阴影贴图 */
SP_API void sp_shadow_matrix(const float* m16);    /* 光空间 MVP（应用自行计算） */
SP_API void sp_shadow_enable(int on);              /* 主渲染时启用阴影（片元深度比较） */

/* ---- 拾取（ID 缓冲） ---- */
SP_API void sp_load_id(int id);       /* 当前图元拾取 ID（默认 0=无） */
SP_API int  sp_pick_id(int x, int y); /* 读 ID 缓冲 (x,y)，窗口坐标（y 向下），越界返回 0 */
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
