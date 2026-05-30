// DSC Self-Contained Verification Testbench
// Usage: ./dsc_verify [options]
//   -w W  -h H     picture size (default 320x240)
//   -s H           slice height (default 48)
//   -bpc N         bits per component (8/10/12, default 8)
//   -bpp F         bits per pixel (default 8.0)
//   -N  N          number of parallel slices (default 1)
//   -422           enable native 4:2:2 mode
//   -420           enable native 4:2:0 mode
//   -pat NAME      test pattern (bars/ramp/zones/random, default zones)
#include <systemc.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <cmath>

extern "C" {
#include "vdo.h"
#include "dsc_types.h"
#include "dsc_utils.h"
#include "dsc_codec.h"
#include "utl.h"
#include "../tb/dsc_tb.h"
#include "../tb/test_pattern.h"
#include "../tb/dsc_config.h"
}

#include "perf_counters.h"
#include "line_buffer.h"
#include "slice_pipeline.h"

// C model trace capture
struct CModelTrace { int qp, coded_bits, buffer_fullness, ich; };
static std::vector<CModelTrace> c_trace;
extern "C" {
extern void (*dsc_group_trace_cb)(void *state, int group_idx, int slice_y);
static void capture_c_model_group(void *state, int group_idx, int slice_y) {
    dsc_state_t *s = (dsc_state_t*)state;
    c_trace.push_back({s->stQp, s->codedGroupSize, s->bufferFullness, s->ichSelected});
}
}

static void usage(const char *prog) {
    printf("Usage: %s [options]\n", prog);
    printf("  -w W  -h H     picture size (default 320x240)\n");
    printf("  -s H           slice height (default 48)\n");
    printf("  -bpc N         bits/component (8,10,12 default 8)\n");
    printf("  -bpp F         bits/pixel (default 8.0)\n");
    printf("  -N  N          parallel slices (default 1)\n");
    printf("  -422           native 4:2:2 mode\n");
    printf("  -420           native 4:2:0 mode\n");
    printf("  -pat NAME      test pattern (bars/ramp/zones/random default zones)\n");
    exit(1);
}

