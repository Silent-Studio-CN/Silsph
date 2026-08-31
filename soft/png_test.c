// (C) SilentStudio — All Rights Reserved.
// Proprietary license: 未经 SilentStudio 书面许可，禁止复制、分发、修改或使用。
// png_test.c — PNG 纹理加载验证
#include "silsph_soft.h"
#include "silsph_res.h"
#include <stdio.h>

int main(void) {
    int tex = sp_load_texture_png("test.png");
    if (!tex) { printf("PNG 加载失败\n"); return 1; }
    printf("PNG 纹理 ID = %d\n", tex);
    if (!sp_create(256, 256)) return 1;
    sp_viewport(0, 0, 256, 256);
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
    long red = 0, green = 0, blue = 0, white = 0;
    for (int i = 0; i < 256 * 256; i++) {
        uint32_t c = px[i];
        int r = (int)((c >> 16) & 0xFF), g = (int)((c >> 8) & 0xFF), b = (int)(c & 0xFF);
        if (r > 200 && g < 80 && b < 80) red++;
        else if (g > 200 && r < 80 && b < 80) green++;
        else if (b > 200 && r < 80 && g < 80) blue++;
        else if (r > 200 && g > 200 && b > 200) white++;
    }
    printf("红=%ld 绿=%ld 蓝=%ld 白=%ld\n", red, green, blue, white);
    int ok = red > 10000 && green > 10000 && blue > 10000 && white > 10000;
    printf("\n%s\n", ok ? "ALL PASS" : "FAILED");
    sp_destroy();
    return ok ? 0 : 1;
}
