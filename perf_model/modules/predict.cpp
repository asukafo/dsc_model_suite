#include "predict.h"
#include <cstring>
#include <cstdlib>
#include <ctime>
#include "dsc_algo.h"

Predict::Predict(sc_module_name nm)
    : sc_module(nm), cfg(nullptr), lbuf(nullptr), current_qp(3),
      ich_mru(0), bp_vector(0), bp_consecutive(0)
{
    SC_THREAD(process);
    sensitive << clk.pos();
    reset_signal_is(rst, true);
    memset(ich_pixels, 0, sizeof(ich_pixels));
    memset(ich_valid, 0, sizeof(ich_valid));
}

static bool ich_check(int pxl[3][32], int vld[32], int r, int g, int b, int ql) {
    if (ql >= 8) return false;
    int t = 1 << ql, ck = 0;
    for (int i = 0; i < 32 && ck < 16; i++) {
        if (!vld[i]) continue; ck++;
        if (abs(pxl[0][i]-r) <= t && abs(pxl[1][i]-g) <= t && abs(pxl[2][i]-b) <= t)
            return true;
    }
    return false;
}

void Predict::process() {
    GroupInput gi; GroupPredicted gp;
    wait();
    StageTimer timer(&perf().predict);
    static bool sd = false;
    if (!sd) { srand(time(0)); sd = true; }

    while (true) {
        if (qp_in->num_available() > 0) {
            QPUpdate qu = qp_in->read(); current_qp = qu.primaryQp;
        }
        timer.pre_read(); gi = in_port->read(); timer.post_read();

        int qp = current_qp, mV = 255, nU = cfg->native_422 ? 4 : 3;
        int bpc = cfg->bits_per_component;
        bool cr = cfg->convert_rgb, is420 = cfg->native_420;
        memset(&gp, 0, sizeof(gp));
        gp.qp = qp; gp.groupCount = gi.groupCount; gp.unitsPerGroup = nU;

        int def_v = 1 << (bpc - 1);
        for (int u = 0; u < nU; u++) {
            int cpnt = cfg->native_422 ? ((u==0||u==1)?0:(u-1)) : (u % 3);
            int cpnt_bd = bpc + ((cr && cpnt != 0 && bpc != 16) ? 1 : 0);
            int ql = qlevel(qp, cpnt), qr[3] = {0};
            int hpos = gi.hPos;
            if (is420 && cpnt != 0) hpos /= 2;

            for (int s = 0; s < 3; s++) {
                int hp = hpos + s, pred, act = gi.orig[cpnt][s];

                int prev[6] = {def_v, def_v, def_v, def_v, def_v, def_v};
                int cl = def_v;
                bool skip = is420 && cpnt != 0 && (gi.vPos % 2 == 1);
                if (!skip) {
                    for (int i = -2; i <= 3; i++) {
                        int v = def_v;
                        if (lbuf && !gi.firstLine) v = lbuf->read_prev(cpnt, hp + i);
                        prev[i+2] = v;
                    }
                    if (lbuf && !gi.firstLine && hp > 0) cl = lbuf->read_curr(cpnt, hp - 1);
                }
                if (s > 0 && hp > 0) cl = gp.recon[cpnt][s-1];

                if (bp_vector != 0 && !gi.firstLine && s == 0 && lbuf)
                    pred = lbuf->read_curr(cpnt, hp - bp_vector);
                else
                    pred = map_predict(prev, cl, hp, ql, qr);

                int eq = quantize(act - pred, ql);
                int recon = clp(pred + dequant(eq, ql), 0, (1 << cpnt_bd) - 1);
                gp.quantizedResidual[u][s] = eq;
                gp.recon[cpnt][s] = recon; qr[s] = eq;
                if (lbuf) lbuf->write_curr(cpnt, hp, recon);

                int es = residual_size(eq);
                if (es > gp.maxError[u]) gp.maxError[u] = es;
                gp.quantizedResidualMid[u][s] =
                    quantize(act - midpoint_pred(mV, gp.leftRecon[cpnt], ql), ql);
            }
            gp.predictedSize[u] = 1 + gp.maxError[u] * 3;
            gp.leftRecon[u] = gp.recon[cpnt][2];
        }

        gp.ichSelected = ich_check(ich_pixels, ich_valid,
                                   gp.recon[0][0], gp.recon[1][0], gp.recon[2][0],
                                   qlevel(qp, 0)) ? 1 : 0;

        for (int s = 0; s < 3; s++) {
            ich_mru = (ich_mru + 1) % ICH_SZ;
            ich_pixels[0][ich_mru] = gp.recon[0][s];
            ich_pixels[1][ich_mru] = gp.recon[1][s];
            ich_pixels[2][ich_mru] = gp.recon[2][s];
            ich_valid[ich_mru] = 1;
        }

        int cycles = nU * 4 + 2;
        if (bp_vector != 0) cycles++;
        wait(cycles); timer.add_busy(cycles);
        timer.pre_write(); out_port->write(gp); timer.post_write();
        if (bp_vector != 0) perf().bp_uses++;

        if (gi.hPos + 3 >= cfg->slice_width) {
            wait(50); timer.add_busy(50);
            bp_vector = (rand() % 13) - 6;
            if (abs(bp_vector) < 3) bp_vector = 0;
        }
    }
}
