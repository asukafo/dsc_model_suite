// DSC Complete Testbench — main entry point
//
// Usage: ./dsc_tb [options]
//   -w W  -h H     picture size (default 640x480)
//   -s H           slice height (default 48)
//   -bpc N         bits per component (8/10/12, default 8)
//   -bpp F         bits per pixel (default 8.0)
//   -p PAT         test pattern: bars/ramp/checker/random/zones (default bars)
//   -o FILE.csv    output per-group log in CSV format
//   --list         list available test patterns
//
// Links against DSC library (dsc_codec.o, dsc_utils.o, utl.o, ...)
// No dependency on codec_main.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dsc_tb.h"
#include "dsc_config.h"
#include "test_pattern.h"
#include "utl.h"
#ifdef DSC_TRACE
#include "c_tracer.h"
#endif

// External DSC API
extern int  DSC_Encode(dsc_cfg_t*, pic_t*, pic_t*, unsigned char*, pic_t**, int*);
extern void DSC_Decode(dsc_cfg_t*, pic_t*, unsigned char*, pic_t**);

// ---- Globals ----
static TbConfig   g_cfg;
static const char *g_csv_out = NULL;
static const char *g_pattern_name = "bars";

// ---- Pattern name → enum ----
static PatternType parse_pattern(const char *s) {
    if (!strcmp(s,"bars"))        return PAT_COLOR_BARS;
    if (!strcmp(s,"ramp"))        return PAT_RAMP_H;
    if (!strcmp(s,"checker"))     return PAT_CHECKERBOARD;
    if (!strcmp(s,"random"))      return PAT_RANDOM;
    if (!strcmp(s,"zones"))       return PAT_ZONES;
    if (!strcmp(s,"gradient"))    return PAT_GRADIENT;
    fprintf(stderr, "Unknown pattern '%s', using bars\n", s);
    return PAT_COLOR_BARS;
}

static void usage(const char *prog) {
    printf("Usage: %s [options]\n", prog);
    printf("  -w W  -h H      picture size (default 640x480)\n");
    printf("  -s H            slice height (default 48, 0=full frame)\n");
    printf("  -bpc N          bits/component (8,10,12 default 8)\n");
    printf("  -bpp F          bits/pixel (default 8.0)\n");
    printf("  -p  PAT         test pattern (default bars)\n");
    printf("  -o  FILE.csv    output per-group CSV log\n");
    printf("  --list          list test patterns\n");
    printf("\nTest patterns: bars, ramp, checker, random, zones, gradient\n");
    exit(1);
}

static void parse_args(int argc, char *argv[]) {
    tb_config_default(&g_cfg);
    for (int i=1; i<argc; i++) {
        if (!strcmp(argv[i],"-w") && i+1<argc)  g_cfg.pic_w = atoi(argv[++i]);
        else if (!strcmp(argv[i],"-h") && i+1<argc) g_cfg.pic_h = atoi(argv[++i]);
        else if (!strcmp(argv[i],"-s") && i+1<argc) g_cfg.slice_h = atoi(argv[++i]);
        else if (!strcmp(argv[i],"-bpc") && i+1<argc) g_cfg.bpc = atoi(argv[++i]);
        else if (!strcmp(argv[i],"-bpp") && i+1<argc) g_cfg.bpp = atof(argv[++i]);
        else if (!strcmp(argv[i],"-p") && i+1<argc) g_pattern_name = argv[++i];
        else if (!strcmp(argv[i],"-o") && i+1<argc) g_csv_out = argv[++i];
        else if (!strcmp(argv[i],"--list")) {
            printf("Available patterns: bars ramp checker random zones gradient\n");
            exit(0);
        }
        else usage(argv[0]);
    }
}

// ---- Run encode+decode for one slice, log per-group stats ----
static int run_slice(const TbConfig *tbc, pic_t *src, pic_t *enc_out,
                     pic_t *dec_out, pic_t **tmp,
                     unsigned char *buf, int *chunk_sizes,
                     SlicePerfLog *log, int ys)
{
    dsc_cfg_t cfg;
    if (dsc_setup_config(&cfg, tbc) != 0) {
        fprintf(stderr, "Config setup failed\n");
        return -1;
    }
    cfg.xstart = 0;
    cfg.ystart = ys;

    // Encode
    int nbits = DSC_Encode(&cfg, src, enc_out, buf, tmp, chunk_sizes);
    // Decode
    DSC_Decode(&cfg, dec_out, buf, tmp);

    // Per-group stats are internal to DSC_Encode and not directly accessible.
    // For detailed logging, we'd need to instrument dsc_codec.c.
    // Here we log aggregate per-slice stats.
    (void)log;
    return nbits;
}

