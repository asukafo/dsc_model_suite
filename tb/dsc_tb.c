/***************************************************************************
 * DSC Model Testbench
 *
 * Standalone test that exercises DSC_Encode / DSC_Decode programmatically.
 * Mimics the slice-loop pattern from codec_main.c:
 *   for each vertical slice:  set xstart/ystart → Encode → Decode
 *
 * Generates a synthetic RGB image, round-trips it, and checks PSNR.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "vdo.h"
#include "dsc_types.h"
#include "dsc_utils.h"
#include "dsc_codec.h"
#include "utl.h"

/* ------------------------------------------------------------------ */
/*  Mini RC-parameter helpers  (derived from codec_main.c logic)       */
/* ------------------------------------------------------------------ */

#define OFFSET_FRACTIONAL_BITS  11

static int compute_offset(dsc_cfg_t *cfg, int pixelsPerGroup,
                          int groupsPerLine, int grpcnt,
                          int initialDelay, float bitsPerPixel,
                          int firstLineBpgOfs, int secondLineBpgOfs,
                          int native420)
{
    int offset = 0;
    int grpcnt_id = (int)ceil((float)initialDelay / pixelsPerGroup);

    if (grpcnt <= grpcnt_id)
        offset = (int)ceil(grpcnt * pixelsPerGroup * bitsPerPixel);
    else
        offset = (int)ceil(grpcnt_id * pixelsPerGroup * bitsPerPixel)
                 - (((grpcnt - grpcnt_id) * cfg->slice_bpg_offset)
                    >> OFFSET_FRACTIONAL_BITS);

    if (grpcnt <= groupsPerLine)
        offset += grpcnt * firstLineBpgOfs;
    else
        offset += groupsPerLine * firstLineBpgOfs
                  - (((grpcnt - groupsPerLine) * cfg->nfl_bpg_offset)
                     >> OFFSET_FRACTIONAL_BITS);

    if (native420) {
        if (grpcnt <= groupsPerLine)
            offset -= (grpcnt * cfg->nsl_bpg_offset) >> OFFSET_FRACTIONAL_BITS;
        else if (grpcnt <= 2 * groupsPerLine)
            offset += (grpcnt - groupsPerLine) * secondLineBpgOfs
                      - ((groupsPerLine * cfg->nsl_bpg_offset)
                         >> OFFSET_FRACTIONAL_BITS);
        else
            offset += (grpcnt - groupsPerLine) * secondLineBpgOfs
                      - (((grpcnt - groupsPerLine) * cfg->nsl_bpg_offset)
                         >> OFFSET_FRACTIONAL_BITS);
    }
    return offset;
}

