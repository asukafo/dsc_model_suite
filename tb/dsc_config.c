// Standalone DSC config setup — derived from codec_main.c logic
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "dsc_config.h"

#define OFFSET_FRACTIONAL_BITS 11

void tb_config_default(TbConfig *c) {
    memset(c, 0, sizeof(*c));
    c->pic_w = 640; c->pic_h = 480;
    c->slice_w = 0; c->slice_h = 48;
    c->bpc = 8; c->bpp = 8.0f;
    c->convert_rgb = 1;
    c->block_pred_enable = 1;
    c->dsc_version_minor = 2;
    c->linebuf_depth = 9;
    c->full_ich_err_precision = 0;
    c->rc_model_size = 8192;
    c->initial_offset = 6144;
    c->initial_xmit_delay = 512;
}

void tb_config_print(const TbConfig *c) {
    printf("  %dx%d  slice=%dx%d  %dbpc/%.1fbpp  %s  DSC1.%d\n",
           c->pic_w, c->pic_h, c->slice_w ? c->slice_w : c->pic_w,
           c->slice_h ? c->slice_h : c->pic_h,
           c->bpc, (double)c->bpp,
           c->convert_rgb ? "RGB" : "YCbCr",
           c->dsc_version_minor);
}

// ---- RC parameter computation (from codec_main.c:compute_rc_parameters) ----
static int compute_offset(dsc_cfg_t *cfg, int ppg, int gpl,
                          int grpcnt, int id, float bppf,
                          int flbo, int slbo, int n420) {
    int offset = 0;
    int gid = (int)ceil((float)id / ppg);
    if (grpcnt <= gid)
        offset = (int)ceil(grpcnt * ppg * bppf);
    else
        offset = (int)ceil(gid * ppg * bppf)
                 - (((grpcnt - gid) * cfg->slice_bpg_offset) >> OFFSET_FRACTIONAL_BITS);
    if (grpcnt <= gpl)
        offset += grpcnt * flbo;
    else
        offset += gpl * flbo
                  - (((grpcnt - gpl) * cfg->nfl_bpg_offset) >> OFFSET_FRACTIONAL_BITS);
    if (n420) {
        if (grpcnt <= gpl)
            offset -= (grpcnt * cfg->nsl_bpg_offset) >> OFFSET_FRACTIONAL_BITS;
        else if (grpcnt <= 2*gpl)
            offset += (grpcnt - gpl) * slbo
                      - ((gpl * cfg->nsl_bpg_offset) >> OFFSET_FRACTIONAL_BITS);
        else
            offset += (grpcnt - gpl) * slbo
                      - (((grpcnt - gpl) * cfg->nsl_bpg_offset) >> OFFSET_FRACTIONAL_BITS);
    }
    return offset;
}

