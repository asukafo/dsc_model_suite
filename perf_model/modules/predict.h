#ifndef PREDICT_H
#define PREDICT_H

#include <systemc.h>
#include "dsc_config.h"
#include "dsc_state.h"
#include "perf_counters.h"
#include "line_buffer.h"

class Predict : public sc_core::sc_module {
public:
    sc_in_clk                   clk;
    sc_in<bool>                 rst;
    sc_fifo_in<GroupInput>      in_port;
    sc_fifo_out<GroupPredicted>  out_port;
    sc_fifo_in<QPUpdate>        qp_in;

    const DSCConfig *cfg;
    LineBuffer      *lbuf;
    int              current_qp;

    // ICH state
    static const int ICH_SZ = 32;
    int ich_pixels[3][ICH_SZ];
    int ich_valid[ICH_SZ];
    int ich_mru;

    // BP state
    int bp_vector;
    int bp_consecutive;

    SC_HAS_PROCESS(Predict);

    Predict(sc_module_name nm);

    void set_config(const DSCConfig *c)  { cfg = c; current_qp = 3; }
    void set_linebuf(LineBuffer *lb)     { lbuf = lb; }

    void process();
};

#endif
