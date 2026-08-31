// (C) SilentStudio — All Rights Reserved.
// Proprietary license: 未经 SilentStudio 书面许可，禁止复制、分发、修改或使用。
// silsph_res.h — 资源加载（零第三方依赖）：OBJ 模型 / BMP 纹理
#ifndef SILSPH_RES_H
#define SILSPH_RES_H

#ifdef _WIN32
#  if defined(SP_BUILD_DLL)
#    define SP_RES_API __declspec(dllexport)
#  else
#    define SP_RES_API __declspec(dllimport)
#  endif
#else
#  define SP_RES_API
#endif

/* 网格：展开式顶点（无索引共享），每顶点 8 float：x,y,z, nx,ny,nz, u,v */
typedef struct sp_mesh {
    int      vcount;    /* 顶点数（= 三角形数 x 3） */
    float*   verts;     /* vcount x 8 */
    unsigned* idx;      /* vcount 个顺序索引（保留接口，当前 0..vcount-1） */
    char     mtl[64];   /* 第一个 usemtl 名（无则空） */
} sp_mesh;

#ifdef __cplusplus
extern "C" {
#endif

/* Wavefront OBJ：支持 v/vt/vn/f（v、v/vt、v//vn、v/vt/vn），四边形拆三角形。
   返回 NULL 表示失败（stderr 打印原因）。 */
SP_RES_API sp_mesh* sp_load_obj(const char* path);
SP_RES_API void     sp_free_mesh(sp_mesh* m);

/* BMP 纹理：24/32bpp BI_RGB（bottom-up/top-down）→ 纹理 ID（sp_gen_texture），失败返回 0 */
SP_RES_API int      sp_load_texture_bmp(const char* path);
/* PNG 纹理：8-bit / 灰度/RGB/灰度+alpha/RGBA / 无交织（手写 inflate，零依赖） */
SP_RES_API int      sp_load_texture_png(const char* path);

#ifdef __cplusplus
}
#endif
#endif
