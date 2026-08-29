// (C) SilentStudio — All Rights Reserved.
// Proprietary license: 未经 SilentStudio 书面许可，禁止复制、分发、修改或使用。
// pick_test.c — 拾取（ID 缓冲）验证
// 场景：红色方块(ID=1)左、蓝色方块(ID=2)右，中间空隙(ID=0)；后方方块被前方遮挡时拾取前方
#include "silsph_soft.h"
#include <stdio.h>

static void square(float x0, float y0, float x1, float y1, float z, float r, float g, float b) {
    sp_color3f(r, g, b);
    sp_begin(SP_TRIANGLES);
    sp_vertex3f(x0, y0, z); sp_vertex3f(x1, y0, z); sp_vertex3f(x1, y1, z);
    sp_vertex3f(x0, y0, z); sp_vertex3f(x1, y1, z); sp_vertex3f(x0, y1, z);
    sp_end();
}

int main(void) {
    if (!sp_create(480, 320)) return 1;
    sp_viewport(0, 0, 480, 320);
    sp_clear_color(0, 0, 0, 1);
    sp_matrix_mode(SP_PROJECTION); sp_load_identity();
    sp_ortho(-1, 1, -1, 1, 0.1f, 10.0f);
    sp_matrix_mode(SP_MODELVIEW); sp_load_identity();

    /* 拾取帧：清 颜色+深度+ID，两物体带 ID 渲染 */
    sp_clear(SP_COLOR | SP_DEPTH | SP_ID);
    sp_load_id(1); square(-0.9f, -0.5f, -0.2f, 0.5f, -1.0f, 0.9f, 0.2f, 0.2f);   /* 左：ID1 */
    sp_load_id(2); square( 0.2f, -0.5f,  0.9f, 0.5f, -1.0f, 0.2f, 0.2f, 0.9f);   /* 右：ID2 */
    /* 中间空隙 + 一个后方方块测试遮挡：前方小方块(ID=3)盖住 ID2 方块中心 */
    sp_load_id(3); square( 0.35f, -0.2f, 0.75f, 0.2f, -0.5f, 0.2f, 0.9f, 0.2f); /* 前：ID3 z=-0.5 更近 */

    /* 拾取点：屏幕坐标（y 向下） */
    int p1 = sp_pick_id(120, 160);   /* 左方块中心 -> 1 */
    int p2 = sp_pick_id(360, 160);   /* 右方块中心 -> 3（被前方覆盖） */
    int p3 = sp_pick_id(360, 200);   /* 右方块下部 -> 2（未被覆盖） */
    int p4 = sp_pick_id(240, 160);   /* 空隙 -> 0 */
    int p5 = sp_pick_id(-5, -5);     /* 越界 -> 0 */

    printf("左方块中心     -> ID %d (预期 1)\n", p1);
    printf("右方块中心(遮挡)-> ID %d (预期 3)\n", p2);
    printf("右方块下部     -> ID %d (预期 2)\n", p3);
    printf("空隙           -> ID %d (预期 0)\n", p4);
    printf("越界           -> ID %d (预期 0)\n", p5);

    int ok = (p1 == 1) && (p2 == 3) && (p3 == 2) && (p4 == 0) && (p5 == 0);
    printf("\n%s\n", ok ? "ALL PASS" : "FAILED");
    sp_destroy();
    return ok ? 0 : 1;
}
