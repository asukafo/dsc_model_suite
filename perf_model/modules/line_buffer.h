#ifndef LINE_BUFFER_H
#define LINE_BUFFER_H

#include <systemc.h>
#include "dsc_config.h"
#include "perf_counters.h"

#define LB_MAX_COMP  4
#define LB_MAX_W     8192
#define LB_PAD_L     5
#define LB_PAD_R     10
#define LB_TOTAL_PAD (LB_PAD_L + LB_PAD_R)

class LineBuffer : public sc_core::sc_module {
public:
    int num_comp;
    int line_width;
    int bits_per_sample;
    int linebuf_depth;
    int read_ports;
    int write_ports;

    int **prevLine[LB_MAX_COMP];
    int **currLine[LB_MAX_COMP];
    int **origLine[LB_MAX_COMP];
    int  total_pixels;

    uint64_t reads, writes;
    uint64_t read_bytes, write_bytes;

    LineBuffer(sc_module_name nm, int comps = 3, int width = 640, int bps = 8,
               int lbd = 9, int rp = 5, int wp = 3);

    ~LineBuffer();

    int  read_prev(int comp, int x, int buf_idx = 0);
    int  read_curr(int comp, int x);
    void write_curr(int comp, int x, int val);
    void write_orig(int comp, int x, int val);
    int  read_orig(int comp, int x);
    void swap_curr_to_prev(int vpos);
    void dump_stats();
};

#endif
