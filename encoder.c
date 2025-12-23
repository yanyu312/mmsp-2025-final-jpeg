// encoder.c
// Mode 0:
//   encoder 0 input.bmp R.txt G.txt B.txt dim.txt
//
// Mode 1 :
//   encoder 1 input.bmp Qt_Y.txt Qt_Cb.txt Qt_Cr.txt dim.txt
//           qF_Y.raw qF_Cb.raw qF_Cr.raw eF_Y.raw eF_Cb.raw eF_Cr.raw
//
// Notes:
// - Supports 24-bit uncompressed BMP (BI_RGB) with row padding.
// - RGB -> YCbCr (JPEG-style), then level shift -128 before DCT.
// - DCT output eF_*.raw is float32, block-by-block (8x8), row-major.
// - Quantized output qF_*.raw is int16, same order as eF.

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#pragma pack(push, 1)
typedef struct {
    uint16_t bfType;      // 'BM'
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
} BMPFileHeader;

typedef struct {
    uint32_t biSize;        // 40
    int32_t  biWidth;
    int32_t  biHeight;      // + bottom-up, - top-down
    uint16_t biPlanes;      // 1
    uint16_t biBitCount;    // 24
    uint32_t biCompression; // 0 = BI_RGB
    uint32_t biSizeImage;   // can be 0 for BI_RGB
    int32_t  biXPelsPerMeter;
    int32_t  biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
} BMPInfoHeader;
#pragma pack(pop)

static void die(const char* msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

static uint8_t clamp_u8(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (uint8_t)v;
}

static int clamp_i16(int v) {
    if (v < -32768) return -32768;
    if (v >  32767) return  32767;
    return v;
}

// JPEG-style RGB -> YCbCr in [0,255]
static void rgb_to_ycbcr_u8(uint8_t R, uint8_t G, uint8_t B,
                            uint8_t *Y, uint8_t *Cb, uint8_t *Cr) {
    int y  = (int)( 0.299     * R + 0.587     * G + 0.114     * B + 0.5);
    int cb = (int)(-0.168736  * R - 0.331264  * G + 0.5       * B + 128.0 + 0.5);
    int cr = (int)( 0.5       * R - 0.418688  * G - 0.081312  * B + 128.0 + 0.5);
    *Y  = clamp_u8(y);
    *Cb = clamp_u8(cb);
    *Cr = clamp_u8(cr);
}

// Read BMP -> planar R/G/B (top-down in memory), each size width*height.
// Caller must free(*R,*G,*B).
static void read_bmp24_planar_rgb(const char* path,
                                  int *width, int *height,
                                  uint8_t **R, uint8_t **G, uint8_t **B)
{
    FILE *fin = fopen(path, "rb");
    if (!fin) {
        fprintf(stderr, "Cannot open BMP: %s\n", path);
        die("Tip: ensure BMP is 24-bit uncompressed and in the same folder, or use absolute path.");
    }

    BMPFileHeader fh;
    BMPInfoHeader ih;

    if (fread(&fh, sizeof(fh), 1, fin) != 1) die("Failed to read BMP file header.");
    if (fread(&ih, sizeof(ih), 1, fin) != 1) die("Failed to read BMP info header.");

    if (fh.bfType != 0x4D42) die("Not a BMP file (bfType != 'BM').");
    if (ih.biBitCount != 24) die("Only supports 24-bit BMP.");
    if (ih.biCompression != 0) die("Only supports uncompressed BMP (BI_RGB).");

    int w = ih.biWidth;
    int h_abs = (ih.biHeight >= 0) ? ih.biHeight : -ih.biHeight;
    int top_down_file = (ih.biHeight < 0);

    if (w <= 0 || h_abs <= 0) die("Invalid BMP dimensions.");

    int row_bytes = w * 3;
    int padding = (4 - (row_bytes % 4)) % 4;

    uint8_t *r = (uint8_t*)malloc((size_t)w * h_abs);
    uint8_t *g = (uint8_t*)malloc((size_t)w * h_abs);
    uint8_t *b = (uint8_t*)malloc((size_t)w * h_abs);
    if (!r || !g || !b) die("Out of memory.");

    if (fseek(fin, (long)fh.bfOffBits, SEEK_SET) != 0) die("Failed to seek to pixel data.");

    // Store as top-down in memory
    for (int y = 0; y < h_abs; y++) {
        int row = top_down_file ? y : (h_abs - 1 - y);
        for (int x = 0; x < w; x++) {
            unsigned char bgr[3];
            if (fread(bgr, 1, 3, fin) != 3) die("Unexpected EOF while reading pixels.");
            b[row * w + x] = bgr[0];
            g[row * w + x] = bgr[1];
            r[row * w + x] = bgr[2];
        }
        if (padding) {
            unsigned char tmp[3];
            if (fread(tmp, 1, (size_t)padding, fin) != (size_t)padding)
                die("Unexpected EOF while reading padding.");
        }
    }
    fclose(fin);

    *width = w;
    *height = h_abs;
    *R = r; *G = g; *B = b;
}

static void write_dim_txt(const char* path, int width, int height) {
    FILE *fd = fopen(path, "w");
    if (!fd) die("Cannot open dim.txt for writing.");
    fprintf(fd, "%d %d\n", width, height);
    fclose(fd);
}

static void write_channel_txt_u8(const char* path, const uint8_t* C, int width, int height) {
    FILE *f = fopen(path, "w");
    if (!f) die("Cannot open channel txt for writing.");
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            fprintf(f, "%u%s", (unsigned)C[y * width + x], (x == width - 1) ? "" : " ");
        }
        fprintf(f, "\n");
    }
    fclose(f);
}

