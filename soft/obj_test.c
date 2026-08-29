// (C) SilentStudio — All Rights Reserved.
// Proprietary license: 未经 SilentStudio 书面许可，禁止复制、分发、修改或使用。
// obj_test.c — OBJ 加载器验证：程序生成金字塔 OBJ -> 加载 -> 渲染 -> 像素统计
#include "silsph_soft.h"
#include "silsph_res.h"
#include <stdio.h>

static void gen_pyramid_obj(const char* path) {
    FILE* f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "# pyramid test\n");
    fprintf(f, "v 0 1.2 0\nv -1 0 -1\nv 1 0 -1\nv 1 0 1\nv -1 0 1\n");
    fprintf(f, "vt 0 0\nvt 1 0\nvt 0 1\nvt 1 1\n");
    fprintf(f, "vn 0 1 0\nvn 0 0 -1\nvn 1 0 0\nvn 0 0 1\nvn -1 0 0\n");
    fprintf(f, "f 1/1/2 2/2/2 3/3/2\n");
    fprintf(f, "f 1/1/3 3/2/3 4/3/3\n");
    fprintf(f, "f 1/1/4 4/2/4 5/3/4\n");
    fprintf(f, "f 1/1/5 5/2/5 2/3/5\n");
    fprintf(f, "f 2/1/1 4/2/1 3/3/1\n");
    fprintf(f, "f 2/1/1 5/2/1 4/3/1\n");
    fclose(f);
}

int main(void) {
    gen_pyramid_obj("pyramid.obj");
    sp_mesh* m = sp_load_obj("pyramid.obj");
    if (!m) { printf("加载失败\n"); return 1; }
    printf("顶点数 = %d (预期 18 = 6 三角形 x 3)\n", m->vcount);

    if (!sp_create(960, 640)) return 1;
    sp_viewport(0, 0, 960, 640);
    sp_clear_color(0.063f, 0.075f, 0.102f, 1.0f);
    sp_clear(SP_COLOR | SP_DEPTH);
    sp_cull_face(0);   /* 绕序验证另行处理；这里验证加载+变换+光栅化 */
    sp_matrix_mode(SP_PROJECTION); sp_load_identity();
    sp_perspective(55.0f, 960.0f / 640.0f, 0.1f, 100.0f);
    sp_matrix_mode(SP_MODELVIEW); sp_load_identity();
    sp_look_at(3, 2.5f, 5, 0, 0.4f, 0, 0, 1, 0);
    sp_rotate(20, 0, 1, 0);
    sp_begin(SP_TRIANGLES);
    for (int i = 0; i < m->vcount; i++) {
        const float* v = m->verts + i * 8;
        float nx = v[3], ny = v[4], nz = v[5];
        float d = nx * 0.4f + ny * 0.8f + nz * 0.6f;
        if (d < 0) d = 0;
        float light = 0.25f + 0.75f * d;
        sp_color3f(0.9f * light, 0.6f * light, 0.3f * light);
        sp_vertex3f(v[0], v[1], v[2]);
    }
    sp_end();
    long n = 0, sx = 0, sy = 0;
    const uint32_t* px = (const uint32_t*)sp_pixels(NULL, NULL);
    const uint32_t bg = 0xFF10131Au;
    for (int y = 0; y < 640; y++)
        for (int x = 0; x < 960; x++)
            if (px[(size_t)y * 960 + x] != bg) { n++; sx += x; sy += y; }
    printf("渲染像素 = %ld, 质心 = (%ld, %ld)\n", n, n ? sx / n : -1, n ? sy / n : -1);

    int ok = (m->vcount == 18) && (n > 5000);
    printf("\n%s\n", ok ? "ALL PASS" : "FAILED");
    sp_free_mesh(m);
    sp_destroy();
    return ok ? 0 : 1;
}
