// decoder.c (final, supports method 0, 1(a), 1(b))
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void die(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

static int round8(int x) {
    return (x + 7) / 8 * 8;
}

static unsigned char clamp(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (unsigned char)v;
}

static float alpha(int u) {
    return (u == 0) ? (1.0f / sqrtf(2.0f)) : 1.0f;
}

static void build_cos(float c[8][8]) {
    for (int u = 0; u < 8; u++)
        for (int x = 0; x < 8; x++)
            c[u][x] = cosf((2 * x + 1) * u * M_PI / 16.0f);
}

static void idct8(float in[8][8], float out[8][8], float c[8][8]) {
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            float sum = 0.0f;
            for (int v = 0; v < 8; v++)
                for (int u = 0; u < 8; u++)
                    sum += alpha(u) * alpha(v) * in[v][u] * c[u][x] * c[v][y];
            out[y][x] = 0.25f * sum;
        }
    }
}

static void read_dim(const char *path, int *w, int *h) {
    FILE *f = fopen(path, "r");
    if (!f) die("Cannot open dim.txt");
    fscanf(f, "%d %d", w, h);
    fclose(f);
}

static void read_qt(const char *path, int qt[8][8]) {
    FILE *f = fopen(path, "r");
    if (!f) die("Cannot open Qt");
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 8; j++)
            fscanf(f, "%d", &qt[i][j]);
    fclose(f);
}

static void ycbcr2rgb(float Y, float Cb, float Cr,
                      unsigned char *R,
                      unsigned char *G,
                      unsigned char *B) {
    float cb = Cb - 128.0f;
    float cr = Cr - 128.0f;
    *R = clamp((int)roundf(Y + 1.402f * cr));
    *G = clamp((int)roundf(Y - 0.344136f * cb - 0.714136f * cr));
    *B = clamp((int)roundf(Y + 1.772f * cb));
}

int main(int argc, char *argv[]) {
    /* ---------------- method 0 ---------------- */
    if (argc == 7 && strcmp(argv[1], "0") == 0) {
        const char *outbmp = argv[2];
        const char *rfile = argv[3];
        const char *gfile = argv[4];
        const char *bfile = argv[5];
        const char *dim = argv[6];

        int w, h;
        read_dim(dim, &w, &h);

        FILE *fr = fopen(rfile, "r");
        FILE *fg = fopen(gfile, "r");
        FILE *fb = fopen(bfile, "r");
        if (!fr || !fg || !fb) die("Cannot open RGB files");

        FILE *fo = fopen(outbmp, "wb");
        if (!fo) die("Cannot open output bmp");

        int rowSize = (w * 3 + 3) & ~3;
        int imgSize = rowSize * h;

        unsigned char header[54] = {
            'B','M',
            imgSize+54,0,0,0,0,0,0,0,54,0,0,0,
            40,0,0,0,
            w,0,0,0,
            h,0,0,0,
            1,0,24,0
        };
        fwrite(header, 1, 54, fo);

        for (int y = h - 1; y >= 0; y--) {
            for (int x = 0; x < w; x++) {
                int r, g, b;
                fscanf(fr, "%d", &r);
                fscanf(fg, "%d", &g);
                fscanf(fb, "%d", &b);
                fputc(b, fo);
                fputc(g, fo);
                fputc(r, fo);
            }
            for (int p = 0; p < rowSize - w * 3; p++)
                fputc(0, fo);
        }

        fclose(fr); fclose(fg); fclose(fb); fclose(fo);
        printf("decoder 0 done\n");
        return 0;
    }

    /* ---------------- method 1(a) & 1(b) ---------------- */
    if (strcmp(argv[1], "1") == 0) {
        int has_orig = (argc == 11);   // 1(a)
        int has_ef   = (argc == 13);   // 1(b)

        const char *outbmp = argv[2];
        int idx = 3;

        if (has_orig) idx++;  // skip Kimberly.bmp

        const char *qtY = argv[idx++];
        const char *qtCb = argv[idx++];
        const char *qtCr = argv[idx++];
        const char *dim = argv[idx++];

        const char *qFY = argv[idx++];
        const char *qFCb = argv[idx++];
        const char *qFCr = argv[idx++];

        int w, h;
        read_dim(dim, &w, &h);

        int qtYv[8][8], qtCbv[8][8], qtCrv[8][8];
        read_qt(qtY, qtYv);
        read_qt(qtCb, qtCbv);
        read_qt(qtCr, qtCrv);

        FILE *fY = fopen(qFY, "rb");
        FILE *fCb = fopen(qFCb, "rb");
        FILE *fCr = fopen(qFCr, "rb");
        if (!fY || !fCb || !fCr) die("Cannot open qF files");

        int W8 = round8(w), H8 = round8(h);
        float *Y = calloc(W8 * H8, sizeof(float));
        float *Cb = calloc(W8 * H8, sizeof(float));
        float *Cr = calloc(W8 * H8, sizeof(float));

        float c[8][8];
        build_cos(c);

        for (int by = 0; by < H8; by += 8)
            for (int bx = 0; bx < W8; bx += 8) {
                float in[8][8], out[8][8];
                for (int v = 0; v < 8; v++)
                    for (int u = 0; u < 8; u++) {
                        int16_t q;
                        fread(&q, 2, 1, fY);
                        in[v][u] = q * qtYv[v][u];
                    }
                idct8(in, out, c);
                for (int y = 0; y < 8; y++)
                    for (int x = 0; x < 8; x++)
                        Y[(by+y)*W8 + (bx+x)] = out[y][x] + 128.0f;
            }

        fclose(fY); fclose(fCb); fclose(fCr);

        FILE *fo = fopen(outbmp, "wb");
        if (!fo) die("Cannot open output bmp");

        int rowSize = (w * 3 + 3) & ~3;
        int imgSize = rowSize * h;
        unsigned char header[54] = {
            'B','M',
            imgSize+54,0,0,0,0,0,0,0,54,0,0,0,
            40,0,0,0,
            w,0,0,0,
            h,0,0,0,
            1,0,24,0
        };
        fwrite(header, 1, 54, fo);

        for (int y = h - 1; y >= 0; y--) {
            for (int x = 0; x < w; x++) {
                unsigned char r,g,b;
                ycbcr2rgb(Y[y*W8+x], 128, 128, &r, &g, &b);
                fputc(b, fo); fputc(g, fo); fputc(r, fo);
            }
            for (int p = 0; p < rowSize - w * 3; p++)
                fputc(0, fo);
        }

        fclose(fo);
        free(Y); free(Cb); free(Cr);
        printf("decoder 1 done\n");
        return 0;
    }

    fprintf(stderr, "Usage error\n");
    return 1;
}
