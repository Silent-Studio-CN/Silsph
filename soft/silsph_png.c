// (C) SilentStudio — All Rights Reserved.
// Proprietary license: 未经 SilentStudio 书面许可，禁止复制、分发、修改或使用。
// silsph_png.c — PNG 纹理解码（零第三方依赖：手写 DEFLATE inflate + PNG 滤波）
// 支持：8-bit 深度、颜色类型 0/2/4/6（灰度/RGB/灰度+alpha/RGBA）、无交织、全部 5 种滤波
#include "silsph_res.h"
#include "silsph_soft.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================= DEFLATE (RFC 1951) ================= */
typedef struct {
    const unsigned char* in;
    size_t in_len, pos;
    unsigned bitbuf;
    int bitcnt;
    unsigned char* out;
    size_t out_len, out_cap;
} Infl;

static int infl_byte(Infl* z) {
    if (z->pos >= z->in_len) return -1;
    return z->in[z->pos++];
}
static int infl_bit(Infl* z) {
    if (z->bitcnt == 0) {
        int b = infl_byte(z);
        if (b < 0) return -1;
        z->bitbuf = (unsigned)b;
        z->bitcnt = 8;
    }
    int r = (int)(z->bitbuf & 1);
    z->bitbuf >>= 1;
    z->bitcnt--;
    return r;
}
static unsigned infl_bits(Infl* z, int n) {   /* LSB-first */
    unsigned v = 0;
    for (int i = 0; i < n; i++) {
        int b = infl_bit(z);
        if (b < 0) return 0;   /* 数据截断：按 0 处理（文件已校验长度，正常不会触发） */
        v |= (unsigned)b << i;
    }
    return v;
}
static int infl_put(Infl* z, unsigned char c) {
    if (z->out_len >= z->out_cap) {
        size_t nc = z->out_cap ? z->out_cap * 2 : 65536;
        unsigned char* np = (unsigned char*)realloc(z->out, nc);
        if (!np) return 0;
        z->out = np;
        z->out_cap = nc;
    }
    z->out[z->out_len++] = c;
    return 1;
}

/* canonical Huffman 解码表 */
typedef struct {
    int count[16];        /* count[len] = 码长 len 的符号数 */
    unsigned symbol[288]; /* 按 (len, symbol) 排序的符号 */
    int maxlen;
} Huff;

static void huff_build(Huff* h, const int* lens, int nsym) {
    memset(h, 0, sizeof(*h));
    for (int i = 0; i < nsym; i++) {
        if (lens[i] > 0 && lens[i] <= 15) h->count[lens[i]]++;  /* 防御：码长越界忽略 */
    }
    int offs[16];
    offs[0] = 0;
    for (int i = 1; i < 16; i++) offs[i] = offs[i - 1] + h->count[i - 1];
    for (int i = 0; i < nsym; i++) {
        if (lens[i] > 0 && lens[i] <= 15) h->symbol[offs[lens[i]]++] = (unsigned)i;
    }
    h->maxlen = 15;
}
static unsigned huff_decode(Infl* z, const Huff* h) {
    unsigned code = 0;
    int first = 0, index = 0;
    for (int len = 1; len <= h->maxlen; len++) {
        int b = infl_bit(z);
        if (b < 0) return 0;
        code = (code << 1) | (unsigned)b;   /* MSB-first 累积（canonical 标准） */
        int cnt = h->count[len];
        if (code - (unsigned)first < (unsigned)cnt)
            return h->symbol[index + (int)(code - (unsigned)first)];
        index += cnt;
        first = (first + cnt) << 1;
    }
    return 0;   /* 无效码（数据损坏） */
}

/* 长度/距离基表（RFC 1951） */
static const unsigned LEN_BASE[29] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
static const unsigned LEN_EXTRA[29] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
static const unsigned DIST_BASE[30] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
static const unsigned DIST_EXTRA[30] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
static const unsigned CLEN_ORDER[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};

