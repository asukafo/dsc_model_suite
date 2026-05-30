// DSC SystemC — Stage 0: FETCH
#ifndef FETCH_H
#define FETCH_H
#include <systemc.h>
#include "dsc_config.h"
#include "dsc_state.h"
#include "perf_counters.h"
#include "line_buffer.h"
struct pic_s; typedef struct pic_s pic_t;

SC_MODULE(Fetch) {
    sc_in_clk    clk;
    sc_in<bool>  rst;
    sc_fifo_out<GroupInput> out_port;

    const DSCConfig *cfg;
    LineBuffer     *lbuf;
    pic_t          *src_img;   // source image for pixel data
    int hPos, vPos, groupCount;
    int xstart;                // horizontal offset for multi-slice

    SC_HAS_PROCESS(Fetch);
    Fetch(sc_module_name nm) : sc_module(nm), lbuf(nullptr), src_img(nullptr),
        hPos(0), vPos(0), groupCount(0), xstart(0) {
        SC_THREAD(process);
        sensitive << clk.pos();
        reset_signal_is(rst, true);
    }
    void set_config(const DSCConfig *c)  { cfg = c; }
    void set_linebuf(LineBuffer *lb)     { lbuf = lb; }
    void set_src(pic_t *s)               { src_img = s; }
    void set_xstart(int xs)              { xstart = xs; }
    void process();
};
#endif
