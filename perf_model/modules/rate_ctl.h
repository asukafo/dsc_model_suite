// DSC SystemC — Stage 4: RATECTL (Rate Control + Flatness)
#ifndef RATE_CTL_H
#define RATE_CTL_H
#include <systemc.h>
#include "dsc_config.h"
#include "dsc_state.h"
#include "perf_counters.h"

SC_MODULE(RateCtl) {
    sc_in_clk    clk;
    sc_in<bool>  rst;
    sc_fifo_in<GroupEncoded>  in_port;
    sc_fifo_out<QPUpdate>     qp_out;

    const DSCConfig *cfg;

    // RC state
    int bufferFullness, stQp, prevQp;
    int rcXformOffset, pixelCount, rcIntegrator;
    int currentScale, scaleAdjustCounter;
    int bitSaveMode, mppState;

    SC_HAS_PROCESS(RateCtl);
    RateCtl(sc_module_name nm) : sc_module(nm) {
        SC_THREAD(process);
        sensitive << clk.pos();
        reset_signal_is(rst, true);
    }
    void set_config(const DSCConfig *c);
    void reset_state();
    void process();
};
#endif
