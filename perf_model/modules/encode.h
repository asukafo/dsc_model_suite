// DSC SystemC — Stage 2: ENCODE (VLC with ICH/MPP support)
#ifndef ENCODE_H
#define ENCODE_H
#include <systemc.h>
#include "dsc_config.h"
#include "dsc_state.h"
#include "perf_counters.h"

SC_MODULE(Encode) {
    sc_in_clk    clk;
    sc_in<bool>  rst;
    sc_fifo_in<GroupPredicted>  in_port;
    sc_fifo_out<GroupEncoded>   out_mux;
    sc_fifo_out<GroupEncoded>   out_rc;

    const DSCConfig *cfg;

    SC_HAS_PROCESS(Encode);
    Encode(sc_module_name nm) : sc_module(nm) {
        SC_THREAD(process);
        sensitive << clk.pos();
        reset_signal_is(rst, true);
    }
    void set_config(const DSCConfig *c) { cfg = c; }
    void process();
};
#endif
