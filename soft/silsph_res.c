// (C) SilentStudio — All Rights Reserved.
// Proprietary license: 未经 SilentStudio 书面许可，禁止复制、分发、修改或使用。
// silsph_res.c — 资源加载实现（零第三方依赖）
#include "silsph_res.h"
#include "silsph_soft.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================= OBJ 解析（展开式顶点） ================= */
typedef struct { float x, y, z; } Rv3;
typedef struct { float u, v; }    Rv2;

typedef struct {
    Rv3* v;    int nv, cv;
    Rv2* vt;   int nvt, cvt;
    Rv3* vn;   int nvn, cvn;
    float* out; int no, co;
} Obj;

static int grow(void** p, size_t esz, int* cap, int need, int start) {
    if (need <= *cap) return 1;
    int nc = *cap ? *cap * 2 : start;
    if (nc < need) nc = need;
    void* np = realloc(*p, (size_t)nc * esz);
    if (!np) return 0;
    *p = np; *cap = nc;
    return 1;
}

static void obj_push_v(Obj* o, float x, float y, float z) {
    if (grow((void**)&o->v, sizeof(Rv3), &o->cv, o->nv + 1, 64)) {
        o->v[o->nv].x = x; o->v[o->nv].y = y; o->v[o->nv].z = z; o->nv++;
    }
}
static void obj_push_vt(Obj* o, float u, float v) {
    if (grow((void**)&o->vt, sizeof(Rv2), &o->cvt, o->nvt + 1, 64)) {
        o->vt[o->nvt].u = u; o->vt[o->nvt].v = v; o->nvt++;
    }
}
static void obj_push_vn(Obj* o, float x, float y, float z) {
    if (grow((void**)&o->vn, sizeof(Rv3), &o->cvn, o->nvn + 1, 64)) {
        o->vn[o->nvn].x = x; o->vn[o->nvn].y = y; o->vn[o->nvn].z = z; o->nvn++;
    }
}
/* 展开一个面顶点（vi/ti/ni 为 1-based 索引，越界给默认值） */
static void obj_emit(Obj* o, int vi, int ti, int ni) {
    float x = 0, y = 0, z = 0, u = 0, v = 0, nx = 0, ny = 1, nz = 0;
    if (vi > 0 && vi <= o->nv) { x = o->v[vi-1].x; y = o->v[vi-1].y; z = o->v[vi-1].z; }
    if (ti > 0 && ti <= o->nvt) { u = o->vt[ti-1].u; v = o->vt[ti-1].v; }
    if (ni > 0 && ni <= o->nvn) { nx = o->vn[ni-1].x; ny = o->vn[ni-1].y; nz = o->vn[ni-1].z; }
    if (grow((void**)&o->out, sizeof(float) * 8, &o->co, o->no + 8, 256)) {
        float* p = o->out + o->no;
        p[0]=x; p[1]=y; p[2]=z; p[3]=nx; p[4]=ny; p[5]=nz; p[6]=u; p[7]=v;
        o->no += 8;
    }
}

