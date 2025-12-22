#ifndef COLOR_H
#define COLOR_H

#include <stdint.h>

// Convert one RGB pixel to YCbCr (JPEG-style)
// Input: R,G,B in [0,255]
// Output: Y,Cb,Cr in [0,255] (clamped)
void rgb_to_ycbcr_u8(uint8_t R, uint8_t G, uint8_t B,
                     uint8_t *Y, uint8_t *Cb, uint8_t *Cr);

// Convert a whole image (arrays length = width*height)
// RGB arrays are uint8_t per pixel
// Output Y/Cb/Cr arrays are uint8_t per pixel
void rgb_image_to_ycbcr(const uint8_t *R, const uint8_t *G, const uint8_t *B,
                        int width, int height,
                        uint8_t *Y, uint8_t *Cb, uint8_t *Cr);

#endif
