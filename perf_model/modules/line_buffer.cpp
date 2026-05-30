#include "line_buffer.h"
#include <cstring>

LineBuffer::LineBuffer(sc_module_name nm, int comps, int width, int bps,
                       int lbd, int rp, int wp)
    : sc_module(nm), num_comp(comps), line_width(width),
      bits_per_sample(bps), linebuf_depth(lbd),
      read_ports(rp), write_ports(wp),
      reads(0), writes(0), read_bytes(0), write_bytes(0)
{
    total_pixels = line_width + LB_TOTAL_PAD;
    int init_val = 1 << (bits_per_sample - 1);

    for (int c = 0; c < num_comp; c++) {
        prevLine[c] = new int*[2];
        currLine[c] = new int*[1];
        origLine[c] = new int*[1];
        for (int b = 0; b < 2; b++) {
            prevLine[c][b] = new int[total_pixels];
            for (int i = 0; i < total_pixels; i++)
                prevLine[c][b][i] = init_val;
        }
        currLine[c][0] = new int[total_pixels];
        for (int i = 0; i < total_pixels; i++)
            currLine[c][0][i] = init_val;
        origLine[c][0] = new int[total_pixels];
        memset(origLine[c][0], 0, total_pixels * sizeof(int));
    }
}

LineBuffer::~LineBuffer() {
    for (int c = 0; c < num_comp; c++) {
        for (int b = 0; b < 2; b++) delete[] prevLine[c][b];
        delete[] prevLine[c];
        delete[] currLine[c][0]; delete[] currLine[c];
        delete[] origLine[c][0]; delete[] origLine[c];
    }
}

int LineBuffer::read_prev(int comp, int x, int buf_idx) {
    reads++; read_bytes += linebuf_depth / 8;
    int idx = x + LB_PAD_L;
    if (idx < 0) idx = 0;
    if (idx >= total_pixels) idx = total_pixels - 1;
    return prevLine[comp][buf_idx][idx];
}

int LineBuffer::read_curr(int comp, int x) {
    reads++; read_bytes += bits_per_sample / 8;
    int idx = x + LB_PAD_L;
    if (idx < 0) idx = 0;
    if (idx >= total_pixels) idx = total_pixels - 1;
    return currLine[comp][0][idx];
}

void LineBuffer::write_curr(int comp, int x, int val) {
    writes++; write_bytes += bits_per_sample / 8;
    int idx = x + LB_PAD_L;
    if (idx < 0 || idx >= total_pixels) return;
    currLine[comp][0][idx] = val;
}

void LineBuffer::write_orig(int comp, int x, int val) {
    int idx = x + LB_PAD_L;
    if (idx < 0 || idx >= total_pixels) return;
    origLine[comp][0][idx] = val;
}

int LineBuffer::read_orig(int comp, int x) {
    int idx = x + LB_PAD_L;
    if (idx < 0 || idx >= total_pixels) return 0;
    return origLine[comp][0][idx];
}

void LineBuffer::swap_curr_to_prev(int vpos) {
    int buf_idx = (vpos % 2) ? 1 : 0;
    for (int c = 0; c < num_comp; c++) {
        for (int i = 0; i < total_pixels; i++) {
            int val = currLine[c][0][i];
            int shift = bits_per_sample - linebuf_depth;
            if (shift > 0) {
                val = ((val + (1 << (shift - 1))) >> shift) << shift;
                if (val > (1 << bits_per_sample) - 1)
                    val = (1 << bits_per_sample) - 1;
            }
            prevLine[c][buf_idx][i] = val;
        }
    }
}

void LineBuffer::dump_stats() {
    perf().linebuf_reads  += reads;
    perf().linebuf_writes += writes;
    perf().linebuf_read_bytes  += read_bytes;
    perf().linebuf_write_bytes += write_bytes;
}
