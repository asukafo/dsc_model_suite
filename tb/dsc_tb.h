// DSC Testbench — shared types, logging, and utilities
#ifndef DSC_TB_H
#define DSC_TB_H

#include <stdint.h>
#include <stdio.h>
#include "vdo.h"
#include "dsc_types.h"

// ---- Test configuration ----
typedef struct {
    int   pic_w, pic_h;
    int   slice_w, slice_h;
    int   bpc;              // bits_per_component
    float bpp;              // bits_per_pixel (float)
    int   native_422;
    int   native_420;
    int   simple_422;
    int   convert_rgb;
    int   block_pred_enable;
    int   dsc_version_minor;
    int   linebuf_depth;
    int   full_ich_err_precision;

    // RC parameters (from cfg file, -1 = auto)
    int   rc_model_size;
    int   initial_offset;
    int   initial_xmit_delay;
} TbConfig;

// ---- Per-group statistics ----
#define MAX_GROUPS_PER_SLICE  16384
#define MAX_BUFFER_TRACE      65536

typedef struct {
    int   group_idx;
    int   qp;
    int   coded_bits;
    int   buffer_fullness;
    int   rc_range;
    int   ich_selected;
    int   flatness_detected;
} GroupStats;

typedef struct {
    int         num_groups;
    int         total_enc_bits;
    int         total_dec_bits;
    GroupStats *groups;       // array of size num_groups
    int         max_groups;

    // Aggregate stats
    double      avg_qp;
    int         qp_min, qp_max;
    int         qp_hist[32];
    int         ich_count;
    int         total_output_bytes;
} SlicePerfLog;

// ---- Log file format (for SystemC model comparison) ----
// CSV: group_idx, qp, coded_bits, buffer_fullness, rc_range

// ---- API ----
void       tb_config_default(TbConfig *c);
void       tb_config_print(const TbConfig *c);

SlicePerfLog *perf_log_create(int max_groups);
void          perf_log_record(SlicePerfLog *log, int qp, int coded_bits,
                              int buffer_fullness, int rc_range,
                              int ich, int flatness);
void          perf_log_finalize(SlicePerfLog *log);
void          perf_log_write_csv(SlicePerfLog *log, const char *filename);
void          perf_log_print_summary(SlicePerfLog *log);
void          perf_log_free(SlicePerfLog *log);

#endif // DSC_TB_H