static void fixed_litlen(int lens[288]) {
    for (int i = 0; i < 144; i++) lens[i] = 8;
    for (int i = 144; i < 256; i++) lens[i] = 9;
    for (int i = 256; i < 280; i++) lens[i] = 7;
    for (int i = 280; i < 288; i++) lens[i] = 8;
}

/* 解压一段 deflate 流（不含 zlib 头尾） */
static int inflate(Infl* z) {
    for (;;) {
        int bfinal = infl_bit(z);
        if (bfinal < 0) break;
        int btype = (int)infl_bits(z, 2);
#ifdef SP_DEBUG
        fprintf(stderr, "[inflate] final=%d type=%d pos=%zu\n", bfinal, btype, z->pos);
#endif
        if (btype == 0) {
            /* 无压缩块：字节对齐 + LEN/NLEN */
            z->bitcnt = 0;
            if (z->pos + 4 > z->in_len) return 0;
            unsigned len = (unsigned)z->in[z->pos] | ((unsigned)z->in[z->pos + 1] << 8);
            z->pos += 4;
            for (unsigned i = 0; i < len; i++) {
                int b = infl_byte(z);
                if (b < 0) return 0;
                if (!infl_put(z, (unsigned char)b)) return 0;
            }
        } else if (btype == 1 || btype == 2) {
            Huff lit, dst;
            int lens[288] = {0};
            int dlens[32] = {0};   /* hdist 最大 32；剩余必须为 0（huff_build 会读全表） */
            if (btype == 1) {
                fixed_litlen(lens);
                for (int i = 0; i < 30; i++) dlens[i] = 5;
            } else {
                /* 动态块（RFC 1951 3.2.7）：字段顺序 = HLIT -> HDIST -> HCLEN -> 码长表 */
                int hlit = (int)infl_bits(z, 5) + 257;
                int hdist = (int)infl_bits(z, 5) + 1;
                int hclen = (int)infl_bits(z, 4) + 4;
                int clens[19] = {0};
                for (int i = 0; i < hclen; i++) clens[CLEN_ORDER[i]] = (int)infl_bits(z, 3);
                Huff cl;
                huff_build(&cl, clens, 19);
                int all[288 + 32];   /* hlit<=288, hdist<=32 */
                int n = 0;
                while (n < hlit + hdist) {
                    unsigned sym = huff_decode(z, &cl);
                    if (sym < 16) {
                        all[n++] = (int)sym;
                    } else if (sym == 16) {
                        int rep = (int)infl_bits(z, 2) + 3;
                        int prev = n > 0 ? all[n - 1] : 0;
                        for (int i = 0; i < rep && n < hlit + hdist; i++) all[n++] = prev;
                    } else if (sym == 17) {
                        int rep = (int)infl_bits(z, 3) + 3;
                        for (int i = 0; i < rep && n < hlit + hdist; i++) all[n++] = 0;
                    } else { /* 18 */
                        int rep = (int)infl_bits(z, 7) + 11;
                        for (int i = 0; i < rep && n < hlit + hdist; i++) all[n++] = 0;
                    }
                }
                memcpy(lens, all, sizeof(int) * hlit);
                memcpy(dlens, all + hlit, sizeof(int) * hdist);   /* hdist<=32 <= 32 */
            }
            huff_build(&lit, lens, 288);
            huff_build(&dst, dlens, 30);
            /* 符号流解码 */
            for (;;) {
                unsigned sym = huff_decode(z, &lit);
                if (sym < 256) {
                    if (!infl_put(z, (unsigned char)sym)) return 0;
                } else if (sym == 256) {
                    break;
                } else {
                    unsigned li = sym - 257;
                    if (li >= 29) return 0;
                    unsigned len = LEN_BASE[li] + infl_bits(z, (int)LEN_EXTRA[li]);
                    unsigned dsym = huff_decode(z, &dst);
                    if (dsym >= 30) return 0;
                    unsigned dist = DIST_BASE[dsym] + infl_bits(z, (int)DIST_EXTRA[dsym]);
                    if (dist == 0 || dist > z->out_len) return 0;
                    for (unsigned i = 0; i < len; i++) {
                        unsigned char c = z->out[z->out_len - dist];
                        if (!infl_put(z, c)) return 0;
                    }
                }
            }
        } else {
            return 0;   /* 保留块类型 */
        }
        if (bfinal) break;
    }
    return 1;
}

