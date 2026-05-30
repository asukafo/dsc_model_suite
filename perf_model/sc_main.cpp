// DSC SystemC Performance Model — Standalone
// Usage: ./dsc_perf [options]
//   -w W  -h H     picture size (default 320x240)
//   -s H           slice height (default 48)
//   -bpc N         bits per component (8/10/12, default 8)
//   -bpp F         bits per pixel (default 8.0)
//   -N  N          number of parallel slices (default 1)
//   -422/-420      chroma format
//   -pat NAME      test pattern (bars/ramp/zones/random)
#include <systemc.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "perf_counters.h"
#include "dsc_config.h"
#include "dsc_state.h"
#include "line_buffer.h"
#include "slice_pipeline.h"

// Test image generation (embedded)
#include "fetch.h"  // for pic_t definition

static pic_t *gen_image(int w, int h, int bpc, const char *pat) {
    pic_t *p = (pic_t*)calloc(1, sizeof(pic_t)); p->w=w; p->h=h; p->bits=bpc;
    p->data.rgb.r=(int**)malloc(h*sizeof(int*)); p->data.rgb.g=(int**)malloc(h*sizeof(int*));
    p->data.rgb.b=(int**)malloc(h*sizeof(int*));
    for(int y=0;y<h;y++){
        p->data.rgb.r[y]=(int*)malloc(w*sizeof(int));
        p->data.rgb.g[y]=(int*)malloc(w*sizeof(int));
        p->data.rgb.b[y]=(int*)malloc(w*sizeof(int));
    }
    int maxv=(1<<bpc)-1;
    for(int y=0;y<h;y++)for(int x=0;x<w;x++){
        int r,g,b;
        if(!strcmp(pat,"bars")){int bw=w/8;if(bw<1)bw=1;
            int cols[8][3]={{maxv,0,0},{0,maxv,0},{0,0,maxv},{maxv,maxv,0},
                            {0,maxv,maxv},{maxv,0,maxv},{maxv,maxv,maxv},{0,0,0}};
            int bar=(x/bw)%8;r=cols[bar][0];g=cols[bar][1];b=cols[bar][2];}
        else if(!strcmp(pat,"ramp")){r=g=b=x*maxv/w;}
        else if(!strcmp(pat,"random")){r=rand()%(maxv+1);g=rand()%(maxv+1);b=rand()%(maxv+1);}
        else { // zones (default)
            int zx=x/(w/8),zy=y/(h/6);
            r=((zx*37+zy*73)%256)*maxv/256;
            g=((zx*53+zy*113)%256)*maxv/256;
            b=((zx*97+zy*151)%256)*maxv/256;
        }
        p->data.rgb.r[y][x]=r;p->data.rgb.g[y][x]=g;p->data.rgb.b[y][x]=b;
    }
    return p;
}
static void free_image(pic_t *p){
    for(int y=0;y<p->h;y++){free(p->data.rgb.r[y]);free(p->data.rgb.g[y]);free(p->data.rgb.b[y]);}
    free(p->data.rgb.r);free(p->data.rgb.g);free(p->data.rgb.b);free(p);
}

static void usage(const char *prog) {
    printf("Usage: %s [options]\n", prog);
    printf("  -w W -h H  -s H  -bpc N  -bpp F  -N N  -422/-420  -pat NAME\n");
    exit(1);
}

int sc_main(int argc, char *argv[]) {
    int pic_w=320, pic_h=240, slice_h=48, bpc=8, num_slices=1;
    float bpp=8.0f;
    int native_422=0, native_420=0;
    const char *pat="zones";
    for(int i=1;i<argc;i++){
        if(!strcmp(argv[i],"-w")&&i+1<argc) pic_w=atoi(argv[++i]);
        else if(!strcmp(argv[i],"-h")&&i+1<argc) pic_h=atoi(argv[++i]);
        else if(!strcmp(argv[i],"-s")&&i+1<argc) slice_h=atoi(argv[++i]);
        else if(!strcmp(argv[i],"-bpc")&&i+1<argc) bpc=atoi(argv[++i]);
        else if(!strcmp(argv[i],"-bpp")&&i+1<argc) bpp=atof(argv[++i]);
        else if(!strcmp(argv[i],"-N")&&i+1<argc) num_slices=atoi(argv[++i]);
        else if(!strcmp(argv[i],"-422")) native_422=1;
        else if(!strcmp(argv[i],"-420")) native_420=1;
        else if(!strcmp(argv[i],"-pat")&&i+1<argc) pat=argv[++i];
        else usage(argv[0]);
    }

    printf("========================================\n");
    printf("  DSC SystemC Perf Model\n");
    printf("  %dx%d s=%d %dbpc/%.0fbpp %s N=%d\n",
           pic_w, pic_h, slice_h, bpc, bpp,
           native_422?"4:2:2":(native_420?"4:2:0":"4:4:4"), num_slices);
    printf("========================================\n\n");

    // Test image
    pic_t *src = gen_image(pic_w, pic_h, bpc, pat);
    printf("Source: %dx%d RGB %d-bit (%s)\n", pic_w, pic_h, bpc, pat);

    // Config
    DSCConfig cfg = make_default_config(pic_w, pic_h, slice_h);
    cfg.bits_per_component = bpc;
    cfg.bits_per_pixel = (int)(bpp * 16 + 0.5);
    cfg.native_422 = native_422; cfg.native_420 = native_420;
    cfg.linebuf_depth = bpc + 1;
    cfg.num_slices = num_slices;
    int sw = pic_w / num_slices; if (sw < 64) sw = 64;
    cfg.slice_width = sw;
    cfg.mux_word_size = (bpc <= 10) ? 48 : 64;
    cfg.num_ssps = native_422 ? 4 : 3;

    // Pipeline
    sc_clock clk("clk", 10, SC_NS);
    sc_signal<bool> rst("rst");
    SlicePipeline **slices = new SlicePipeline*[num_slices];
    for (int s = 0; s < num_slices; s++) {
        char nm[32]; snprintf(nm, 32, "slice_%d", s);
        slices[s] = new SlicePipeline(nm, 8);
        slices[s]->clk(clk); slices[s]->rst(rst);
        slices[s]->setup(&cfg, src, s);
    }

    perf().reset();
    int groups = (sw / 3) * slice_h;
    int sim_ns = groups * 300 + 5000;
    printf("Running: %d slices × %d groups...\n", num_slices, groups);
    rst.write(true); sc_start(5, SC_NS); rst.write(false);
    sc_start(sim_ns, SC_NS); sc_stop();
    printf("Done: %llu groups processed\n\n", perf().fetch.groups);

    perf().print_summary(cfg);

    for (int s = 0; s < num_slices; s++) delete slices[s];
    delete[] slices;
    free_image(src);
    return 0;
}