static int compute_rc_params(dsc_cfg_t *cfg, int id, float bppf, int ifo,
                             int bpc, int use_yuv_input,
                             int *flbo_p, int *slbo_p) {
    int flbo = *flbo_p, slbo = *slbo_p;
    int n420 = cfg->native_420, n422 = cfg->native_422;
    int ppg = 3, sw = cfg->slice_width >> (n420 || n422);
    int uncomp = n422 ? 3*bpc*4 : (3*bpc + (use_yuv_input?0:2))*3;

    if (flbo < 0) {
        if (cfg->slice_height >= 8)
            flbo = 12 + (int)(0.09 * (cfg->slice_height - 8 < 34 ? cfg->slice_height - 8 : 34));
        else flbo = 2*(cfg->slice_height - 1);
        if (flbo < 0) flbo = 0;
        if (flbo > (int)(uncomp - 3*bppf)) flbo = (int)(uncomp - 3*bppf);
    }
    if (slbo < 0) { slbo = n420 ? 12 : 0; if (slbo > (int)(uncomp - 3*bppf)) slbo = (int)(uncomp - 3*bppf); }
    cfg->first_line_bpg_ofs = flbo; cfg->second_line_bpg_ofs = slbo;
    *flbo_p = flbo; *slbo_p = slbo;

    int gpl = (sw + ppg - 1) / ppg;
    cfg->chunk_size = (int)(ceil(sw * bppf / 8.0));

    int nem = cfg->convert_rgb
        ? (3*(cfg->mux_word_size + (4*bpc+4)-2))
        : (!n422 ? (3*cfg->mux_word_size+(4*bpc+4)+2*(4*bpc)-2)
                 : (4*cfg->mux_word_size+(4*bpc+4)+3*(4*bpc)-2));
    int sb = 8 * cfg->chunk_size * cfg->slice_height;
    while (nem > 0 && (sb - nem) % cfg->mux_word_size) nem--;

    cfg->initial_scale_value = 8 * cfg->rc_model_size / (cfg->rc_model_size - cfg->initial_offset);
    if (gpl < cfg->initial_scale_value - 8) cfg->initial_scale_value = gpl + 8;
    cfg->scale_decrement_interval = cfg->initial_scale_value > 8 ? gpl/(cfg->initial_scale_value-8) : 4095;

    int fv = cfg->rc_model_size - ((cfg->initial_xmit_delay * cfg->bits_per_pixel + 8) >> 4) + nem;
    cfg->final_offset = fv;
    if (fv >= cfg->rc_model_size) { fprintf(stderr, "ERROR: final_offset >= rc_model_size\n"); return -1; }
    int final_scale = 8 * cfg->rc_model_size / (cfg->rc_model_size - fv);

    cfg->nfl_bpg_offset = cfg->slice_height > 1 ? (int)ceil((double)(flbo << OFFSET_FRACTIONAL_BITS)/(cfg->slice_height-1)) : 0;
    cfg->nsl_bpg_offset = cfg->slice_height > 2 ? (int)ceil((double)(slbo << OFFSET_FRACTIONAL_BITS)/(cfg->slice_height-1)) : 0;
    int gt = gpl * cfg->slice_height;
    cfg->slice_bpg_offset = (int)ceil((double)(1<<OFFSET_FRACTIONAL_BITS)*(cfg->rc_model_size-cfg->initial_offset+nem)/gt);

    cfg->scale_increment_interval = 0;
    if (final_scale > 9)
        cfg->scale_increment_interval = (int)((double)(1<<OFFSET_FRACTIONAL_BITS)*cfg->final_offset/
            ((double)(final_scale-9)*(cfg->nfl_bpg_offset+cfg->slice_bpg_offset+cfg->nsl_bpg_offset)));

    int mo = compute_offset(cfg,ppg,gpl,(int)ceil((float)id/ppg),id,bppf,flbo,slbo,n420);
    mo = mo > compute_offset(cfg,ppg,gpl,gpl,id,bppf,flbo,slbo,n420)
         ? mo : compute_offset(cfg,ppg,gpl,gpl,id,bppf,flbo,slbo,n420);
    mo = mo > compute_offset(cfg,ppg,gpl,2*gpl,id,bppf,flbo,slbo,n420)
         ? mo : compute_offset(cfg,ppg,gpl,2*gpl,id,bppf,flbo,slbo,n420);
    int rbsMin = cfg->rc_model_size - ifo + mo;
    int hrdDelay = (int)(ceil((double)rbsMin / bppf));
    cfg->rcb_bits = (int)(ceil((double)hrdDelay * bppf));
    cfg->initial_dec_delay = hrdDelay - cfg->initial_xmit_delay;
    return 0;
}

void dsc_setup_rc_8bpc_8bpp(dsc_cfg_t *cfg) {
    int off[] = {2,0,0,-2,-4,-6,-8,-8,-8,-10,-10,-12,-12,-12,-12};
    int mn[]  = {0,0,1,1,3,3,3,3,3,3,5,5,5,9,12};
    int mx[]  = {4,4,5,6,7,7,7,8,9,10,10,11,11,12,13};
    int th[]  = {896,1792,2688,3584,4480,5376,6272,6720,7168,7616,7744,7872,8000,8064};
    for (int i=0;i<15;i++) {
        cfg->rc_range_parameters[i].range_bpg_offset=off[i];
        cfg->rc_range_parameters[i].range_min_qp=mn[i];
        cfg->rc_range_parameters[i].range_max_qp=mx[i];
    }
    for (int i=0;i<14;i++) cfg->rc_buf_thresh[i]=th[i];
}

