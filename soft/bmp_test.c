// (C) SilentStudio — All Rights Reserved.
// Proprietary license: 未经 SilentStudio 书面许可，禁止复制、分发、修改或使用。
// bmp_test.c — BMP 纹理加载验证：左红右蓝 64x64 BMP -> 贴四边形 -> 像素验证
// 注意：帧缓冲内存序 B,G,R,A；uint32 值 = A<<24|R<<16|G<<8|B（R 在 16-23 位）
#include "silsph_soft.h"
#include "silsph_res.h"
#include <stdio.h>

int main(void) {
    int tex = sp_load_texture_bmp("test.bmp");
    if (!tex) { printf("BMP 加载失败\n"); return 1; }
    printf("纹理 ID = %d\n", tex);

    if (!sp_create(480, 320)) return 1;
    sp_viewport(0, 0, 480, 320);
    sp_clear_color(0, 0, 0, 1);
    sp_clear(SP_COLOR | SP_DEPTH);
    sp_matrix_mode(SP_PROJECTION); sp_load_identity();
    sp_ortho(-1, 1, -1, 1, 0.1f, 10.0f);
    sp_matrix_mode(SP_MODELVIEW); sp_load_identity();
    sp_bind_texture(tex);
    sp_tex_filter(SP_TEX_NEAREST);
    sp_color3f(1, 1, 1);
    sp_begin(SP_TRIANGLES);
    sp_texcoord2f(0,0); sp_vertex3f(-1,-1,-1);
    sp_texcoord2f(1,0); sp_vertex3f( 1,-1,-1);
    sp_texcoord2f(1,1); sp_vertex3f( 1, 1,-1);
    sp_texcoord2f(0,0); sp_vertex3f(-1,-1,-1);
    sp_texcoord2f(1,1); sp_vertex3f( 1, 1,-1);
    sp_texcoord2f(0,1); sp_vertex3f(-1, 1,-1);
    sp_end();

    const uint32_t* px = (const uint32_t*)sp_pixels(NULL, NULL);
    long red = 0, blue = 0, other = 0;
    for (int i = 0; i < 480 * 320; i++) {
        uint32_t c = px[i];
        int r = (int)((c >> 16) & 0xFF), g = (int)((c >> 8) & 0xFF), b = (int)(c & 0xFF);
        if (r > 200 && g < 80 && b < 80) red++;
        else if (b > 200 && r < 80 && g < 80) blue++;
        else if (c != 0) other++;
    }
    printf("红色像素 = %ld (预期~76800), 蓝色像素 = %ld (预期~76800), 其他 = %ld\n", red, blue, other);
    int ok = (red > 70000) && (blue > 70000) && (other < 2000);
    printf("\n%s\n", ok ? "ALL PASS" : "FAILED");
    sp_destroy();
    return ok ? 0 : 1;
}