static void write_qtable_txt(const char* path, const int Qt[8][8]) {
    FILE *f = fopen(path, "w");
    if (!f) die("Cannot open Qt txt for writing.");
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            fprintf(f, "%d%s", Qt[i][j], (j == 7) ? "" : " ");
        }
        fprintf(f, "\n");
    }
    fclose(f);
}

static int round_up_to_8(int x) {
    return (x + 7) / 8 * 8;
}

// Get pixel from plane with edge-replication padding
static float get_padded_levelshift(const uint8_t* plane, int w, int h, int x, int y) {
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= w) x = w - 1;
    if (y >= h) y = h - 1;
    return (float)plane[y * w + x] - 128.0f;
}

// Precompute cos table for DCT
static void build_cos_table(float c[8][8]) {
    for (int u = 0; u < 8; u++) {
        for (int x = 0; x < 8; x++) {
            c[u][x] = (float)cos(((2.0 * x + 1.0) * u * M_PI) / 16.0);
        }
    }
}

static float alpha(int u) {
    return (u == 0) ? (1.0f / (float)sqrt(2.0)) : 1.0f;
}

// Forward DCT on 8x8 block (input float block[8][8]) -> out float dct[8][8]
static void fdct8x8(const float in[8][8], float out[8][8], const float cosT[8][8]) {
    for (int u = 0; u < 8; u++) {
        for (int v = 0; v < 8; v++) {
            float sum = 0.0f;
            for (int x = 0; x < 8; x++) {
                for (int y = 0; y < 8; y++) {
                    sum += in[y][x] * cosT[u][x] * cosT[v][y];
                }
            }
            out[v][u] = 0.25f * alpha(u) * alpha(v) * sum;
        }
    }
}

// Write float32 / int16 raw in block order
static void write_block_raw_f32(FILE* f, const float blk[8][8]) {
    // row-major (y then x)
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            float v = blk[y][x];
            fwrite(&v, sizeof(float), 1, f);
        }
    }
}

static void write_block_raw_i16(FILE* f, const int16_t blk[8][8]) {
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            int16_t v = blk[y][x];
            fwrite(&v, sizeof(int16_t), 1, f);
        }
    }
}