static int compute_rc_params(dsc_cfg_t *cfg, int initialDelay,
                             float bitsPerPixelF, int initialFullnessOfs,
                             int bitsPerComponent, int useYuvInput,
                             int *firstLineBpgOfsPtr, int *secondLineBpgOfsPtr)
{
    int groupsPerLine, num_extra_mux_bits, sliceBits;
    int final_value, final_scale, groups_total;
    int maxOffset, rbsMin, hrdDelay, slicew;
    int uncompressedBpgRate;
    int pixelsPerGroup = 3;
    int firstLineBpgOfs = *firstLineBpgOfsPtr;
    int secondLineBpgOfs = *secondLineBpgOfsPtr;
    int native420 = cfg->native_420;
    int native422 = cfg->native_422;

    slicew = cfg->slice_width >> (native420 || native422);
    if (native422)
        uncompressedBpgRate = 3 * bitsPerComponent * 4;
    else
        uncompressedBpgRate = (3 * bitsPerComponent + (useYuvInput ? 0 : 2)) * 3;

    if (firstLineBpgOfs < 0) {
        if (cfg->slice_height >= 8)
            firstLineBpgOfs = 12 + (int)(0.09 * (cfg->slice_height - 8 < 34 ? cfg->slice_height - 8 : 34));
        else
            firstLineBpgOfs = 2 * (cfg->slice_height - 1);
        if (firstLineBpgOfs < 0) firstLineBpgOfs = 0;
        if (firstLineBpgOfs > (int)(uncompressedBpgRate - 3 * bitsPerPixelF))
            firstLineBpgOfs = (int)(uncompressedBpgRate - 3 * bitsPerPixelF);
    }
    if (secondLineBpgOfs < 0) {
        secondLineBpgOfs = native420 ? 12 : 0;
        if (secondLineBpgOfs < 0) secondLineBpgOfs = 0;
        if (secondLineBpgOfs > (int)(uncompressedBpgRate - 3 * bitsPerPixelF))
            secondLineBpgOfs = (int)(uncompressedBpgRate - 3 * bitsPerPixelF);
    }
    cfg->first_line_bpg_ofs = firstLineBpgOfs;
    cfg->second_line_bpg_ofs = secondLineBpgOfs;
    *firstLineBpgOfsPtr = firstLineBpgOfs;
    *secondLineBpgOfsPtr = secondLineBpgOfs;

    groupsPerLine = (slicew + pixelsPerGroup - 1) / pixelsPerGroup;
    cfg->chunk_size = (int)(ceil(slicew * bitsPerPixelF / 8.0));

    if (cfg->convert_rgb)
        num_extra_mux_bits = (3 * (cfg->mux_word_size + (4 * bitsPerComponent + 4) - 2));
    else if (!cfg->native_422)
        num_extra_mux_bits = (3 * cfg->mux_word_size + (4 * bitsPerComponent + 4) + 2 * (4 * bitsPerComponent) - 2);
    else
        num_extra_mux_bits = (4 * cfg->mux_word_size + (4 * bitsPerComponent + 4) + 3 * (4 * bitsPerComponent) - 2);

    sliceBits = 8 * cfg->chunk_size * cfg->slice_height;
    while ((num_extra_mux_bits > 0) && ((sliceBits - num_extra_mux_bits) % cfg->mux_word_size))
        num_extra_mux_bits--;

    cfg->initial_scale_value = 8 * cfg->rc_model_size / (cfg->rc_model_size - cfg->initial_offset);
    if (groupsPerLine < cfg->initial_scale_value - 8)
        cfg->initial_scale_value = groupsPerLine + 8;

    if (cfg->initial_scale_value > 8)
        cfg->scale_decrement_interval = groupsPerLine / (cfg->initial_scale_value - 8);
    else
        cfg->scale_decrement_interval = 4095;

    final_value = cfg->rc_model_size
                  - ((cfg->initial_xmit_delay * cfg->bits_per_pixel + 8) >> 4)
                  + num_extra_mux_bits;
    cfg->final_offset = final_value;
    if (final_value >= cfg->rc_model_size) {
        fprintf(stderr, "ERROR: final_offset >= rc_model_size (try increasing initial_xmit_delay)\n");
        return 1;
    }
    final_scale = 8 * cfg->rc_model_size / (cfg->rc_model_size - final_value);
    if (cfg->slice_height > 1)
        cfg->nfl_bpg_offset = (int)ceil((double)(cfg->first_line_bpg_ofs << OFFSET_FRACTIONAL_BITS) / (cfg->slice_height - 1));
    else
        cfg->nfl_bpg_offset = 0;
    if (cfg->slice_height > 2)
        cfg->nsl_bpg_offset = (int)ceil((double)(cfg->second_line_bpg_ofs << OFFSET_FRACTIONAL_BITS) / (cfg->slice_height - 1));
    else
        cfg->nsl_bpg_offset = 0;

    groups_total = groupsPerLine * cfg->slice_height;
    cfg->slice_bpg_offset = (int)ceil((double)(1 << OFFSET_FRACTIONAL_BITS) *
                                      (cfg->rc_model_size - cfg->initial_offset + num_extra_mux_bits) / groups_total);

    if (final_scale > 9)
        cfg->scale_increment_interval = (int)((double)(1 << OFFSET_FRACTIONAL_BITS) * cfg->final_offset /
                                              ((double)(final_scale - 9) * (cfg->nfl_bpg_offset + cfg->slice_bpg_offset + cfg->nsl_bpg_offset)));
    else
        cfg->scale_increment_interval = 0;

    maxOffset = compute_offset(cfg, pixelsPerGroup, groupsPerLine,
                               (int)(ceil((float)initialDelay / pixelsPerGroup)),
                               initialDelay, bitsPerPixelF,
                               firstLineBpgOfs, secondLineBpgOfs, native420);
    maxOffset = maxOffset > compute_offset(cfg, pixelsPerGroup, groupsPerLine,
                                           groupsPerLine, initialDelay,
                                           bitsPerPixelF, firstLineBpgOfs,
                                           secondLineBpgOfs, native420)
                    ? maxOffset
                    : compute_offset(cfg, pixelsPerGroup, groupsPerLine,
                                     groupsPerLine, initialDelay,
                                     bitsPerPixelF, firstLineBpgOfs,
                                     secondLineBpgOfs, native420);
    maxOffset = maxOffset > compute_offset(cfg, pixelsPerGroup, groupsPerLine,
                                           2 * groupsPerLine, initialDelay,
                                           bitsPerPixelF, firstLineBpgOfs,
                                           secondLineBpgOfs, native420)
                    ? maxOffset
                    : compute_offset(cfg, pixelsPerGroup, groupsPerLine,
                                     2 * groupsPerLine, initialDelay,
                                     bitsPerPixelF, firstLineBpgOfs,
                                     secondLineBpgOfs, native420);

    rbsMin = cfg->rc_model_size - initialFullnessOfs + maxOffset;
    hrdDelay = (int)(ceil((double)rbsMin / bitsPerPixelF));
    cfg->rcb_bits = (int)(ceil((double)hrdDelay * bitsPerPixelF));
    cfg->initial_dec_delay = hrdDelay - cfg->initial_xmit_delay;

    return 0;
}

