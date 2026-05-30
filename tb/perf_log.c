// Per-group performance logging
#include <stdlib.h>
#include <string.h>
#include "dsc_tb.h"

SlicePerfLog *perf_log_create(int max_groups) {
    SlicePerfLog *log = (SlicePerfLog*)calloc(1, sizeof(SlicePerfLog));
    if (!log) return NULL;
    log->max_groups = max_groups > 0 ? max_groups : MAX_GROUPS_PER_SLICE;
    log->groups = (GroupStats*)calloc(log->max_groups, sizeof(GroupStats));
    log->qp_min = 99; log->qp_max = 0;
    return log;
}

void perf_log_record(SlicePerfLog *log, int qp, int coded_bits,
                     int buffer_fullness, int rc_range,
                     int ich, int flatness) {
    if (!log || log->num_groups >= log->max_groups) return;
    GroupStats *g = &log->groups[log->num_groups++];
    g->group_idx       = log->num_groups - 1;
    g->qp              = qp;
    g->coded_bits      = coded_bits;
    g->buffer_fullness = buffer_fullness;
    g->rc_range        = rc_range;
    g->ich_selected    = ich;
    g->flatness_detected = flatness;
}

void perf_log_finalize(SlicePerfLog *log) {
    if (!log || log->num_groups == 0) return;
    double sum_qp = 0;
    log->qp_min = 99; log->qp_max = 0;
    log->ich_count = 0;
    log->total_enc_bits = 0;
    memset(log->qp_hist, 0, sizeof(log->qp_hist));

    for (int i = 0; i < log->num_groups; i++) {
        GroupStats *g = &log->groups[i];
        sum_qp += g->qp;
        if (g->qp < log->qp_min) log->qp_min = g->qp;
        if (g->qp > log->qp_max) log->qp_max = g->qp;
        if (g->qp >= 0 && g->qp < 32) log->qp_hist[g->qp]++;
        if (g->ich_selected) log->ich_count++;
        log->total_enc_bits += g->coded_bits;
    }
    log->avg_qp = sum_qp / log->num_groups;
    log->total_output_bytes = (log->total_enc_bits + 7) / 8;
}

void perf_log_write_csv(SlicePerfLog *log, const char *filename) {
    if (!log || !filename) return;
    FILE *f = fopen(filename, "w");
    if (!f) { fprintf(stderr, "Cannot open %s\n", filename); return; }
    fprintf(f, "group,qp,coded_bits,buffer_fullness,rc_range,ich,flatness\n");
    for (int i = 0; i < log->num_groups; i++) {
        GroupStats *g = &log->groups[i];
        fprintf(f, "%d,%d,%d,%d,%d,%d,%d\n",
                g->group_idx, g->qp, g->coded_bits, g->buffer_fullness,
                g->rc_range, g->ich_selected, g->flatness_detected);
    }
    fclose(f);
    printf("  Perf log written: %s (%d groups)\n", filename, log->num_groups);
}

void perf_log_print_summary(SlicePerfLog *log) {
    if (!log) return;
    printf("  Groups: %d  Total bits: %d  Bytes: %d\n",
           log->num_groups, log->total_enc_bits, log->total_output_bytes);
    printf("  QP: avg=%.1f  min=%d  max=%d\n", log->avg_qp, log->qp_min, log->qp_max);
    printf("  ICH: %d/%d (%.1f%%)\n", log->ich_count, log->num_groups,
           log->num_groups ? 100.0*log->ich_count/log->num_groups : 0.0);
    printf("  QP hist: ");
    for (int i=0;i<=log->qp_max && i<32;i++)
        if (log->qp_hist[i]) printf("%d:%d ", i, log->qp_hist[i]);
    printf("\n");
}

void perf_log_free(SlicePerfLog *log) {
    if (log) { free(log->groups); free(log); }
}