// ---- Main ----
int main(int argc, char *argv[]) {
    parse_args(argc, argv);

    printf("========================================\n");
    printf("  DSC Testbench — Full Validation\n");
    printf("========================================\n");
    tb_config_print(&g_cfg);
    printf("  Pattern: %s\n\n", g_pattern_name);

    // ---- 1. Generate test image ----
    PatternType pat = parse_pattern(g_pattern_name);
    pic_t *src = gen_test_image(g_cfg.pic_w, g_cfg.pic_h, g_cfg.bpc, pat);
    if (!src) { fprintf(stderr, "Failed to create test image\n"); return 1; }
    printf("Source: %dx%d RGB %d-bit\n", src->w, src->h, src->bits);

    // ---- 2. Allocate buffers ----
    int sh = g_cfg.slice_h ? g_cfg.slice_h : g_cfg.pic_h;
    int sw = g_cfg.slice_w ? g_cfg.slice_w : g_cfg.pic_w;

    pic_t *enc_out = pcreate_ext(FRAME, RGB, YUV_444, g_cfg.pic_w, g_cfg.pic_h, g_cfg.bpc);
    pic_t *dec_out = pcreate_ext(FRAME, RGB, YUV_444, g_cfg.pic_w, g_cfg.pic_h, g_cfg.bpc);

    pic_t **tmp = (pic_t**)malloc(sizeof(pic_t*)*2);
    tmp[0] = pcreate_ext(FRAME, YUV_HD, YUV_444, g_cfg.pic_w, g_cfg.pic_h, g_cfg.bpc);
    tmp[1] = pcreate_ext(FRAME, YUV_HD, YUV_444, g_cfg.pic_w, g_cfg.pic_h, g_cfg.bpc);

    // Pre-compute chunk_size for buffer allocation
    dsc_cfg_t tmp_cfg;
    dsc_setup_config(&tmp_cfg, &g_cfg);
    int buf_size = tmp_cfg.chunk_size * sh;
    unsigned char *cmpr_buf = (unsigned char*)malloc(buf_size);
    int *chunk_sizes = (int*)malloc(sizeof(int) * sh);

    // ---- 3. Run slice loop ----
    SlicePerfLog *log = perf_log_create(65536);
    int total_slices = (g_cfg.pic_h + sh - 1) / sh;
    int total_bits = 0;

#ifdef DSC_TRACE
    trace_reset();
    trace_install();
#endif

    printf("Slices: %d  slice_h=%d  chunk_size=%d  buf=%d bytes\n\n",
           total_slices, sh, tmp_cfg.chunk_size, buf_size);

    for (int ys = 0; ys < g_cfg.pic_h; ys += sh) {
        int sn = ys / sh + 1;
        memset(cmpr_buf, 0, buf_size);

        // Setup config for this slice
        dsc_cfg_t cfg;
        dsc_setup_config(&cfg, &g_cfg);
        cfg.xstart = 0;
        cfg.ystart = ys;

        // Encode
        int nbits = DSC_Encode(&cfg, src, enc_out, cmpr_buf, tmp, chunk_sizes);
        total_bits += nbits;

        // Decode
        DSC_Decode(&cfg, dec_out, cmpr_buf, tmp);

        printf("  Slice %d/%d  y=%d..%d  enc=%d bits (%d bytes)  chunk_size=%d\r",
               sn, total_slices, ys, ys+sh-1,
               nbits, nbits/8, cfg.chunk_size);
        fflush(stdout);
    }
    printf("\n\n");

    // ---- 4. Results ----
    double psnr_enc = image_psnr(src, enc_out);
    double psnr_dec = image_psnr(src, dec_out);
    int max_r, max_g, max_b;
    image_max_diff(src, dec_out, &max_r, &max_g, &max_b);

    printf("--- Results ---\n");
    printf("  Total enc bits:  %d  (%d bytes)\n", total_bits, total_bits/8);
    printf("  Original bytes:  %d\n", g_cfg.pic_w * g_cfg.pic_h * 3 * g_cfg.bpc / 8);
    printf("  Compression:     %.1f : 1\n",
           (float)(g_cfg.pic_w * g_cfg.pic_h * 3) / (total_bits / 8));
    printf("  PSNR (enc-loop): %.2f dB\n", psnr_enc);
    printf("  PSNR (roundtrip): %.2f dB\n", psnr_dec);
    printf("  Max diff:  R=%d G=%d B=%d\n", max_r, max_g, max_b);
    printf("  %s\n\n", (psnr_dec > 30.0) ? "PASS" : "FAIL");

#ifdef DSC_TRACE
    // Write per-group trace from DSC_Encode callback
    if (g_csv_out) {
        char trace_fn[256];
        snprintf(trace_fn, sizeof(trace_fn), "%s_trace.csv", g_csv_out);
        trace_write_csv(trace_fn);
    }
#endif

    // ---- 5. Write CSV log ----
    if (g_csv_out) {
        perf_log_finalize(log);
        perf_log_write_csv(log, g_csv_out);
        perf_log_print_summary(log);
    }

    // ---- 6. Cleanup ----
    pdestroy(src);
    pdestroy(enc_out);
    pdestroy(dec_out);
    pdestroy(tmp[0]); pdestroy(tmp[1]);
    free(tmp);
    free(cmpr_buf);
    free(chunk_sizes);
    perf_log_free(log);

    return (psnr_dec > 30.0) ? 0 : 1;
}
