#include "color.h"

// clamp int to [0,255]
static uint8_t clamp_u8(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (uint8_t)v;
}

void rgb_to_ycbcr_u8(uint8_t R, uint8_t G, uint8_t B,
                     uint8_t *Y, uint8_t *Cb, uint8_t *Cr)
{
    // JPEG-style full-range conversion (common)
    // Y  =  0.299R + 0.587G + 0.114B
    // Cb = -0.168736R -0.331264G +0.5B + 128
    // Cr =  0.5R -0.418688G -0.081312B + 128
    // Use integer-friendly rounding
    int y  = (int)( 0.299    * R + 0.587    * G + 0.114    * B + 0.5);
    int cb = (int)(-0.168736* R - 0.331264* G + 0.5      * B + 128.0 + 0.5);
    int cr = (int)( 0.5      * R - 0.418688* G - 0.081312* B + 128.0 + 0.5);

    *Y  = clamp_u8(y);
    *Cb = clamp_u8(cb);
    *Cr = clamp_u8(cr);
}

void rgb_image_to_ycbcr(const uint8_t *R, const uint8_t *G, const uint8_t *B,
                        int width, int height,
                        uint8_t *Y, uint8_t *Cb, uint8_t *Cr)
{
    int n = width * height;
    for (int i = 0; i < n; i++) {
        rgb_to_ycbcr_u8(R[i], G[i], B[i], &Y[i], &Cb[i], &Cr[i]);
    }
}
