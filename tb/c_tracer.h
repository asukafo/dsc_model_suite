// C Model Tracer — per-group data capture for SystemC model verification
#ifndef C_TRACER_H
#define C_TRACER_H
#include <stdint.h>

#define MAX_TRACE_GROUPS 131072

// Per-group data captured from DSC_Encode internal state
typedef struct {
    int group_idx;
    int slice_y;
    int qp;                 // stQp after RateControl
    int prev_qp;
    int coded_bits;         // codedGroupSize
    int buffer_fullness;
    int rc_range;
    int ich_selected;
    int mpp_selected[4];    // per-unit MPP flags
    int primary_qp;         // primaryQp for this group
} GroupTrace;

typedef struct {
    int num_groups;
    GroupTrace groups[MAX_TRACE_GROUPS];
    int total_enc_bits;
} DscTrace;

// Global trace buffer
extern DscTrace g_trace;

// Callback function (set by tb, called from dsc_codec.c)
void dsc_trace_callback_impl(void *state, int group_idx, int slice_y);

// Install the trace callback into dsc_codec's global pointer
void trace_install(void);

// Save trace to CSV
void trace_write_csv(const char *filename);

// Reset trace
void trace_reset(void);

#endif
