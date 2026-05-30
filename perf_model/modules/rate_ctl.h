#ifndef RATE_CTL_H
#define RATE_CTL_H

#include <systemc.h>
#include "dsc_config.h"
#include "dsc_state.h"
#include "perf_counters.h"

class RateCtl : public sc_core::sc_module {
public:
    sc_in_clk                clk;
    sc_in<bool>              rst;
    sc_fifo_in<GroupEncoded>  in_port;
    sc_fifo_out<QPUpdate>     qp_out;

    const DSCConfig *cfg;

    int bufferFullness, stQp, prevQp;
    int rcXformOffset, pixelCount;
    int currentScale, scaleAdjustCounter;

    SC_HAS_PROCESS(RateCtl);

    RateCtl(sc_module_name nm);

    void set_config(const DSCConfig *c);
    void reset_state();
    void process();
};

#endif
