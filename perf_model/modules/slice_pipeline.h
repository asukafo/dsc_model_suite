#ifndef SLICE_PIPELINE_H
#define SLICE_PIPELINE_H

#include <systemc.h>
#include "dsc_config.h"
#include "perf_counters.h"
#include "line_buffer.h"
#include "fetch.h"
#include "predict.h"
#include "encode.h"
#include "mux.h"
#include "rate_ctl.h"

class SlicePipeline : public sc_core::sc_module {
public:
    const DSCConfig *cfg;
    pic_t           *src_img;
    int              slice_idx;

    LineBuffer  *lbuf;
    Fetch       *fetch_mod;
    Predict     *predict_mod;
    Encode      *encode_mod;
    Mux         *mux_mod;
    RateCtl     *ratectl_mod;

    sc_fifo<GroupInput>     *fifo_fp;
    sc_fifo<GroupPredicted> *fifo_pe;
    sc_fifo<GroupEncoded>   *fifo_em, *fifo_er;
    sc_fifo<QPUpdate>       *fifo_rq;

    sc_in_clk    clk;
    sc_in<bool>  rst;

    SlicePipeline(sc_module_name nm, int fifo_depth = 8);
    ~SlicePipeline();

    void setup(const DSCConfig *c, pic_t *src, int idx);
};

#endif
