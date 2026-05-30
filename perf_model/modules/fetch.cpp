#include "fetch.h"

Fetch::Fetch(sc_module_name nm)
    : sc_module(nm), cfg(nullptr), lbuf(nullptr), src_img(nullptr),
      hPos(0), vPos(0), groupCount(0), xstart(0)
{
    SC_THREAD(process);
    sensitive << clk.pos();
    reset_signal_is(rst, true);
}

void Fetch::process() {
    GroupInput gi;
    wait();

    StageTimer timer(&perf().fetch);

    while (true) {
        memset(&gi, 0, sizeof(gi));
        gi.hPos       = hPos;
        gi.vPos       = vPos;
        gi.qp         = 3;
        gi.groupCount  = groupCount;
        gi.firstLine  = (vPos == 0);
        gi.native420  = cfg->native_420;
        gi.native422  = cfg->native_422;

        int px = hPos;
        for (int p = 0; p < 3 && (px + p) < cfg->slice_width; p++) {
            for (int c = 0; c < 3; c++) {
                if (src_img && vPos < src_img->h) {
                    int row = vPos + cfg->ystart;
                    if (row >= src_img->h) row = src_img->h - 1;
                    int col = xstart + px + p;
                    if (col >= src_img->w) col = src_img->w - 1;
                    if (c == 0)      gi.orig[c][p] = src_img->data.rgb.r[row][col];
                    else if (c == 1) gi.orig[c][p] = src_img->data.rgb.g[row][col];
                    else             gi.orig[c][p] = src_img->data.rgb.b[row][col];
                } else {
                    gi.orig[c][p] = ((px + p) * 255 / cfg->slice_width + c * 85 + vPos) % 256;
                }
            }
        }

        int def_v = 1 << (cfg->bits_per_component - 1);
        for (int c = 0; c < 3; c++)
            for (int i = 0; i < 8; i++)
                gi.prevLine[c][i] = def_v;

        wait(3);
        timer.add_busy(3);

        timer.pre_write();
        out_port->write(gi);
        timer.post_write();

        hPos += 3; groupCount++;
        if (hPos >= cfg->slice_width) {
            hPos = 0; vPos++; groupCount = 0;
            if (lbuf) {
                lbuf->swap_curr_to_prev(vPos - 1);
                lbuf->dump_stats();
            }
            wait(10); timer.add_busy(10);
        }
        if (vPos >= cfg->slice_height) {
            wait(100); timer.add_busy(100);
            break;
        }
    }
}
