// DSC SystemC — Stage 1: PREDICT (MAP/BP/ICH/MPP)
#ifndef PREDICT_H
#define PREDICT_H
#include <systemc.h>
#include "dsc_config.h"
#include "dsc_state.h"
#include "perf_counters.h"
#include "line_buffer.h"

#define ICH_SIZE 32
#define BP_RANGE 13
#define BP_COUNT   3   // consecutive BP wins needed

SC_MODULE(Predict) {
    sc_in_clk    clk;
    sc_in<bool>  rst;
    sc_fifo_in<GroupInput>     in_port;
    sc_fifo_out<GroupPredicted> out_port;
    sc_fifo_in<QPUpdate>       qp_in;

    const DSCConfig *cfg;
    LineBuffer     *lbuf;
    int current_qp;

    // ICH state — 32-entry history per component
    int ich_pixels[3][ICH_SIZE];
    int ich_valid[ICH_SIZE];
    int ich_mru;  // most recently used index

    // BP state — block prediction vector
    int bp_vector;            // selected BP offset for current line
    int bp_consecutive;       // consecutive BP wins
    int bp_last_edge;         // groups since last edge
    int prev_line_done;       // line buffer swap complete flag

    SC_HAS_PROCESS(Predict);
    Predict(sc_module_name nm) : sc_module(nm), lbuf(nullptr), current_qp(3),
        ich_mru(0), bp_vector(0), bp_consecutive(0), bp_last_edge(0),
        prev_line_done(1) {
        SC_THREAD(process);
        sensitive << clk.pos();
        reset_signal_is(rst, true);
        memset(ich_pixels, 0, sizeof(ich_pixels));
        memset(ich_valid, 0, sizeof(ich_valid));
    }
    void set_config(const DSCConfig *c)  { cfg = c; current_qp = 3; }
    void set_linebuf(LineBuffer *lb)     { lbuf = lb; }
    void process();
};
#endif
