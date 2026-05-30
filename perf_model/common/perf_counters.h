// DSC SystemC Performance Model — Per-Stage Cycle Counters
#ifndef PERF_COUNTERS_H
#define PERF_COUNTERS_H

#include <cstdio>
#include <cstdint>
#include <vector>
#include "dsc_config.h"

// ---- Per-stage timing ----
struct StageCounters {
    uint64_t busy_cycles;       // wait(N) processing time
    uint64_t stall_read;        // blocked on sc_fifo::read()
    uint64_t stall_write;       // blocked on sc_fifo::write()
    uint64_t groups;            // total groups processed

    void reset() { busy_cycles=stall_read=stall_write=groups=0; }

    double util_pct(uint64_t total) const {
        return total ? 100.0 * busy_cycles / total : 0.0;
    }
    double stall_rd_pct(uint64_t total) const {
        return total ? 100.0 * stall_read / total : 0.0;
    }
    double stall_wr_pct(uint64_t total) const {
        return total ? 100.0 * stall_write / total : 0.0;
    }
};

// ---- Pipeline metrics ----
struct PipeMetrics {
    double eff_throughput;     // groups processed / total cycles
    double bubble_pct;         // idle time between pipeline fills
};

// ---- Global counters ----
struct PerfCounters {
    StageCounters fetch;
    StageCounters predict;
    StageCounters encode;
    StageCounters mux;
    StageCounters ratectl;

    // Algorithm
    uint64_t ich_groups;
    uint64_t mpp_units;
    uint64_t bp_uses;
    double   avg_qp;
    int      qp_min, qp_max;
    int      qp_histogram[32];

    // Output
    uint64_t total_output_bits;     // mux output bits
    uint64_t total_output_bytes;
    uint64_t total_enc_bits;        // encode stage total coded bits
    std::vector<int> buffer_fullness_trace;

    // Line buffer
    uint64_t linebuf_reads;
    uint64_t linebuf_writes;
    uint64_t linebuf_read_bytes;
    uint64_t linebuf_write_bytes;

    // Timing helpers — track timestamp per stage for delta measurement
    uint64_t t_fetch, t_predict, t_encode, t_mux, t_ratectl;

    void reset() {
        fetch.reset(); predict.reset(); encode.reset();
        mux.reset(); ratectl.reset();
        ich_groups=mpp_units=bp_uses=0;
        avg_qp=0; qp_min=99; qp_max=0;
        for(int i=0;i<32;i++) qp_histogram[i]=0;
        total_output_bits=total_output_bytes=total_enc_bits=0;
        buffer_fullness_trace.clear();
        linebuf_reads=linebuf_writes=0;
        linebuf_read_bytes=linebuf_write_bytes=0;
        t_fetch=t_predict=t_encode=t_mux=t_ratectl=0;
    }

    // Helper: record read stall (time since last timestamp)
    // use cycles = sc_time_stamp/10ns
    static uint64_t now_cycles() { return sc_time_stamp().value() / 10000; }

