// DSC SystemC — Slice Pipeline (encapsulates FETCH→PREDICT→ENCODE→MUX→RATECTL)
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

SC_MODULE(SlicePipeline) {
    // Config
    const DSCConfig *cfg;
    pic_t           *src_img;
    int              slice_idx;

    // Line buffer (per-slice, independent)
    LineBuffer  *lbuf;

    // Pipeline stages
    Fetch    *fetch_mod;
    Predict  *predict_mod;
    Encode   *encode_mod;
    Mux      *mux_mod;
    RateCtl  *ratectl_mod;

    // FIFOs
    sc_fifo<GroupInput>     *fifo_fp;
    sc_fifo<GroupPredicted> *fifo_pe;
    sc_fifo<GroupEncoded>   *fifo_em, *fifo_er;
    sc_fifo<QPUpdate>       *fifo_rq;

    // Clock & reset
    sc_in_clk    clk;
    sc_in<bool>  rst;

    SC_HAS_PROCESS(SlicePipeline);

    SlicePipeline(sc_module_name nm, int fifo_depth=8)
        : sc_module(nm), cfg(nullptr), src_img(nullptr), slice_idx(0),
          lbuf(nullptr), fetch_mod(nullptr), predict_mod(nullptr),
          encode_mod(nullptr), mux_mod(nullptr), ratectl_mod(nullptr)
    {
        // Create FIFOs
        fifo_fp = new sc_fifo<GroupInput>(fifo_depth);
        fifo_pe = new sc_fifo<GroupPredicted>(fifo_depth);
        fifo_em = new sc_fifo<GroupEncoded>(fifo_depth);
        fifo_er = new sc_fifo<GroupEncoded>(fifo_depth);
        fifo_rq = new sc_fifo<QPUpdate>(fifo_depth);
    }

    ~SlicePipeline() {
        delete lbuf;
        delete fetch_mod; delete predict_mod; delete encode_mod;
        delete mux_mod; delete ratectl_mod;
        delete fifo_fp; delete fifo_pe; delete fifo_em; delete fifo_er; delete fifo_rq;
    }

    void setup(const DSCConfig *c, pic_t *src, int idx) {
        cfg = c; src_img = src; slice_idx = idx;

        // Create per-slice line buffer
        int sw = c->slice_width ? c->slice_width : c->pic_width;
        lbuf = new LineBuffer("lbuf", 3, sw, c->bits_per_component, c->linebuf_depth, 5, 3);

        // Create pipeline stages
        char nm[32];
        snprintf(nm,32,"fetch_s%d",idx);    fetch_mod    = new Fetch(nm);
        snprintf(nm,32,"predict_s%d",idx);  predict_mod  = new Predict(nm);
        snprintf(nm,32,"encode_s%d",idx);   encode_mod   = new Encode(nm);
        snprintf(nm,32,"mux_s%d",idx);      mux_mod      = new Mux(nm);
        snprintf(nm,32,"ratectl_s%d",idx);  ratectl_mod  = new RateCtl(nm);

        fetch_mod->set_config(c);    fetch_mod->set_linebuf(lbuf);    fetch_mod->set_src(src);
        predict_mod->set_config(c);  predict_mod->set_linebuf(lbuf);
        encode_mod->set_config(c);
        mux_mod->set_config(c);
        ratectl_mod->set_config(c);

        // Set slice position
        int xstart = idx * sw;
        fetch_mod->set_xstart(xstart);

        // Connect FIFOs
        fetch_mod->out_port(*fifo_fp);
        predict_mod->in_port(*fifo_fp);
        predict_mod->out_port(*fifo_pe);
        encode_mod->in_port(*fifo_pe);
        encode_mod->out_mux(*fifo_em);
        mux_mod->in_port(*fifo_em);
        encode_mod->out_rc(*fifo_er);
        ratectl_mod->in_port(*fifo_er);
        ratectl_mod->qp_out(*fifo_rq);
        predict_mod->qp_in(*fifo_rq);

        // Connect clock & reset
        fetch_mod->clk(clk);    fetch_mod->rst(rst);
        predict_mod->clk(clk);  predict_mod->rst(rst);
        encode_mod->clk(clk);   encode_mod->rst(rst);
        mux_mod->clk(clk);      mux_mod->rst(rst);
        ratectl_mod->clk(clk);  ratectl_mod->rst(rst);
    }
};

#endif
