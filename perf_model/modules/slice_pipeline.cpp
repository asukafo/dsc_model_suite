#include "slice_pipeline.h"

SlicePipeline::SlicePipeline(sc_module_name nm, int fifo_depth)
    : sc_module(nm), cfg(nullptr), src_img(nullptr), slice_idx(0),
      lbuf(nullptr), fetch_mod(nullptr), predict_mod(nullptr),
      encode_mod(nullptr), mux_mod(nullptr), ratectl_mod(nullptr)
{
    fifo_fp = new sc_fifo<GroupInput>(fifo_depth);
    fifo_pe = new sc_fifo<GroupPredicted>(fifo_depth);
    fifo_em = new sc_fifo<GroupEncoded>(fifo_depth);
    fifo_er = new sc_fifo<GroupEncoded>(fifo_depth);
    fifo_rq = new sc_fifo<QPUpdate>(fifo_depth);
}

SlicePipeline::~SlicePipeline() {
    delete lbuf;
    delete fetch_mod; delete predict_mod; delete encode_mod;
    delete mux_mod; delete ratectl_mod;
    delete fifo_fp; delete fifo_pe; delete fifo_em; delete fifo_er; delete fifo_rq;
}

void SlicePipeline::setup(const DSCConfig *c, pic_t *src, int idx) {
    cfg = c; src_img = src; slice_idx = idx;

    int sw = c->slice_width ? c->slice_width : c->pic_width;
    lbuf = new LineBuffer("lbuf", 3, sw, c->bits_per_component, c->linebuf_depth, 5, 3);

    char nm[32];
    snprintf(nm, 32, "fetch_s%d", idx);
    fetch_mod = new Fetch(nm);
    snprintf(nm, 32, "predict_s%d", idx);
    predict_mod = new Predict(nm);
    snprintf(nm, 32, "encode_s%d", idx);
    encode_mod = new Encode(nm);
    snprintf(nm, 32, "mux_s%d", idx);
    mux_mod = new Mux(nm);
    snprintf(nm, 32, "ratectl_s%d", idx);
    ratectl_mod = new RateCtl(nm);

    fetch_mod->set_config(c);    fetch_mod->set_linebuf(lbuf);    fetch_mod->set_src(src);
    predict_mod->set_config(c);  predict_mod->set_linebuf(lbuf);
    encode_mod->set_config(c);
    mux_mod->set_config(c);
    ratectl_mod->set_config(c);

    int xstart = idx * sw;
    fetch_mod->set_xstart(xstart);

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

    fetch_mod->clk(clk);       fetch_mod->rst(rst);
    predict_mod->clk(clk);     predict_mod->rst(rst);
    encode_mod->clk(clk);      encode_mod->rst(rst);
    mux_mod->clk(clk);         mux_mod->rst(rst);
    ratectl_mod->clk(clk);     ratectl_mod->rst(rst);
}
