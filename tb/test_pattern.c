// Test pattern generators
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "test_pattern.h"
#include "utl.h"

pic_t *gen_test_image(int w, int h, int bits, PatternType pat) {
    pic_t *p = pcreate_ext(FRAME, RGB, YUV_444, w, h, bits);
    if (!p) return NULL;
    fill_test_image(p, pat);
    return p;
}

void fill_test_image(pic_t *p, PatternType pat) {
    int w = p->w, h = p->h, maxv = (1 << p->bits) - 1;
    if (p->color != RGB) return;

    switch (pat) {
    case PAT_COLOR_BARS: {
        int bar_w = w / 8;
        if (bar_w < 1) bar_w = 1;
        int colours[8][3] = {
            {maxv,0,0},{0,maxv,0},{0,0,maxv},{maxv,maxv,0},
            {0,maxv,maxv},{maxv,0,maxv},{maxv,maxv,maxv},{0,0,0}};
        for (int y=0;y<h;y++) for (int x=0;x<w;x++) {
            int bar = (x/bar_w) % 8;
            p->data.rgb.r[y][x] = colours[bar][0];
            p->data.rgb.g[y][x] = colours[bar][1];
            p->data.rgb.b[y][x] = colours[bar][2];
        }
        break;
    }
    case PAT_RAMP_H:
        for (int y=0;y<h;y++) for (int x=0;x<w;x++) {
            int v = x * maxv / w;
            p->data.rgb.r[y][x]=v; p->data.rgb.g[y][x]=v; p->data.rgb.b[y][x]=v;
        }
        break;
    case PAT_RAMP_V:
        for (int y=0;y<h;y++) for (int x=0;x<w;x++) {
            int v = y * maxv / h;
            p->data.rgb.r[y][x]=v; p->data.rgb.g[y][x]=v; p->data.rgb.b[y][x]=v;
        }
        break;
    case PAT_CHECKERBOARD: {
        int sz = (w < h ? w : h) / 16;
        if (sz < 4) sz = 4;
        for (int y=0;y<h;y++) for (int x=0;x<w;x++) {
            int v = ((x/sz)+(y/sz))%2 ? maxv : 0;
            p->data.rgb.r[y][x]=v; p->data.rgb.g[y][x]=v; p->data.rgb.b[y][x]=v;
        }
        break;
    }
    case PAT_RANDOM:
        srand(42);  // deterministic seed
        for (int y=0;y<h;y++) for (int x=0;x<w;x++) {
            p->data.rgb.r[y][x]=rand()%(maxv+1);
            p->data.rgb.g[y][x]=rand()%(maxv+1);
            p->data.rgb.b[y][x]=rand()%(maxv+1);
        }
        break;
    case PAT_ZONES:
        // Flat zones separated by sharp edges (stress test for DSC)
        for (int y=0;y<h;y++) for (int x=0;x<w;x++) {
            int zx = x/(w/8), zy = y/(h/6);
            int r = ((zx*37+zy*73)%256) * maxv/256;
            int g = ((zx*53+zy*113)%256) * maxv/256;
            int b = ((zx*97+zy*151)%256) * maxv/256;
            p->data.rgb.r[y][x]=r; p->data.rgb.g[y][x]=g; p->data.rgb.b[y][x]=b;
        }
        break;
    case PAT_GRADIENT:
    default:
        for (int y=0;y<h;y++) for (int x=0;x<w;x++) {
            p->data.rgb.r[y][x] = (x*maxv/w + y*maxv/h)/2;
            p->data.rgb.g[y][x] = ((w-x)*maxv/w + y*maxv/h)/2;
            p->data.rgb.b[y][x] = (x*maxv/w + (h-y)*maxv/h)/2;
        }
        break;
    }
}

double image_psnr(pic_t *a, pic_t *b) {
    if (!a || !b || a->w!=b->w || a->h!=b->h) return 0.0;
    double mse=0; int n=0, maxv=(1<<a->bits)-1;
    for (int y=0;y<a->h;y++) for (int x=0;x<a->w;x++) {
        double dr=a->data.rgb.r[y][x]-b->data.rgb.r[y][x];
        double dg=a->data.rgb.g[y][x]-b->data.rgb.g[y][x];
        double db=a->data.rgb.b[y][x]-b->data.rgb.b[y][x];
        mse+=dr*dr+dg*dg+db*db; n+=3;
    }
    mse/=n;
    return mse>0 ? 10.0*log10(maxv*maxv/mse) : 99.0;
}

int image_max_diff(pic_t *a, pic_t *b, int *max_r, int *max_g, int *max_b) {
    if (!a||!b) return -1;
    *max_r=*max_g=*max_b=0;
    for (int y=0;y<a->h;y++) for (int x=0;x<a->w;x++) {
        int dr=abs(a->data.rgb.r[y][x]-b->data.rgb.r[y][x]);
        int dg=abs(a->data.rgb.g[y][x]-b->data.rgb.g[y][x]);
        int db=abs(a->data.rgb.b[y][x]-b->data.rgb.b[y][x]);
        if (dr>*max_r)*max_r=dr; if (dg>*max_g)*max_g=dg; if (db>*max_b)*max_b=db;
    }
    return 0;
}
