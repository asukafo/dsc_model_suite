#include "mux.h"

Mux::Mux(sc_module_name nm) : sc_module(nm), cfg(nullptr), total_bits(0) {
    SC_THREAD(process);
    sensitive << clk.pos();
    reset_signal_is(rst, true);
}

void Mux::process() {
    GroupEncoded ge;
    int ssp_fifo[4] = {0};
    wait();
    StageTimer timer(&perf().mux);

    while (true) {
        timer.pre_read();
        ge = in_port->read();
        timer.post_read();

        int mb = 0, mw = cfg->mux_word_size;
        int bpu = ge.codedGroupSize / (ge.unitsPerGroup > 0 ? ge.unitsPerGroup : 1);
        for (int u = 0; u < ge.unitsPerGroup; u++) {
            int ssp = u % cfg->num_ssps;
            ssp_fifo[ssp] += bpu;
            while (ssp_fifo[ssp] >= mw) { ssp_fifo[ssp] -= mw; mb += mw; }
        }
        total_bits += mb;
        perf().total_output_bits += mb;
        perf().total_output_bytes += mb / 8;

        wait(1); timer.add_busy(1);
        timer.pre_write(); timer.post_write();
    }
}