void dsc_setup_rc_8bpc_12bpp(dsc_cfg_t *cfg) {
    int off[] = {2,0,0,-2,-4,-6,-8,-8,-8,-10,-10,-10,-12,-12,-12};
    int mn[]  = {0,0,1,1,3,3,3,3,3,3,5,5,5,7,11};
    int mx[]  = {2,3,4,5,6,6,7,7,8,9,9,10,11,11,12};
    int th[]  = {896,1792,2688,3584,4480,5376,6272,6720,7168,7616,7744,7872,8000,8064};
    for (int i=0;i<15;i++) {
        cfg->rc_range_parameters[i].range_bpg_offset=off[i];
        cfg->rc_range_parameters[i].range_min_qp=mn[i];
        cfg->rc_range_parameters[i].range_max_qp=mx[i];
    }
    for (int i=0;i<14;i++) cfg->rc_buf_thresh[i]=th[i];
}

int dsc_setup_config(dsc_cfg_t *cfg, const TbConfig *tb) {
    memset(cfg, 0, sizeof(*cfg));

    cfg->pic_width    = tb->pic_w;
    cfg->pic_height   = tb->pic_h;
    cfg->slice_width  = tb->slice_w ? tb->slice_w : tb->pic_w;
    cfg->slice_height = tb->slice_h ? tb->slice_h : tb->pic_h;
    cfg->bits_per_component = tb->bpc;
    cfg->bits_per_pixel     = (int)(tb->bpp * 16 + 0.5);
    cfg->convert_rgb        = tb->convert_rgb;
    cfg->native_422         = tb->native_422;
    cfg->native_420         = tb->native_420;
    cfg->simple_422         = tb->simple_422;
    cfg->block_pred_enable  = tb->block_pred_enable;
    cfg->dsc_version_minor  = tb->dsc_version_minor;
    cfg->linebuf_depth      = tb->linebuf_depth;
    cfg->full_ich_err_precision = tb->full_ich_err_precision;
    cfg->vbr_enable         = 0;
    cfg->mux_word_size      = (tb->bpc <= 10) ? 48 : 64;
    cfg->second_line_ofs_adj = 0;

    cfg->rc_model_size      = tb->rc_model_size;
    cfg->initial_offset     = tb->initial_offset;
    cfg->initial_xmit_delay = tb->initial_xmit_delay;
    cfg->rc_edge_factor     = 6;
    cfg->rc_quant_incr_limit0 = 11;
    cfg->rc_quant_incr_limit1 = 11;
    cfg->rc_tgt_offset_hi   = 3;
    cfg->rc_tgt_offset_lo   = 3;
    cfg->flatness_min_qp    = 3;
    cfg->flatness_max_qp    = 12;
    cfg->flatness_det_thresh = 2;
    cfg->very_flat_qp       = 1 + 2*(tb->bpc - 8);
    cfg->somewhat_flat_qp_delta  = 4;
    cfg->somewhat_flat_qp_thresh = 7 + 2*(tb->bpc - 8);

    // Select RC table
    if (tb->bpc == 8 && tb->bpp <= 8.0f)      dsc_setup_rc_8bpc_8bpp(cfg);
    else if (tb->bpc == 8 && tb->bpp <= 12.0f) dsc_setup_rc_8bpc_12bpp(cfg);
    else if (tb->bpc == 8)                     dsc_setup_rc_8bpc_12bpp(cfg);
    else                                       dsc_setup_rc_8bpc_8bpp(cfg);

    int flb = -1, slb = -1;
    return compute_rc_params(cfg, tb->initial_xmit_delay, tb->bpp,
                             tb->initial_offset, tb->bpc, !tb->convert_rgb,
                             &flb, &slb);
}
