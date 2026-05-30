#ifndef MUX_H
#define MUX_H

#include <systemc.h>
#include "dsc_config.h"
#include "dsc_state.h"
#include "perf_counters.h"

class Mux : public sc_core::sc_module {
public:
    sc_in_clk               clk;
    sc_in<bool>             rst;
    sc_fifo_in<GroupEncoded> in_port;

    const DSCConfig *cfg;
    int total_bits;

    SC_HAS_PROCESS(Mux);

    Mux(sc_module_name nm);

    void set_config(const DSCConfig *c) { cfg = c; }

    void process();
};

#endif