/* ------------------------------------------------------------------ */
/*  Fill dsc_cfg_t with a sensible 8bpc / 8bpp  RGB-4:4:4  config    */
/* ------------------------------------------------------------------ */
static void setup_dsc_config(dsc_cfg_t *cfg, int pic_w, int pic_h,
                             int slice_w, int slice_h)
{
    memset(cfg, 0, sizeof(*cfg));

    /* ---- basic geometry ---- */
    cfg->pic_width    = pic_w;
    cfg->pic_height   = pic_h;
    cfg->slice_width  = slice_w ? slice_w : pic_w;
    cfg->slice_height = slice_h ? slice_h : pic_h;

    /* ---- pixel format ---- */
    cfg->bits_per_component = 8;
    cfg->bits_per_pixel     = 8 * 16;   /* 8 bpp  → ×16 fixed-point */
    cfg->convert_rgb        = 1;        /* RGB → YCoCg internally    */
    cfg->simple_422         = 0;
    cfg->native_422         = 0;
    cfg->native_420         = 0;
    cfg->linebuf_depth      = 9;        /* ≥ bpc+1                   */
    cfg->block_pred_enable  = 1;
    cfg->dsc_version_minor  = 2;
    cfg->vbr_enable         = 0;
    cfg->mux_word_size      = 48;       /* bpc≤10 → 48               */
    cfg->full_ich_err_precision = 0;
    cfg->second_line_ofs_adj   = 0;

    /* ---- RC model ---- */
    cfg->rc_model_size      = 8192;
    cfg->initial_offset     = 6144;
    cfg->initial_xmit_delay = 512;

    cfg->rc_edge_factor       = 6;
    cfg->rc_quant_incr_limit0 = 11;
    cfg->rc_quant_incr_limit1 = 11;
    cfg->rc_tgt_offset_hi     = 3;
    cfg->rc_tgt_offset_lo     = 3;

    /* ---- flatness ---- */
    cfg->flatness_min_qp     = 3;
    cfg->flatness_max_qp     = 12;
    cfg->flatness_det_thresh = 2;
    cfg->very_flat_qp            = 1;
    cfg->somewhat_flat_qp_delta  = 4;
    cfg->somewhat_flat_qp_thresh = 7;

    /* ---- RC ranges (from rc_8bpc_8bpp.cfg) ---- */
    int offsets[]   = { 2, 0, 0, -2, -4, -6, -8, -8, -8, -10, -10, -12, -12, -12, -12 };
    int minqps[]    = { 0, 0, 1, 1, 3, 3, 3, 3, 3, 3, 5, 5, 5, 9, 12 };
    int maxqps[]    = { 4, 4, 5, 6, 7, 7, 7, 8, 9, 10, 10, 11, 11, 12, 13 };
    int thresholds[] = { 896, 1792, 2688, 3584, 4480, 5376, 6272, 6720, 7168, 7616, 7744, 7872, 8000, 8064 };

    for (int i = 0; i < 15; i++) {
        cfg->rc_range_parameters[i].range_bpg_offset = offsets[i];
        cfg->rc_range_parameters[i].range_min_qp     = minqps[i];
        cfg->rc_range_parameters[i].range_max_qp     = maxqps[i];
    }
    for (int i = 0; i < 14; i++)
        cfg->rc_buf_thresh[i] = thresholds[i];

    /* ---- derived parameters ---- */
    int flb = -1, slb = -1;
    compute_rc_params(cfg, 512, 8.0f, 6144, 8, 0, &flb, &slb);
}