SP_RES_API sp_mesh* sp_load_obj(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) { fprintf(stderr, "OBJ: 无法打开 %s\n", path); return NULL; }
    Obj o;
    memset(&o, 0, sizeof(o));
    char mtl[64] = "";
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char* p = line;
        while (*p == ' ' || *p == '	') p++;
        if (*p == '#' || *p == '\n' || *p == '\r' || *p == 0) continue;
        if (strncmp(p, "v ", 2) == 0) {
            float x, y, z;
            if (sscanf(p + 2, "%f %f %f", &x, &y, &z) == 3) obj_push_v(&o, x, y, z);
        } else if (strncmp(p, "vt", 2) == 0) {
            float u, v;
            if (sscanf(p + 3, "%f %f", &u, &v) == 2) obj_push_vt(&o, u, v);
        } else if (strncmp(p, "vn", 2) == 0) {
            float x, y, z;
            if (sscanf(p + 3, "%f %f %f", &x, &y, &z) == 3) obj_push_vn(&o, x, y, z);
        } else if (strncmp(p, "usemtl", 6) == 0 && !mtl[0]) {
            char name[64];
            if (sscanf(p + 7, "%63s", name) == 1) strncpy(mtl, name, sizeof(mtl) - 1);
        } else if (*p == 'f') {
            int fi[16][3];
            int nf = 0;
            char* tok = p + 1;
            while (nf < 16) {
                while (*tok == ' ' || *tok == '	') tok++;
                if (*tok == '\n' || *tok == '\r' || *tok == 0) break;
                int vi = 0, ti = 0, ni = 0;
                int got = sscanf(tok, "%d/%d/%d", &vi, &ti, &ni);
                if (got < 3) {
                    ti = ni = 0;
                    if (sscanf(tok, "%d//%d", &vi, &ni) < 2)
                        sscanf(tok, "%d", &vi);
                }
                fi[nf][0] = vi; fi[nf][1] = ti; fi[nf][2] = ni;
                nf++;
                while (*tok && *tok != ' ' && *tok != '\t' &&
                       *tok != '\n' && *tok != '\r') tok++;
            }
            if (nf < 3) continue;
            /* 扇形拆三角形（含四边形） */
            for (int k = 1; k + 1 < nf; k++) {
                obj_emit(&o, fi[0][0], fi[0][1], fi[0][2]);
                obj_emit(&o, fi[k][0],   fi[k][1],   fi[k][2]);
                obj_emit(&o, fi[k+1][0], fi[k+1][1], fi[k+1][2]);
            }
        }
    }
    fclose(f);
    if (o.no == 0 || !o.out) {
        fprintf(stderr, "OBJ: %s 无面数据\n", path);
        free(o.v); free(o.vt); free(o.vn); free(o.out);
        return NULL;
    }
    sp_mesh* m = (sp_mesh*)calloc(1, sizeof(sp_mesh));
    if (!m) { free(o.out); free(o.v); free(o.vt); free(o.vn); return NULL; }
    m->vcount = o.no / 8;
    m->verts = o.out;                      /* 转移所有权 */
    m->idx = (unsigned*)malloc((size_t)m->vcount * sizeof(unsigned));
    for (int i = 0; i < m->vcount; i++) m->idx[i] = (unsigned)i;
    strncpy(m->mtl, mtl, sizeof(m->mtl) - 1);
    free(o.v); free(o.vt); free(o.vn);
    return m;
}

SP_RES_API void sp_free_mesh(sp_mesh* m) {
    if (!m) return;
    free(m->verts);
    free(m->idx);
    free(m);
}

/* ================= BMP 纹理（24/32bpp BI_RGB） ================= */
SP_RES_API int sp_load_texture_bmp(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "BMP: 无法打开 %s\n", path); return 0; }
    unsigned char hdr[54];
    if (fread(hdr, 1, 54, f) != 54 || hdr[0] != 'B' || hdr[1] != 'M') {
        fprintf(stderr, "BMP: %s 头无效\n", path);
        fclose(f); return 0;
    }
    int w, h;
    memcpy(&w, hdr + 18, 4);
    memcpy(&h, hdr + 22, 4);
    unsigned short bpp;
    memcpy(&bpp, hdr + 28, 2);
    unsigned comp;
    memcpy(&comp, hdr + 30, 4);
    int topdown = 0;
    if (h < 0) { topdown = 1; h = -h; }
    if (w <= 0 || h <= 0 || (bpp != 24 && bpp != 32) || comp != 0) {
        fprintf(stderr, "BMP: %s 不支持的格式 (w=%d h=%d bpp=%u comp=%u)\n",
                path, w, h, bpp, comp);
        fclose(f); return 0;
    }
    int row = (int)(((w * (int)bpp / 8) + 3) & ~3);
    unsigned char* buf = (unsigned char*)malloc((size_t)row * h);
    if (!buf) { fclose(f); return 0; }
    long off;
    memcpy(&off, hdr + 10, 4);
    fseek(f, off, SEEK_SET);
    if (fread(buf, 1, (size_t)row * h, f) != (size_t)row * h) {
        fprintf(stderr, "BMP: %s 数据不完整\n", path);
        free(buf); fclose(f); return 0;
    }
    fclose(f);
    unsigned char* rgba = (unsigned char*)malloc((size_t)w * h * 4);
    if (!rgba) { free(buf); return 0; }
    int bppb = (int)bpp / 8;
    for (int y = 0; y < h; y++) {
        int sy = topdown ? y : (h - 1 - y);
        const unsigned char* s = buf + (size_t)sy * row;
        unsigned char* d = rgba + (size_t)y * w * 4;
        for (int x = 0; x < w; x++) {
            d[x*4+0] = s[x*bppb+2];   /* BGR -> RGB */
            d[x*4+1] = s[x*bppb+1];
            d[x*4+2] = s[x*bppb+0];
            d[x*4+3] = (bppb == 4) ? s[x*4+3] : 255;
        }
    }
    free(buf);
    int tex = sp_gen_texture(w, h, rgba);
    free(rgba);
    if (!tex) fprintf(stderr, "BMP: %s 纹理槽已满\n", path);
    return tex;
}