static int16_t quantize_coeff(float c, int q) {
    // round to nearest
    float v = c / (float)q;
    int iv = (int)lrintf(v);
    iv = clamp_i16(iv);
    return (int16_t)iv;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage:\n");
        fprintf(stderr, "  %s 0 input.bmp R.txt G.txt B.txt dim.txt\n", argv[0]);
        fprintf(stderr, "  %s 1 input.bmp Qt_Y.txt Qt_Cb.txt Qt_Cr.txt dim.txt qF_Y.raw qF_Cb.raw qF_Cr.raw eF_Y.raw eF_Cb.raw eF_Cr.raw\n", argv[0]);
        return 1;
    }

    // -----------------------
    // Mode 0 
    // -----------------------
    if (strcmp(argv[1], "0") == 0) {
        if (argc != 7) {
            fprintf(stderr, "Usage: %s 0 input.bmp R.txt G.txt B.txt dim.txt\n", argv[0]);
            return 1;
        }
        const char* in_bmp = argv[2];
        const char* out_r  = argv[3];
        const char* out_g  = argv[4];
        const char* out_b  = argv[5];
        const char* out_dim= argv[6];

        int w, h;
        uint8_t *R, *G, *B;
        read_bmp24_planar_rgb(in_bmp, &w, &h, &R, &G, &B);

        write_channel_txt_u8(out_r, R, w, h);
        write_channel_txt_u8(out_g, G, w, h);
        write_channel_txt_u8(out_b, B, w, h);
        write_dim_txt(out_dim, w, h);

        free(R); free(G); free(B);
        printf("encoder 0 done\n");
        return 0;
    }

    // -----------------------
    // Mode 1 
    // -----------------------
    if (strcmp(argv[1], "1") == 0) {
        if (argc != 13) {
            fprintf(stderr,
                "Usage: %s 1 input.bmp Qt_Y.txt Qt_Cb.txt Qt_Cr.txt dim.txt qF_Y.raw qF_Cb.raw qF_Cr.raw eF_Y.raw eF_Cb.raw eF_Cr.raw\n",
                argv[0]
            );
            return 1;
        }

        const char* in_bmp  = argv[2];
        const char* qtY_txt = argv[3];
        const char* qtCb_txt= argv[4];
        const char* qtCr_txt= argv[5];
        const char* dim_txt = argv[6];

        const char* qFY_raw = argv[7];
        const char* qFCb_raw= argv[8];
        const char* qFCr_raw= argv[9];

        const char* eFY_raw = argv[10];
        const char* eFCb_raw= argv[11];
        const char* eFCr_raw= argv[12];

        // Standard JPEG quant tables
        static const int QtY[8][8] = {
            {16,11,10,16,24,40,51,61},
            {12,12,14,19,26,58,60,55},
            {14,13,16,24,40,57,69,56},
            {14,17,22,29,51,87,80,62},
            {18,22,37,56,68,109,103,77},
            {24,35,55,64,81,104,113,92},
            {49,64,78,87,103,121,120,101},
            {72,92,95,98,112,100,103,99}
        };

        static const int QtC[8][8] = {
            {17,18,24,47,99,99,99,99},
            {18,21,26,66,99,99,99,99},
            {24,26,56,99,99,99,99,99},
            {47,66,99,99,99,99,99,99},
            {99,99,99,99,99,99,99,99},
            {99,99,99,99,99,99,99,99},
            {99,99,99,99,99,99,99,99},
            {99,99,99,99,99,99,99,99}
        };

        // Read BMP
        int w, h;
        uint8_t *R, *G, *B;
        read_bmp24_planar_rgb(in_bmp, &w, &h, &R, &G, &B);

        // Convert to YCbCr planes
        int n = w * h;
        uint8_t *Y  = (uint8_t*)malloc((size_t)n);
        uint8_t *Cb = (uint8_t*)malloc((size_t)n);
        uint8_t *Cr = (uint8_t*)malloc((size_t)n);
        if (!Y || !Cb || !Cr) die("Out of memory.");

        for (int i = 0; i < n; i++) {
            rgb_to_ycbcr_u8(R[i], G[i], B[i], &Y[i], &Cb[i], &Cr[i]);
        }

        // Write Qt tables + dim
        write_qtable_txt(qtY_txt,  QtY);
        write_qtable_txt(qtCb_txt, QtC);
        write_qtable_txt(qtCr_txt, QtC);
        write_dim_txt(dim_txt, w, h);

        // Prepare DCT
        float cosT[8][8];
        build_cos_table(cosT);

        // Open outputs
        FILE* fqY  = fopen(qFY_raw, "wb");
        FILE* fqCb = fopen(qFCb_raw,"wb");
        FILE* fqCr = fopen(qFCr_raw,"wb");
        FILE* feY  = fopen(eFY_raw, "wb");
        FILE* feCb = fopen(eFCb_raw,"wb");
        FILE* feCr = fopen(eFCr_raw,"wb");
        if (!fqY || !fqCb || !fqCr || !feY || !feCb || !feCr) die("Cannot open output raw files.");

        int W8 = round_up_to_8(w);
        int H8 = round_up_to_8(h);

        // Process each channel
        // Helper macro to avoid repeating code 3 times
        #define PROCESS_CHANNEL(plane, Qt, fq, fe) do {                         \
            for (int by = 0; by < H8; by += 8) {                               \
                for (int bx = 0; bx < W8; bx += 8) {                           \
                    float blk[8][8];                                           \
                    float dct[8][8];                                           \
                    int16_t qblk[8][8];                                        \
                    for (int y0 = 0; y0 < 8; y0++) {                           \
                        for (int x0 = 0; x0 < 8; x0++) {                       \
                            int x = bx + x0;                                   \
                            int y = by + y0;                                   \
                            blk[y0][x0] = get_padded_levelshift((plane), w, h, x, y); \
                        }                                                       \
                    }                                                           \
                    fdct8x8(blk, dct, cosT);                                   \
                    for (int y0 = 0; y0 < 8; y0++) {                           \
                        for (int x0 = 0; x0 < 8; x0++) {                       \
                            qblk[y0][x0] = quantize_coeff(dct[y0][x0], (Qt)[y0][x0]); \
                        }                                                       \
                    }                                                           \
                    write_block_raw_f32((fe), dct);                             \
                    write_block_raw_i16((fq), qblk);                            \
                }                                                               \
            }                                                                   \
        } while(0)

        PROCESS_CHANNEL(Y,  QtY, fqY,  feY);
        PROCESS_CHANNEL(Cb, QtC, fqCb, feCb);
        PROCESS_CHANNEL(Cr, QtC, fqCr, feCr);

        #undef PROCESS_CHANNEL

        fclose(fqY); fclose(fqCb); fclose(fqCr);
        fclose(feY); fclose(feCb); fclose(feCr);

        free(R); free(G); free(B);
        free(Y); free(Cb); free(Cr);

        printf("encoder 1 done\n");
        return 0;
    }

    fprintf(stderr, "Unknown mode: %s\n", argv[1]);
    return 1;
}
