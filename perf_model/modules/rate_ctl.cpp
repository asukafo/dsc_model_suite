#include "rate_ctl.h"

RateCtl::RateCtl(sc_module_name nm) : sc_module(nm), cfg(nullptr) {
    SC_THREAD(process);
    sensitive << clk.pos();
    reset_signal_is(rst, true);
}

void RateCtl::set_config(const DSCConfig *c) { cfg = c; reset_state(); }

void RateCtl::reset_state() {
    bufferFullness = cfg->rc_model_size - cfg->initial_offset;
    stQp    = 3;  prevQp  = 0;
    rcXformOffset = cfg->rc_model_size - cfg->initial_offset;
    pixelCount = 0;
    currentScale = cfg->initial_scale_value;
    scaleAdjustCounter = 0;
}

void RateCtl::process() {
    GroupEncoded ge; QPUpdate qpu;
    wait();
    StageTimer timer(&perf().ratectl);

    while (true) {
        timer.pre_read();
        ge = in_port->read();
        timer.post_read();

        int ppg = cfg->pixels_per_group, bpf = cfg->bits_per_pixel;
        for (int p = 0; p < ppg; p++) { bufferFullness -= (bpf >> 4); pixelCount++; }
        bufferFullness += ge.codedGroupSize;

        int throttle = rcXformOffset - cfg->rc_model_size;
        int modelFull = (currentScale * (bufferFullness + throttle)) >> 3;

        int range = 0;
        for (int i = 14 - 1; i >= 0; i--)
            if (modelFull > cfg->rc_buf_thresh[i] - cfg->rc_model_size)
                { range = i + 1; break; }

        int bpg = (bpf * ppg + 8) >> 4;
        int tgt = bpg + cfg->range_bpg_offset[range];
        if (tgt < 0) tgt = 0;

        int nQp = stQp;
        if (ge.codedGroupSize > tgt + cfg->rc_tgt_offset_hi && bufferFullness >= 64) {
            int incr = (ge.codedGroupSize - tgt) >> 1;
            nQp += (incr > 0) ? incr : 1;
        } else if (ge.codedGroupSize < tgt - cfg->rc_tgt_offset_lo) {
            nQp--;
        }
        if (nQp < cfg->range_min_qp[range]) nQp = cfg->range_min_qp[range];
        if (nQp > cfg->range_max_qp[range]) nQp = cfg->range_max_qp[range];
        if (nQp < 0) nQp = 0;

        int ovf = cfg->native_422 ? -224 : -172;
        if (bufferFullness + throttle > ovf) nQp = cfg->range_max_qp[14];

        prevQp = stQp; stQp = nQp;

        double oa = perf().avg_qp;
        double n  = (double)perf().ratectl.groups;
        perf().avg_qp = n > 0 ? (oa * n + stQp) / (n + 1) : stQp;
        if (stQp < perf().qp_min) perf().qp_min = stQp;
        if (stQp > perf().qp_max) perf().qp_max = stQp;
        if (stQp >= 0 && stQp < 32) perf().qp_histogram[stQp]++;
        perf().buffer_fullness_trace.push_back(bufferFullness);

        qpu.primaryQp  = stQp;
        qpu.prevQp     = prevQp;
        qpu.groupCount  = ge.groupCount;

        wait(2); timer.add_busy(2);
        timer.pre_write();
        qp_out->write(qpu);
        timer.post_write();
    }
}
