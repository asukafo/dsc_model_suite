// DSC SystemC — Line Buffer SRAM Model
// Stores prevLine, currLine, origLine for prediction pipeline.
// Models read/write ports, bandwidth, and line-swap at EOL.

#ifndef LINE_BUFFER_H
#define LINE_BUFFER_H
#include <systemc.h>
#include "dsc_config.h"
#include "perf_counters.h"

#define LB_MAX_COMPONENTS 4
#define LB_MAX_WIDTH       8192
#define LB_PAD_LEFT        5
#define LB_PAD_RIGHT       10
#define LB_TOTAL_PAD       (LB_PAD_LEFT + LB_PAD_RIGHT)

SC_MODULE(LineBuffer) {
    // Configuration
    int num_comp;           // 3 for 4:4:4, 4 for 4:2:2
    int line_width;         // slice_width
    int bits_per_sample;    // 8/10/12
    int linebuf_depth;      // bit-reduced storage depth (e.g., 9)

    // Read port count (configurable — affects parallelism)
    int read_ports;         // e.g., 5 for MAP (needs b,c,d,e + 1)
    int write_ports;        // e.g., 3 for 3 components

    // Internal storage
    // prevLine[comp][pixel+PAD] — previous line (bit-reduced)
    // currLine[comp][pixel+PAD] — current line (full precision)
    // origLine[comp][pixel+PAD] — original pixels (full precision)
    int **prevLine[LB_MAX_COMPONENTS];
    int **currLine[LB_MAX_COMPONENTS];
    int **origLine[LB_MAX_COMPONENTS];
    int  total_pixels;

    // Performance
    uint64_t reads, writes;
    uint64_t read_bytes, write_bytes;

    SC_HAS_PROCESS(LineBuffer);

    LineBuffer(sc_module_name nm, int comps=3, int width=640, int bps=8, int lbd=9,
               int rp=5, int wp=3)
        : sc_module(nm), num_comp(comps), line_width(width),
          bits_per_sample(bps), linebuf_depth(lbd),
          read_ports(rp), write_ports(wp),
          reads(0), writes(0), read_bytes(0), write_bytes(0)
    {
        total_pixels = line_width + LB_TOTAL_PAD;
        int init_val = 1 << (bits_per_sample - 1);  // midpoint

        for (int c = 0; c < num_comp; c++) {
            prevLine[c] = new int*[2];  // double-buffered for 4:2:0
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

    ~LineBuffer() {
        for (int c = 0; c < num_comp; c++) {
            for (int b = 0; b < 2; b++) delete[] prevLine[c][b];
            delete[] prevLine[c];
            delete[] currLine[c][0]; delete[] currLine[c];
            delete[] origLine[c][0]; delete[] origLine[c];
        }
    }

    // ---- Pixel-level access (modeled with cycle delay) ----

    // Read from prevLine (used by PREDICT for MAP: b,c,d,e samples)
    int read_prev(int comp, int x, int buf_idx=0) {
        reads++; read_bytes += linebuf_depth / 8;
        // Zero-delay: timing modeled by pipeline stage wait() calls
        int idx = x + LB_PAD_LEFT;
        if (idx < 0) idx = 0;
        if (idx >= total_pixels) idx = total_pixels - 1;
        return prevLine[comp][buf_idx][idx];
    }

    // Read from currLine (used by PREDICT for left-neighbor 'a')
    int read_curr(int comp, int x) {
        reads++; read_bytes += bits_per_sample / 8;
        int idx = x + LB_PAD_LEFT;
        if (idx < 0) idx = 0;
        if (idx >= total_pixels) idx = total_pixels - 1;
        return currLine[comp][0][idx];
    }

    // Write to currLine (PREDICT writes reconstructed pixels)
    void write_curr(int comp, int x, int val) {
        writes++; write_bytes += bits_per_sample / 8;
        int idx = x + LB_PAD_LEFT;
        if (idx < 0 || idx >= total_pixels) return;
        currLine[comp][0][idx] = val;
    }

    // Write original pixels (FETCH writes to origLine)
    void write_orig(int comp, int x, int val) {
        int idx = x + LB_PAD_LEFT;
        if (idx < 0 || idx >= total_pixels) return;
        origLine[comp][0][idx] = val;
    }

    // Batch write original pixels for a line (FETCH)
    void write_orig_line(int comp, const int *data, int len) {
        for (int i = 0; i < len; i++)
            write_orig(comp, i, data[i]);
    }

    // Read original pixel (FETCH)
    int read_orig(int comp, int x) {
        int idx = x + LB_PAD_LEFT;
        if (idx < 0 || idx >= total_pixels) return 0;
        return origLine[comp][0][idx];
    }

    // ---- End-of-line operations ----

    // Swap currLine → prevLine (with bit-reduction via SampToLineBuf)
    void swap_curr_to_prev(int vpos) {
        int buf_idx = 0;
        // For 4:2:0: alternate chroma buffers every other line
        if (vpos % 2) buf_idx = 1;  // simplified — handles chroma lines

        for (int c = 0; c < num_comp; c++) {
            for (int i = 0; i < total_pixels; i++) {
                int val = currLine[c][0][i];
                // SampToLineBuf: bit-reduce to linebuf_depth
                int shift = bits_per_sample - linebuf_depth;
                if (shift > 0) {
                    val = ((val + (1 << (shift - 1))) >> shift) << shift;
                    if (val > (1 << bits_per_sample) - 1)
                        val = (1 << bits_per_sample) - 1;
                }
                prevLine[c][buf_idx][i] = val;
            }
        }
        // Line copy takes line_width cycles (one pixel per cycle per component)
        wait(line_width / write_ports, SC_NS);
    }

    // ---- Statistics ----
    void dump_stats() {
        perf().linebuf_reads  += reads;
        perf().linebuf_writes += writes;
        perf().linebuf_read_bytes  += read_bytes;
        perf().linebuf_write_bytes += write_bytes;
    }
};

#endif