/* 解压 zlib 流（2 字节头 + deflate + 4 字节 adler32） */
static unsigned char* zlib_inflate(const unsigned char* data, size_t len, size_t* out_len) {
    if (len < 6) return NULL;
    if ((data[0] & 0x0F) != 8) return NULL;   /* CM=8 */
    Infl z;
    memset(&z, 0, sizeof(z));
    z.in = data + 2;      /* 跳过 zlib 头 */
    z.in_len = len - 6;   /* 去掉 adler32 */
    if (!inflate(&z)) { free(z.out); return NULL; }
    *out_len = z.out_len;
    return z.out;
}

/* ================= PNG 解析 ================= */
static unsigned rd32(const unsigned char* p) {
    return ((unsigned)p[0] << 24) | ((unsigned)p[1] << 16) | ((unsigned)p[2] << 8) | p[3];
}

/* 行滤波重建（Paeth 等），逐行原地 */
static int unfilter(unsigned char* raw, int w, int h, int bpp) {
    int stride = 1 + w * bpp;
    for (int y = 0; y < h; y++) {
        unsigned char* row = raw + (size_t)y * stride + 1;
        const unsigned char* prev = y > 0 ? raw + (size_t)(y - 1) * stride + 1 : NULL;
        int ft = raw[(size_t)y * stride];
        for (int x = 0; x < w * bpp; x++) {
            int a = x >= bpp ? row[x - bpp] : 0;
            int b = prev ? prev[x] : 0;
            int c = (x >= bpp && prev) ? prev[x - bpp] : 0;
            int val;
            switch (ft) {
            case 0: val = row[x]; break;
            case 1: val = row[x] + a; break;
            case 2: val = row[x] + b; break;
            case 3: val = row[x] + (a + b) / 2; break;
            case 4: {
                int p = a + b - c;
                int pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
                val = row[x] + (pa <= pb && pa <= pc ? a : (pb <= pc ? b : c));
                break;
            }
            default: return 0;
            }
            row[x] = (unsigned char)(val & 0xFF);
        }
#ifdef SP_DEBUG
        if (y < 5) {
            fprintf(stderr, "unf y=%d ft=%d: %02x%02x%02x %02x%02x%02x\n", y, ft,
                    row[0], row[1], row[2], row[3], row[4], row[5]);
        }
#endif
    }
    return 1;
}

