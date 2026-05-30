// DSC SystemC — Stage 3: MUX (SSP Sub-Stream Multiplexing)
#ifndef MUX_H
#define MUX_H
#include <systemc.h>
#include "dsc_config.h"
#include "dsc_state.h"
#include "perf_counters.h"

SC_MODULE(Mux) {
    sc_in_clk    clk;
    sc_in<bool>  rst;
    sc_fifo_in<GroupEncoded>  in_port;

    const DSCConfig *cfg;
    int total_bits;

    SC_HAS_PROCESS(Mux);
    Mux(sc_module_name nm) : sc_module(nm), total_bits(0) {
        SC_THREAD(process);
        sensitive << clk.pos();
        reset_signal_is(rst, true);
    }
    void set_config(const DSCConfig *c) { cfg = c; }
    void process();
};
#endif
