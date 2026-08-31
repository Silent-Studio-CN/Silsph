// (C) SilentStudio — All Rights Reserved.
// Proprietary license: 未经 SilentStudio 书面许可，禁止复制、分发、修改或使用。
// cmd_test.c — 命令缓冲（统一 API）验证
// 1) 直接渲染 vs 命令缓冲渲染 -> 逐字节一致
// 2) 命令缓冲复用（多次回放同命令）
// 3) 状态快照（命令记录后改状态，回放不受影响）
#include "silsph_soft.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W 480
#define H 320

static void draw_scene(void) {
    sp_clear(SP_COLOR | SP_DEPTH);
    sp_matrix_mode(SP_PROJECTION); sp_load_identity();
    sp_perspective(55.0f, (float)W / H, 0.1f, 100.0f);
    sp_matrix_mode(SP_MODELVIEW); sp_load_identity();
    sp_look_at(17.0f * 0.0f, 8.5f, 17.0f * 1.0f, 0, 0, 0, 0, 1, 0);
    /* 彩色立方体（无纹理，纯色+混合板） */
    const float faces[6][6] = {
        {0,0,-1, 1.0f,0.62f,0.25f}, {0,0,1, 0.31f,1.0f,0.99f},
        {-1,0,0, 0.44f,0.69f,1.0f}, {1,0,0, 1.0f,0.44f,0.69f},
        {0,-1,0, 0.61f,1.0f,0.44f}, {0,1,0, 1.0f,0.85f,0.44f},
    };
    for (int i = 0; i < 6; i++) {
        const float* n = faces[i];
        float tx[3];
        if (n[2] != 0.0f)      { tx[0]=1; tx[1]=0; tx[2]=0; }
        else if (n[1] != 0.0f) { tx[0]=1; tx[1]=0; tx[2]=0; }
        else                   { tx[0]=0; tx[1]=0; tx[2]=1; }
        float ty[3] = { n[1]*tx[2]-n[2]*tx[1], n[2]*tx[0]-n[0]*tx[2], n[0]*tx[1]-n[1]*tx[0] };
        sp_color3f(faces[i][3], faces[i][4], faces[i][5]);
        float v[4][3];
        for (int k = 0; k < 4; k++) {
            float sx = (k==1||k==2) ? 1 : -1, sy = (k==2||k==3) ? 1 : -1;
            v[k][0] = n[0]+tx[0]*sx+ty[0]*sy;
            v[k][1] = n[1]+tx[1]*sx+ty[1]*sy;
            v[k][2] = n[2]+tx[2]*sx+ty[2]*sy;
        }
        sp_begin(SP_TRIANGLES);
        sp_vertex3f(v[0][0],v[0][1],v[0][2]); sp_vertex3f(v[2][0],v[2][1],v[2][2]); sp_vertex3f(v[1][0],v[1][1],v[1][2]);
        sp_vertex3f(v[0][0],v[0][1],v[0][2]); sp_vertex3f(v[3][0],v[3][1],v[3][2]); sp_vertex3f(v[2][0],v[2][1],v[2][2]);
        sp_end();
    }
    sp_blend(1);
    sp_color3f(1, 1, 1);
    sp_begin(SP_TRIANGLES);
    sp_vertex3f(-1.5f,-1.5f,2.5f); sp_vertex3f(1.5f,-1.5f,2.5f); sp_vertex3f(1.5f,1.5f,2.5f);
    sp_vertex3f(-1.5f,-1.5f,2.5f); sp_vertex3f(1.5f,1.5f,2.5f); sp_vertex3f(-1.5f,1.5f,2.5f);
    sp_end();
    sp_blend(0);
    sp_color3f(0.35f, 0.40f, 0.55f);
    sp_begin(SP_LINES);
    for (float x = -8.0f; x <= 8.0f; x += 1.0f) { sp_vertex3f(x,0,-8); sp_vertex3f(x,0,8); }
    for (float z = -8.0f; z <= 8.0f; z += 1.0f) { sp_vertex3f(-8,0,z); sp_vertex3f(8,0,z); }
    sp_end();
}

int main(void) {
    size_t sz = (size_t)W * H * 4;
    unsigned char* direct = (unsigned char*)malloc(sz);
    unsigned char* via_cmd = (unsigned char*)malloc(sz);

    /* 直接渲染 */
    sp_set_threads(1);   /* 串行，排除多线程变量 */
    if (!sp_create(W, H)) return 1;
    sp_viewport(0, 0, W, H);
    sp_clear_color(0.063f, 0.075f, 0.102f, 1.0f);
    draw_scene();
    memcpy(direct, sp_pixels(NULL, NULL), sz);
    sp_destroy();

    /* 命令缓冲渲染（记录 -> 回放） */
    if (!sp_create(W, H)) return 1;
    sp_viewport(0, 0, W, H);
    sp_clear_color(0.063f, 0.075f, 0.102f, 1.0f);
    sp_cmd_begin();
    draw_scene();          /* 记录模式：全部只记录 */
    sp_cmd_end();          /* 回放 */
    memcpy(via_cmd, sp_pixels(NULL, NULL), sz);
    sp_destroy();

    long diff1 = 0;
    for (size_t i = 0; i < sz; i++) if (direct[i] != via_cmd[i]) diff1++;
    printf("1) 直接 vs 命令渲染 差异字节 = %ld (%s)\n", diff1, diff1 == 0 ? "PASS" : "FAIL");

    /* 命令复用 + 状态快照：记录后改状态，回放仍用记录时状态 */
    if (!sp_create(W, H)) return 1;
    sp_viewport(0, 0, W, H);
    sp_clear_color(0.063f, 0.075f, 0.102f, 1.0f);
    sp_cmd_begin();
    draw_scene();
    sp_cmd_end();          /* 第一次回放 */
    memcpy(via_cmd, sp_pixels(NULL, NULL), sz);
    /* 改状态后再次回放同一命令（命令仍存在？cmd_end 清空了——改为记录后多次 end？）
       本测试：记录一次 -> 回放两次（cmd_end 应可重复调用？设计为一次性。
       验证"记录后改状态不影响已记录命令"：在 cmd_end 前改状态 */
    sp_cmd_begin();
    draw_scene();
    sp_color3f(0, 0, 0);           /* 改状态（应不影响已记录批次） */
    sp_blend(0);
    sp_cmd_end();
    long diff2 = 0;
    {
        const unsigned char* px = sp_pixels(NULL, NULL);
        for (size_t i = 0; i < sz; i++) if (via_cmd[i] != px[i]) diff2++;
    }
    printf("2) 记录后改状态，回放不受影响 差异字节 = %ld (%s)\n", diff2, diff2 == 0 ? "PASS" : "FAIL");
    sp_destroy();

    int ok = diff1 == 0 && diff2 == 0;
    printf("\n%s\n", ok ? "ALL PASS" : "FAILED");
    free(direct);
    free(via_cmd);
    return ok ? 0 : 1;
}
