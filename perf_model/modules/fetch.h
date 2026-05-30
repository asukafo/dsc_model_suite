#ifndef FETCH_H
#define FETCH_H

#include <systemc.h>
#include "dsc_config.h"
#include "dsc_state.h"
#include "perf_counters.h"
#include "line_buffer.h"

struct pic_s { int w, h, bits; struct { struct { int **r, **g, **b; } rgb; } data; };
typedef struct pic_s pic_t;

class Fetch : public sc_core::sc_module {
public:
    sc_in_clk           clk;
    sc_in<bool>         rst;
    sc_fifo_out<GroupInput> out_port;

    const DSCConfig    *cfg;
    LineBuffer         *lbuf;
    pic_t              *src_img;
    int                 hPos, vPos, groupCount;
    int                 xstart;

    SC_HAS_PROCESS(Fetch);

    Fetch(sc_module_name nm);

    void set_config(const DSCConfig *c)  { cfg = c; }
    void set_linebuf(LineBuffer *lb)     { lbuf = lb; }
    void set_src(pic_t *s)               { src_img = s; }
    void set_xstart(int xs)              { xstart = xs; }

    void process();
};

#endif
