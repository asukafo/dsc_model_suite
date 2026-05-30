#include "encode.h"
#include <cstring>
#include "dsc_algo.h"

static int  g_ps[4] = {1,1,1,1};
static int  g_pq = 0, g_pi = 0, g_gc = 0, g_ff = -1;

Encode::Encode(sc_module_name nm) : sc_module(nm), cfg(nullptr) {
    SC_THREAD(process);
    sensitive << clk.pos();
    reset_signal_is(rst, true);
}

static int vlc_unit(const GroupPredicted &gp, int u, int qp, int bpc, bool cr,
                    bool use_ich, int *mpp_out) {
    int bits = 0, cpnt = u % 3;
    int cpnt_bd = bpc + ((cr && cpnt != 0 && cpnt != 3 && bpc != 16) ? 1 : 0);
    int ql = qlevel(qp, cpnt);
    int pred = g_ps[u]; if (pred < 1) pred = 1;

    int ql_new = ql, ql_old = qlevel(g_pq, cpnt);
    int adj_pred = pred + ql_old - ql_new;
    int max_sz = cpnt_bd - ql;
    if (adj_pred < 0) adj_pred = 0;
    if (adj_pred > max_sz - 1) adj_pred = max_sz - 1;

    if (u == 0) {
        bool flat_ok = (qp >= 3 && qp <= 12);
        if ((g_gc % 4) == 3 && flat_ok) bits += 1;
        if ((g_gc % 4) == 0 && g_ff >= 0) {
            if (qp >= 7) bits += 1;
            bits += 2;
        }
    }

    int ich_disallow = (bpc == 16 && u == 0 && 3*ql <= 3 - adj_pred);
    if (use_ich && u == 0 && !ich_disallow) {
        bits += (g_pi ? 1 : 3) + 5;
        *mpp_out = 0; g_ps[u] = 1;
        return bits;
    }
    if (u > 0 && use_ich) { bits += 5; return bits; }

    int rs[3], max_r = 0;
    for (int s = 0; s < 3; s++) {
        rs[s] = residual_size(gp.quantizedResidual[u][s]);
        if (rs[s] > max_r) max_r = rs[s];
    }

    int max_res = cpnt_bd - ql;
    *mpp_out = 0;
    if (max_r >= max_res) {
        max_r = max_res;
        for (int s = 0; s < 3; s++) rs[s] = max_r;
        *mpp_out = 1;
    }

    int pfx_val, size;
    if (adj_pred < max_r) { pfx_val = max_r - adj_pred; size = max_r; }
    else                   { pfx_val = 0; size = adj_pred; }
    if (u == 0 && !ich_disallow) pfx_val += g_pi;

    int max_pfx = max_res + (u == 0 && !ich_disallow) - adj_pred;
    bits += (pfx_val == max_pfx) ? max_pfx : (pfx_val + 1);
    for (int s = 0; s < 3; s++) bits += size;

    g_ps[u] = predict_size(rs);
    return bits;
}

void Encode::process() {
    GroupPredicted gp; GroupEncoded ge;
    wait();
    StageTimer timer(&perf().encode);
    memset(g_ps, 0, sizeof(g_ps));
    for (int i = 0; i < 4; i++) g_ps[i] = 1;
    g_pq = g_pi = g_gc = 0; g_ff = -1;

    while (true) {
        timer.pre_read(); gp = in_port->read(); timer.post_read();
        memset(&ge, 0, sizeof(ge));
        ge.qp = gp.qp; ge.groupCount = gp.groupCount;
        ge.unitsPerGroup = gp.unitsPerGroup;
        memcpy(ge.recon, gp.recon, sizeof(ge.recon));

        int qp = gp.qp, bpc = cfg->bits_per_component, nU = gp.unitsPerGroup;
        bool cr = cfg->convert_rgb;

        if (g_gc % 4 == 0) g_ff = -1;
        if (g_ff < 0) g_ff = g_gc % 4;

        bool ich_valid = gp.ichSelected != 0;
        bool use_ich = false;
        if (ich_valid) {
            int ps_save[4], pq_save = g_pq, pi_save = g_pi, gc_save = g_gc, ff_save = g_ff;
            memcpy(ps_save, g_ps, sizeof(g_ps));
            int mpp_dummy, bits_p = 0, bits_i = 0;
            for (int u = 0; u < nU; u++) bits_p += vlc_unit(gp, u, qp, bpc, cr, false, &mpp_dummy);
            memcpy(g_ps, ps_save, sizeof(g_ps));
            g_pq = pq_save; g_pi = pi_save; g_gc = gc_save; g_ff = ff_save;
            for (int u = 0; u < nU; u++) bits_i += vlc_unit(gp, u, qp, bpc, cr, true, &mpp_dummy);
            memcpy(g_ps, ps_save, sizeof(g_ps));
            g_pq = pq_save; g_pi = pi_save; g_gc = gc_save; g_ff = ff_save;
            use_ich = (bits_i < bits_p);
        }
        ge.ichSelected = use_ich ? 1 : 0;

        int tb = 0;
        for (int u = 0; u < nU; u++) {
            int mpp = 0;
            tb += vlc_unit(gp, u, qp, bpc, cr, use_ich, &mpp);
            if (mpp) { perf().mpp_units++; ge.midpointSelected[u] = 1; }
        }
        ge.codedGroupSize = tb;
        perf().total_enc_bits += tb;
        if (use_ich) { perf().ich_groups++; g_pi = 1; } else g_pi = 0;
        g_pq = qp; g_gc++;
        g_ff = (g_gc % 4 == 0) ? -1 : g_ff;

        wait(nU + 1); timer.add_busy(nU + 1);
        timer.pre_write(); out_mux->write(ge); out_rc->write(ge); timer.post_write();
    }
}