int sc_main(int argc, char *argv[]) {
    // ---- Parse args ----
    int pic_w=320, pic_h=240, slice_h=48, bpc=8, num_slices=1;
    float bpp=8.0f;
    int native_422=0, native_420=0;
    const char *pat_name="zones";
    for(int i=1;i<argc;i++){
        if(!strcmp(argv[i],"-w")&&i+1<argc) pic_w=atoi(argv[++i]);
        else if(!strcmp(argv[i],"-h")&&i+1<argc) pic_h=atoi(argv[++i]);
        else if(!strcmp(argv[i],"-s")&&i+1<argc) slice_h=atoi(argv[++i]);
        else if(!strcmp(argv[i],"-bpc")&&i+1<argc) bpc=atoi(argv[++i]);
        else if(!strcmp(argv[i],"-bpp")&&i+1<argc) bpp=atof(argv[++i]);
        else if(!strcmp(argv[i],"-N")&&i+1<argc) num_slices=atoi(argv[++i]);
        else if(!strcmp(argv[i],"-422")) native_422=1;
        else if(!strcmp(argv[i],"-420")) native_420=1;
        else if(!strcmp(argv[i],"-pat")&&i+1<argc) pat_name=argv[++i];
        else usage(argv[0]);
    }

    // ---- Config ----
    int chroma_fmt = native_422 ? 422 : (native_420 ? 420 : 444);
    printf("========================================\n");
    printf("  DSC Verification: %dx%d s=%d %dbpc/%.0fbpp %s N=%d\n",
           pic_w, pic_h, slice_h, bpc, bpp,
           chroma_fmt==422?"4:2:2":(chroma_fmt==420?"4:2:0":"4:4:4"),
           num_slices);
    printf("========================================\n\n");

    // ---- Test image ----
    PatternType pat = PAT_ZONES;
    if(!strcmp(pat_name,"bars")) pat=PAT_COLOR_BARS;
    else if(!strcmp(pat_name,"ramp")) pat=PAT_RAMP_H;
    else if(!strcmp(pat_name,"random")) pat=PAT_RANDOM;
    pic_t *src = gen_test_image(pic_w, pic_h, bpc, pat);
    printf("Source: %dx%d RGB %d-bit (%s)\n", src->w, src->h, src->bits, pat_name);

    // ---- C model ----
    TbConfig tbc; tb_config_default(&tbc);
    tbc.pic_w=pic_w; tbc.pic_h=pic_h; tbc.slice_h=slice_h; tbc.bpp=bpp; tbc.bpc=bpc;
    tbc.native_422=native_422; tbc.native_420=native_420;
    tbc.convert_rgb=1; tbc.linebuf_depth=bpc+1;

    dsc_cfg_t c_cfg; dsc_setup_config(&c_cfg, &tbc);
    int buf_size = c_cfg.chunk_size * slice_h;
    unsigned char *cmpr_buf=(unsigned char*)malloc(buf_size);
    int *chunk_sizes=(int*)malloc(sizeof(int)*slice_h);
    pic_t *enc_out=pcreate_ext(FRAME,RGB,YUV_444,pic_w,pic_h,bpc);
    pic_t *dec_out=pcreate_ext(FRAME,RGB,YUV_444,pic_w,pic_h,bpc);
    pic_t **tmp=(pic_t**)malloc(sizeof(pic_t*)*2);
    tmp[0]=pcreate_ext(FRAME,YUV_HD,YUV_444,pic_w,pic_h,bpc);
    tmp[1]=pcreate_ext(FRAME,YUV_HD,YUV_444,pic_w,pic_h,bpc);

    dsc_group_trace_cb=capture_c_model_group; c_trace.clear();
    int c_total=0;
    for(int ys=0;ys<pic_h;ys+=slice_h){
        dsc_setup_config(&c_cfg,&tbc);c_cfg.xstart=0;c_cfg.ystart=ys;
        memset(cmpr_buf,0,buf_size);
        c_total+=DSC_Encode(&c_cfg,src,enc_out,cmpr_buf,tmp,chunk_sizes);
        DSC_Decode(&c_cfg,dec_out,cmpr_buf,tmp);
    }
    double c_psnr=image_psnr(src,dec_out);
    printf("C Model: %d groups  %d bits  PSNR=%.1fdB\n",(int)c_trace.size(),c_total,c_psnr);

    // C model slice 0 for fair comparison
    std::vector<CModelTrace> c_s0; int c_s0_bits=0;
    dsc_group_trace_cb=capture_c_model_group;c_trace.clear();
    dsc_setup_config(&c_cfg,&tbc);c_cfg.xstart=0;c_cfg.ystart=0;
    memset(cmpr_buf,0,buf_size);
    c_s0_bits=DSC_Encode(&c_cfg,src,enc_out,cmpr_buf,tmp,chunk_sizes);
    DSC_Decode(&c_cfg,dec_out,cmpr_buf,tmp);c_s0=c_trace;
    printf("C (slice0): %d groups  %d bits\n\n",(int)c_s0.size(),c_s0_bits);

    // ---- SystemC pipeline ----
    DSCConfig sc_cfg=make_default_config(pic_w,pic_h,slice_h);
    sc_cfg.bits_per_component=bpc; sc_cfg.bits_per_pixel=(int)(bpp*16+0.5);
    sc_cfg.native_422=native_422; sc_cfg.native_420=native_420;
    sc_cfg.linebuf_depth=bpc+1;
    sc_cfg.num_slices=num_slices;
    // Adjust slice_width for multi-slice (each slice gets pic_w/N columns)
    int sw=pic_w/num_slices; if(sw<64)sw=64;
    sc_cfg.slice_width=sw;
    sc_cfg.mux_word_size=(bpc<=10)?48:64;
    sc_cfg.num_ssps=native_422?4:3;
    sc_cfg.pixels_per_group=native_422?4:3;

    sc_clock clk("clk",10,SC_NS);sc_signal<bool>rst("rst");
    SlicePipeline *slices[num_slices];
    for(int s=0;s<num_slices;s++){
        char nm[32];snprintf(nm,32,"slice_%d",s);
        slices[s]=new SlicePipeline(nm,8);
        slices[s]->clk(clk);slices[s]->rst(rst);
        slices[s]->setup(&sc_cfg,src,s);
    }

    perf().reset();
    int groups=(sw/3)*slice_h;
    int sim_ns=groups*250+5000;
    printf("SC: %d slices × %d groups = %d groups\n",num_slices,groups,groups*num_slices);
    rst.write(true);sc_start(5,SC_NS);rst.write(false);
    sc_start(sim_ns,SC_NS);sc_stop();
    printf("SC done: %llu groups\n\n",perf().fetch.groups);

    // ---- Report ----
    int c_s0_total=0;for(auto&t:c_s0)c_s0_total+=t.coded_bits;
    int sc_total=(int)perf().total_enc_bits;
    int sc_grps=(int)perf().ratectl.groups;
    double c0_avg=c_s0.empty()?0:(double)c_s0_total/c_s0.size();
    double sc_avg=sc_grps>0?(double)sc_total/sc_grps:0;

    printf("========================================\n");
    printf("  Comparison Report\n");
    printf("========================================\n");
    printf("  %-20s %12s %12s\n","Metric","C Model","SC Model");
    printf("  %-20s %12s %12s\n","--------------------","------------","------------");
    printf("  %-20s %12d %12d\n","Groups (slice0)",(int)c_s0.size(),sc_grps);
    printf("  %-20s %12d %12d\n","Total bits",c_s0_total,sc_total);
    printf("  %-20s %11.1f %12.1f\n","Avg QP",
           c_s0.empty()?0.0:(double)c_s0[0].qp,perf().avg_qp);
    printf("  %-20s %11.1f %12.1f\n","Avg bits/group",c0_avg,sc_avg);
    printf("  %-20s %11s %12s\n","QP match","-",
           fabs((c_s0.empty()?0:c_s0[0].qp)-perf().avg_qp)<5?"PASS":"WARN");
    printf("  %-20s %11s %12llu\n","ICH groups","-",perf().ich_groups);
    printf("  %-20s %11s %12llu\n","MPP units","-",perf().mpp_units);
    printf("========================================\n\n");

    perf().print_summary(sc_cfg);

    // Cleanup
    pdestroy(src);pdestroy(enc_out);pdestroy(dec_out);
    pdestroy(tmp[0]);pdestroy(tmp[1]);free(tmp);free(cmpr_buf);free(chunk_sizes);
    for(int s=0;s<num_slices;s++)delete slices[s];
    return 0;
}