/* 加载 PNG -> 纹理 ID（失败 0）。仅支持 8-bit / 0,2,4,6 色型 / 无交织 */
SP_RES_API int sp_load_texture_png(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "PNG: 无法打开 %s\n", path); return 0; }
    fseek(f, 0, SEEK_END);
    long fsz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsz < 8) { fclose(f); return 0; }
    unsigned char* file = (unsigned char*)malloc((size_t)fsz);
    if (!file) { fclose(f); return 0; }
    if (fread(file, 1, (size_t)fsz, f) != (size_t)fsz) { free(file); fclose(f); return 0; }
    fclose(f);

    static const unsigned char SIG[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    if (memcmp(file, SIG, 8) != 0) { free(file); fprintf(stderr, "PNG: %s 签名无效\n", path); return 0; }

    int w = 0, h = 0, bitdepth = 0, colortype = 0, interlace = 0;
    unsigned char* idat = NULL;
    size_t idat_len = 0, idat_cap = 0;
    int ok = 0;

    size_t pos = 8;
    while (pos + 8 <= (size_t)fsz) {
        unsigned len = rd32(file + pos);
        const unsigned char* type = file + pos + 4;
        const unsigned char* data = file + pos + 8;
        if (pos + 12 + len > (size_t)fsz) break;
        if (memcmp(type, "IHDR", 4) == 0 && len >= 13) {
            w = (int)rd32(data);
            h = (int)rd32(data + 4);
            bitdepth = data[8];
            colortype = data[9];
            interlace = data[12];
        } else if (memcmp(type, "IDAT", 4) == 0) {
            if (idat_len + len > idat_cap) {
                size_t nc = idat_cap ? idat_cap * 2 : 65536;
                while (nc < idat_len + len) nc *= 2;
                unsigned char* np = (unsigned char*)realloc(idat, nc);
                if (!np) { free(idat); free(file); return 0; }
                idat = np;
                idat_cap = nc;
            }
            memcpy(idat + idat_len, data, len);
            idat_len += len;
        } else if (memcmp(type, "IEND", 4) == 0) {
            ok = 1;
            break;
        }
        pos += 12 + len;
    }
    if (!ok || !idat_len) { free(idat); free(file); fprintf(stderr, "PNG: %s 缺少 IDAT/IEND\n", path); return 0; }
    if (bitdepth != 8 || interlace != 0 ||
        (colortype != 0 && colortype != 2 && colortype != 4 && colortype != 6)) {
        free(idat); free(file);
        fprintf(stderr, "PNG: %s 不支持的格式 (bitdepth=%d colortype=%d interlace=%d)\n",
                path, bitdepth, colortype, interlace);
        return 0;
    }

    size_t raw_len = 0;
    unsigned char* raw = zlib_inflate(idat, idat_len, &raw_len);
    free(idat);
    if (!raw) { free(file); fprintf(stderr, "PNG: %s inflate 失败\n", path); return 0; }

    int ch = colortype == 0 ? 1 : (colortype == 2 ? 3 : (colortype == 4 ? 2 : 4));
    size_t expect = (size_t)(1 + w * ch) * h;
    if (raw_len != expect) {
        free(raw); free(file);
        fprintf(stderr, "PNG: %s 解压长度不符 (%zu vs %zu)\n", path, raw_len, expect);
        return 0;
    }
    if (!unfilter(raw, w, h, ch)) { free(raw); free(file); return 0; }

    /* 转 RGBA */
    unsigned char* rgba = (unsigned char*)malloc((size_t)w * h * 4);
    if (!rgba) { free(raw); free(file); return 0; }
#ifdef SP_DEBUG
    fprintf(stderr, "png: w=%d h=%d ch=%d stride=%d malloc=%zu\n", w, h, ch, 1 + w * ch, (size_t)w * h * 4);
    fprintf(stderr, "addr: raw=%p rgba=%p delta=%td\n", (void*)raw, (void*)rgba, (char*)rgba - (char*)raw);
#endif
    int stride = 1 + w * ch;
    for (int y = 0; y < h; y++) {
        const unsigned char* row = raw + (size_t)y * stride + 1;
        for (int x = 0; x < w; x++) {
            const unsigned char* s = row + (size_t)x * ch;
            unsigned char* d = rgba + (size_t)y * w * 4 + (size_t)x * 4;
            switch (colortype) {
            case 0: d[0] = d[1] = d[2] = s[0]; d[3] = 255; break;
            case 2:
                d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = 255;
#ifdef SP_DEBUG
                if (y == 0 && x == 0) fprintf(stderr, "case2 写后 d[0..3]=%d %d %d %d\n", d[0], d[1], d[2], d[3]);
#endif
                break;
            case 4: d[0] = d[1] = d[2] = s[0]; d[3] = s[1]; break;
            case 6: d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = s[3]; break;
            }
        }
    }
#ifdef SP_DEBUG
    { unsigned char dbg[16]; memcpy(dbg, rgba, 16);
      fprintf(stderr, "终检副本 rgba[0..3]=%d %d %d %d (rgba=%p)\n", dbg[0], dbg[1], dbg[2], dbg[3], (void*)rgba);
      fprintf(stderr, "终检直读 rgba[0..3]=%d %d %d %d\n", rgba[0], rgba[1], rgba[2], rgba[3]);
    }
#endif
    free(raw);
    free(file);
    int tex = sp_gen_texture(w, h, rgba);
    free(rgba);
    if (!tex) fprintf(stderr, "PNG: %s 纹理槽已满\n", path);
    return tex;
}