/* ------------------------------------------------------------------ */
/*  Simple PSNR  (RGB, 0–255)                                        */
/* ------------------------------------------------------------------ */
static double compute_psnr(pic_t *a, pic_t *b)
{
    double mse = 0.0;
    int w = a->w, h = a->h, n = 0;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            double dr = a->data.rgb.r[y][x] - b->data.rgb.r[y][x];
            double dg = a->data.rgb.g[y][x] - b->data.rgb.g[y][x];
            double db = a->data.rgb.b[y][x] - b->data.rgb.b[y][x];
            mse += dr * dr + dg * dg + db * db;
            n += 3;
        }
    }
    mse /= n;
    return mse > 0 ? 10.0 * log10(255.0 * 255.0 / mse) : 99.0;
}

/* ------------------------------------------------------------------ */
/*  Generate a simple colour-bar test image (RGB 4:4:4)               */
/* ------------------------------------------------------------------ */
static pic_t *make_test_image(int w, int h)
{
    pic_t *p = pcreate_ext(FRAME, RGB, YUV_444, w, h, 8);
    int colours[8][3] = {
        {255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 0},
        {0, 255, 255}, {255, 0, 255}, {255, 255, 255}, {0, 0, 0},
    };
    int bar_w = w / 8;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int bar = (x / bar_w) % 8;
            if (bar >= 8) bar = 7;
            p->data.rgb.r[y][x] = colours[bar][0];
            p->data.rgb.g[y][x] = colours[bar][1];
            p->data.rgb.b[y][x] = colours[bar][2];
        }
    }
    return p;
}

