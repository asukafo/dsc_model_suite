// DSC SystemC Performance Model — Configuration
// Wraps the C-model dsc_cfg_t for SystemC use.

#ifndef DSC_CONFIG_H
#define DSC_CONFIG_H

struct DSCConfig {
    // ---- geometry ----
    int pic_width;
    int pic_height;
    int slice_width;
    int slice_height;
    int xstart;
    int ystart;

    // ---- pixel format ----
    int bits_per_component;
    int bits_per_pixel;       // << 4 fixed-point (e.g., 8bpp = 128)
    int convert_rgb;
    int simple_422;
    int native_422;
    int native_420;
    int linebuf_depth;
    int block_pred_enable;
    int dsc_version_minor;
    int vbr_enable;
    int mux_word_size;
    int full_ich_err_precision;
    int second_line_ofs_adj;

    // ---- RC model ----
    int rc_model_size;
    int initial_offset;
    int initial_xmit_delay;
    int initial_dec_delay;
    int final_offset;

    int rc_edge_factor;
    int rc_quant_incr_limit0;
    int rc_quant_incr_limit1;
    int rc_tgt_offset_hi;
    int rc_tgt_offset_lo;

    // ---- flatness ----
    int flatness_min_qp;
    int flatness_max_qp;
    int flatness_det_thresh;
    int very_flat_qp;
    int somewhat_flat_qp_delta;
    int somewhat_flat_qp_thresh;

    // ---- RC ranges (15) ----
    int range_bpg_offset[15];
    int range_min_qp[15];
    int range_max_qp[15];
    int rc_buf_thresh[14];

    // ---- derived ----
    int chunk_size;
    int rcb_bits;
    int first_line_bpg_ofs;
    int second_line_bpg_ofs;
    int nfl_bpg_offset;
    int nsl_bpg_offset;
    int slice_bpg_offset;
    int initial_scale_value;
    int scale_decrement_interval;
    int scale_increment_interval;
    int num_ssps;
    int pixels_per_group;
    int num_slices;          // 1/2/4/8/12/16 — horizontal slices for HDMI
};

// Initialize with a standard 8bpc/8bpp RGB 4:4:4 config (from rc_8bpc_8bpp.cfg)
inline DSCConfig make_default_config(int pic_w = 640, int pic_h = 480, int slice_h = 48) {
    DSCConfig c = {};
    c.pic_width    = pic_w;
    c.pic_height   = pic_h;
    c.slice_width  = pic_w;
    c.slice_height = slice_h;
    c.xstart       = 0;
    c.ystart       = 0;

    c.bits_per_component = 8;
    c.bits_per_pixel     = 8 * 16;    // 8 bpp
    c.convert_rgb        = 1;
    c.simple_422         = 0;
    c.native_422         = 0;
    c.native_420         = 0;
    c.linebuf_depth      = 9;
    c.block_pred_enable  = 1;
    c.dsc_version_minor  = 2;
    c.vbr_enable         = 0;
    c.mux_word_size      = 48;
    c.full_ich_err_precision = 0;
    c.second_line_ofs_adj   = 0;

    c.rc_model_size      = 8192;
    c.initial_offset     = 6144;
    c.initial_xmit_delay = 512;
    c.initial_dec_delay  = 512;
    c.final_offset       = 2048;

    c.rc_edge_factor       = 6;
    c.rc_quant_incr_limit0 = 11;
    c.rc_quant_incr_limit1 = 11;
    c.rc_tgt_offset_hi     = 3;
    c.rc_tgt_offset_lo     = 3;

    c.flatness_min_qp     = 3;
    c.flatness_max_qp     = 12;
    c.flatness_det_thresh = 2;
    c.very_flat_qp            = 1;
    c.somewhat_flat_qp_delta  = 4;
    c.somewhat_flat_qp_thresh = 7;

    int offsets[]   = { 2, 0, 0, -2, -4, -6, -8, -8, -8, -10, -10, -12, -12, -12, -12 };
    int minqps[]    = { 0, 0, 1, 1, 3, 3, 3, 3, 3, 3, 5, 5, 5, 9, 12 };
    int maxqps[]    = { 4, 4, 5, 6, 7, 7, 7, 8, 9, 10, 10, 11, 11, 12, 13 };
    int thresholds[] = { 896, 1792, 2688, 3584, 4480, 5376, 6272, 6720, 7168, 7616, 7744, 7872, 8000, 8064 };
    for (int i = 0; i < 15; i++) {
        c.range_bpg_offset[i] = offsets[i];
        c.range_min_qp[i]     = minqps[i];
        c.range_max_qp[i]     = maxqps[i];
    }
    for (int i = 0; i < 14; i++)
        c.rc_buf_thresh[i] = thresholds[i];

    c.chunk_size       = (int)(pic_w * 8.0 / 8 + 0.99);
    c.rcb_bits         = 9360;
    c.first_line_bpg_ofs   = 12;
    c.second_line_bpg_ofs  = 0;
    c.nfl_bpg_offset       = 306;  // ~12 << 11 / (48-1)
    c.nsl_bpg_offset       = 0;
    c.slice_bpg_offset     = 44;
    c.initial_scale_value  = 32;
    c.scale_decrement_interval = 8;
    c.scale_increment_interval = 0;
    c.num_ssps          = 3;
    c.pixels_per_group   = 3;
    c.num_slices         = 1;
    return c;
}

#endif // DSC_CONFIG_H
