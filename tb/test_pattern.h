// Test pattern generators for DSC testbench
#ifndef TEST_PATTERN_H
#define TEST_PATTERN_H
#include "vdo.h"

typedef enum {
    PAT_COLOR_BARS,
    PAT_RAMP_H,
    PAT_RAMP_V,
    PAT_CHECKERBOARD,
    PAT_RANDOM,
    PAT_ZONES,        // flat zones with sharp edges (stress test)
    PAT_GRADIENT,
} PatternType;

// Generate a test image. Caller must pdestroy() the result.
pic_t *gen_test_image(int w, int h, int bits, PatternType pat);

// Fill an existing pic_t with test data
void   fill_test_image(pic_t *p, PatternType pat);

// Compare two images, return PSNR and max absolute difference
double image_psnr(pic_t *a, pic_t *b);
int    image_max_diff(pic_t *a, pic_t *b, int *max_r, int *max_g, int *max_b);

#endif
