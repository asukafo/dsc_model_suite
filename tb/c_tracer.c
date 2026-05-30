// C Model Tracer — callback implementation
#include <stdio.h>
#include <string.h>
#include "c_tracer.h"
#include "dsc_types.h"

DscTrace g_trace;

void trace_reset(void) {
    memset(&g_trace, 0, sizeof(g_trace));
}

// Declare the global function pointer (defined in dsc_codec.c)
extern void (*dsc_group_trace_cb)(void *state, int group_idx, int slice_y);

void trace_install(void) {
    dsc_group_trace_cb = dsc_trace_callback_impl;
}

// Called from dsc_codec.c after each group's RateControl completes
void dsc_trace_callback_impl(void *state, int group_idx, int slice_y) {
    if (g_trace.num_groups >= MAX_TRACE_GROUPS) return;
    dsc_state_t *s = (dsc_state_t*)state;

    GroupTrace *g = &g_trace.groups[g_trace.num_groups++];
    g->group_idx      = group_idx;
    g->slice_y        = slice_y;
    g->qp             = s->stQp;
    g->prev_qp         = s->prevQp;
    g->coded_bits     = s->codedGroupSize;
    g->buffer_fullness = s->bufferFullness;
    g->ich_selected   = s->ichSelected;
    g->primary_qp     = s->primaryQp;
    for (int u = 0; u < 4; u++)
        g->mpp_selected[u] = s->midpointSelected[u];
}

void trace_write_csv(const char *filename) {
    FILE *f = fopen(filename, "w");
    if (!f) { fprintf(stderr, "Cannot open %s\n", filename); return; }
    fprintf(f, "group,slice_y,qp,prev_qp,coded_bits,buffer_fullness,ich,primary_qp,mpp0,mpp1,mpp2,mpp3\n");
    for (int i = 0; i < g_trace.num_groups; i++) {
        GroupTrace *g = &g_trace.groups[i];
        fprintf(f, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
                g->group_idx, g->slice_y, g->qp, g->prev_qp,
                g->coded_bits, g->buffer_fullness, g->ich_selected, g->primary_qp,
                g->mpp_selected[0], g->mpp_selected[1],
                g->mpp_selected[2], g->mpp_selected[3]);
    }
    fclose(f);
    printf("Trace: %d groups written to %s\n", g_trace.num_groups, filename);
}