/* ------------------------------------------------------------------ */
/*  main                                                              */
/* ------------------------------------------------------------------ */
int main(void)
{
    printf("=== DSC Model Testbench ===\n\n");

    /* ---- 1.  Parameters ---- */
    int pic_w = 640;
    int pic_h = 480;
    int slice_h = 48;

    /* ---- 2.  Setup config ---- */
    dsc_cfg_t cfg;
    setup_dsc_config(&cfg, pic_w, pic_h, 0, slice_h);

    printf("Config:  %dx%d,  slice_h=%d,  %dbpc / %dbpp\n",
           pic_w, pic_h, slice_h,
           cfg.bits_per_component, cfg.bits_per_pixel / 16);
    printf("  chunk_size       = %d bytes\n", cfg.chunk_size);
    printf("  rcb_bits          = %d\n", cfg.rcb_bits);
    printf("  initial_scale_val = %d\n", cfg.initial_scale_value);
    printf("  scale_dec_interval= %d\n", cfg.scale_decrement_interval);

    /* ---- 3.  Create test image ---- */
    pic_t *src = make_test_image(pic_w, pic_h);
    printf("\nSource image:  %dx%d  RGB 8-bit\n", src->w, src->h);

    /* ---- 4.  Allocate temp pictures (×2 for YCoCg conversion) ---- */
    pic_t **temp_pic = (pic_t **)malloc(sizeof(pic_t *) * 2);
    temp_pic[0] = pcreate_ext(FRAME, YUV_HD, YUV_444, pic_w, pic_h, cfg.bits_per_component);
    temp_pic[1] = pcreate_ext(FRAME, YUV_HD, YUV_444, pic_w, pic_h, cfg.bits_per_component);
    temp_pic[0]->alpha = 0;
    temp_pic[1]->alpha = 0;

    /* ---- 5.  Allocate output pictures ---- */
    pic_t *enc_out = pcreate_ext(FRAME, RGB, YUV_444, pic_w, pic_h, 8);
    pic_t *dec_out = pcreate_ext(FRAME, RGB, YUV_444, pic_w, pic_h, 8);
    pcopy_header(src, enc_out);
    pcopy_header(src, dec_out);

    /* ---- 6.  Allocate per-slice compressed buffer ---- */
    int buf_size = cfg.chunk_size * cfg.slice_height;
    unsigned char *cmpr_buf = (unsigned char *)malloc(buf_size);
    int *chunk_sizes = (int *)malloc(sizeof(int) * cfg.slice_height);

    /* ---- 7.  Slice loop: encode → decode  (FUNCTION=0 pattern) ---- */
    int slice_total = (pic_h + slice_h - 1) / slice_h;
    int total_enc_bytes = 0;
    int slice_n = 0;

    for (int ys = 0; ys < pic_h; ys += slice_h) {
        slice_n++;
        memset(cmpr_buf, 0, buf_size);

        cfg.xstart = 0;
        cfg.ystart = ys;

        /* -- encode this slice (returns BITS, not bytes) -- */
        int nbits = DSC_Encode(&cfg, src, enc_out, cmpr_buf, temp_pic, chunk_sizes);
        total_enc_bytes += nbits / 8;

        /* -- decode this slice (reuses same buffer) -- */
        DSC_Decode(&cfg, dec_out, cmpr_buf, temp_pic);

        printf("  Slice %d/%d:  y=%3d..%-3d  enc=%d bits (%d bytes)\r",
               slice_n, slice_total, ys, ys + slice_h - 1, nbits, nbits / 8);
        fflush(stdout);
    }
    printf("\n\nTotal encoded:  %d bytes  (ratio %.1f : 1)\n",
           total_enc_bytes, (float)(pic_w * pic_h * 3) / total_enc_bytes);

    /* ---- 8.  PSNR ---- */
    double psnr_enc = compute_psnr(src, enc_out);
    double psnr_dec = compute_psnr(src, dec_out);
    printf("\nPSNR (encode-loop):  %.2f dB\n", psnr_enc);
    printf("PSNR (decode):       %.2f dB\n", psnr_dec);

    /* ---- 9.  Pixel-level diff check ---- */
    int mismatches = 0;
    for (int y = 0; y < pic_h && mismatches < 10; y++) {
        for (int x = 0; x < pic_w && mismatches < 10; x++) {
            if (src->data.rgb.r[y][x] != dec_out->data.rgb.r[y][x] ||
                src->data.rgb.g[y][x] != dec_out->data.rgb.g[y][x] ||
                src->data.rgb.b[y][x] != dec_out->data.rgb.b[y][x]) {
                printf("  MISMATCH at (%d,%d):  src=(%d,%d,%d)  dec=(%d,%d,%d)\n",
                       x, y,
                       src->data.rgb.r[y][x],
                       src->data.rgb.g[y][x],
                       src->data.rgb.b[y][x],
                       dec_out->data.rgb.r[y][x],
                       dec_out->data.rgb.g[y][x],
                       dec_out->data.rgb.b[y][x]);
                mismatches++;
            }
        }
    }
    if (mismatches == 0)
        printf("Pixel match:  PERFECT (0 mismatches)\n");
    else
        printf("Pixel match:  %d mismatches\n", mismatches);

    /* ---- 10. Cleanup ---- */
    pdestroy(src);
    pdestroy(enc_out);
    pdestroy(dec_out);
    pdestroy(temp_pic[0]);
    pdestroy(temp_pic[1]);
    free(temp_pic);
    free(cmpr_buf);
    free(chunk_sizes);

    printf("\n=== %s ===\n", (psnr_dec > 30.0) ? "PASS" : "FAIL");
    return (psnr_dec > 30.0) ? 0 : 1;
}