    // Print summary
    void print_summary(const DSCConfig &cfg) {
        // Compute total_cycles = max stage completion time
        uint64_t total = now_cycles();
        if (total == 0) total = 1;

        PipeMetrics pm;
        pm.eff_throughput = (double)(fetch.groups) / total;
        uint64_t active_sum = fetch.busy_cycles + predict.busy_cycles +
                              encode.busy_cycles + mux.busy_cycles + ratectl.busy_cycles;
        pm.bubble_pct = total ? 100.0 * (total*5 - active_sum) / (total*5) : 0.0;

        printf("\n========================================\n");
        printf("  DSC Performance Model — Summary\n");
        printf("========================================\n");
        printf("Config: %dx%d, slice=%d, %dbpc/%dbpp\n",
               cfg.pic_width, cfg.pic_height, cfg.slice_height,
               cfg.bits_per_component, cfg.bits_per_pixel / 16);

        printf("\n┌─ Pipeline Timing ──────────────────────────────┐\n");
        printf("│ Total cycles:       %8llu                     │\n", total);
        printf("│ Groups processed:   %8llu                     │\n", fetch.groups);
        printf("│ Effective thruput:  %7.3f groups/cycle        │\n", pm.eff_throughput);
        printf("│ Pixels/cycle:       %7.2f                     │\n", pm.eff_throughput * 3);
        printf("├────────────────────────────────────────────────┤\n");
        printf("│ Stage     │ Busy %%  │ RdStall%% │ WrStall%% │Grps    │\n");
        printf("├───────────┼─────────┼───────────┼───────────┼────────┤\n");
        print_stage("FETCH  ", fetch, total);
        print_stage("PREDICT", predict, total);
        print_stage("ENCODE ", encode, total);
        print_stage("MUX    ", mux, total);
        print_stage("RATECTL", ratectl, total);
        printf("├───────────┴─────────┴───────────┴───────────┴────────┤\n");

        // Bottleneck analysis
        double max_util = 0;
        const char *bottleneck = "NONE";
        for (auto [name, sc] : {
                std::pair{"FETCH", &fetch}, std::pair{"PREDICT", &predict},
                std::pair{"ENCODE", &encode}, std::pair{"MUX", &mux},
                std::pair{"RATECTL", &ratectl}}) {
            double u = sc->util_pct(total);
            if (u > max_util) { max_util = u; bottleneck = name; }
        }
        printf("│ Bottleneck: %s (%.1f%%)                      │\n", bottleneck, max_util);
        printf("└────────────────────────────────────────────────────┘\n");

        // Multi-slice analysis
        if (cfg.num_slices > 1 || 1) {
            double single_throughput = fetch.groups > 0 ? (double)fetch.groups / total : 0;
            double px_per_cycle_single = single_throughput * cfg.pixels_per_group;
            double px_per_cycle_multi  = px_per_cycle_single * cfg.num_slices * 0.95; // 95% efficiency

            printf("\n┌─ Multi-Slice (%d slices) ─────────────────────────┐\n", cfg.num_slices);
            printf("│ Single-slice:  %.3f px/cycle                       │\n", px_per_cycle_single);
            printf("│ Multi-slice:   %.3f px/cycle                       │\n", px_per_cycle_multi);
            printf("│ Speedup:       %.1fx                               │\n",
                   px_per_cycle_single > 0 ? px_per_cycle_multi / px_per_cycle_single : 0.0);
            // HDMI format examples
            printf("├─ HDMI Examples ──────────────────────────────────┤\n");
            struct {const char*name;int w,h;double fps,bpp;} fmts[]={
                {"4K60     ",3840,2160,60,8},{"8K60     ",7680,4320,60,8},
                {"8K60 4:2:0",7680,4320,60,6},{"10K120   ",10240,4320,120,8}};
            for(auto&f:fmts){
                double fpx=(double)f.w*f.h*f.fps;
                double need=fpx/(px_per_cycle_multi>0.001?px_per_cycle_multi:0.001)/1e6;
                double fhdmi_bw=fpx*(f.bpp+2)/1e9; // Gbps (approx)
                printf("│ %s: %.0fMpx/s → %.0fMHz, %.1fGbps       │\n",
                       f.name,fpx/1e6,need,fhdmi_bw);
            }
            printf("└──────────────────────────────────────────────────┘\n");
        }

        printf("\n┌─ Algorithm ───────────────────────────────────────┐\n");
        printf("│ ICH hit:    %5.1f%%  (%llu/%llu)                  │\n",
               fetch.groups ? 100.0*ich_groups/fetch.groups : 0.0, ich_groups, fetch.groups);
        printf("│ MPP rate:   %5.1f%%  (%llu units)                │\n",
               fetch.groups ? 100.0*mpp_units/(fetch.groups*3) : 0.0, mpp_units);
        printf("│ Avg QP:     %.1f   (range %d–%d)                 │\n", avg_qp, qp_min, qp_max);
        printf("└────────────────────────────────────────────────────┘\n");

        printf("\n┌─ Line Buffer ─────────────────────────────────────┐\n");
        printf("│ Reads:  %8llu  (%llu bytes)                       │\n", linebuf_reads, linebuf_read_bytes);
        printf("│ Writes: %8llu  (%llu bytes)                       │\n", linebuf_writes, linebuf_write_bytes);
        double bw = total ? (double)(linebuf_read_bytes+linebuf_write_bytes)/total : 0.0;
        printf("│ Bandwidth: %.2f bytes/cycle                       │\n", bw);
        printf("└────────────────────────────────────────────────────┘\n");

        printf("\n┌─ Output ──────────────────────────────────────────┐\n");
        printf("│ Total bits:  %8llu  (%llu bytes)                 │\n", total_output_bits, total_output_bytes);
        printf("│ Compression: %7.1f : 1                           │\n",
               total_output_bytes ? (double)(cfg.pic_width*cfg.pic_height*3)/total_output_bytes : 0.0);
        printf("└────────────────────────────────────────────────────┘\n");
        printf("========================================\n");
    }

    void print_stage(const char *name, const StageCounters &sc, uint64_t total) {
        printf("│ %-9s │ %5.1f%%  │ %5.1f%%   │ %5.1f%%   │ %6llu │\n",
               name, sc.util_pct(total), sc.stall_rd_pct(total),
               sc.stall_wr_pct(total), sc.groups);
    }
};

inline PerfCounters &perf() { static PerfCounters p; return p; }

// ---- Timing utilities (use in module threads) ----
struct StageTimer {
    StageCounters *sc;
    uint64_t t0;

    StageTimer(StageCounters *c) : sc(c) {
        t0 = PerfCounters::now_cycles();
    }
    // Call before blocking read
    void pre_read()  { t0 = PerfCounters::now_cycles(); }
    // Call after blocking read
    void post_read() { sc->stall_read  += PerfCounters::now_cycles() - t0; }
    // Call after processing wait()
    void add_busy(uint64_t n) { sc->busy_cycles += n; }
    // Call before blocking write
    void pre_write() { t0 = PerfCounters::now_cycles(); }
    // Call after blocking write
    void post_write() {
        sc->stall_write += PerfCounters::now_cycles() - t0;
        sc->groups++;
    }
};

#endif
