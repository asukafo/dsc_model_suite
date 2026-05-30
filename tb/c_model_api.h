// Thin C wrappers around DSC C model internals.

#ifndef C_MODEL_API_H
#define C_MODEL_API_H

// QP to quantization level (8bpc)
int dsc_api_qlevel(int qp, int component);
int dsc_api_quantize(int residual, int qlevel);
int dsc_api_residual_size(int eq);
int dsc_api_max_residual_size(int bpc, int component, int qp);
int dsc_api_map_predict(int a, int b, int c, int d, int e,
                        const int *prev_residuals, int qlevel,
                        int sample_idx, int max_val);

// Exact C model SamplePredict — takes individual pixel values
// prev[] = {p[-2],p[-1],p[0],p[1],p[2],p[3]} relative to hPos
// curr_left = pixel at hPos-1
int dsc_api_sample_predict_exact(
    const int prev[6],      // 6 prev_line pixels around hPos
    int curr_left,          // curr_line[hPos-1] (or midpoint for first group pixel)
    int h_pos, int pred_type, int qlevel,
    int unit, int unit_ctype,
    const int quant_residuals[3],
    int cpnt_bit_depth);

// Exact C model QuantizeResidual
int dsc_api_quantize_exact(int residual, int qlevel);

// ---- VLC bit-exact wrappers ----

// Escape code size for ICH mode
int dsc_api_escape_code_size(int bpc, int qp);

// QP-adjusted predicted size for next unit
int dsc_api_adj_pred_size(int qp, int prev_qp, int prev_pred_size,
                          int component, int bpc);

// Predict size for next group (from PredictSize in dsc_codec.c)
int dsc_api_predict_size(const int *required_sizes);

// Exact VLC unit bits — returns total bits for one unit
int dsc_api_vlc_unit_bits(
    int qp, int prev_qp, int unit, int is_unit0,
    int *quantized_residuals,    // [3] quantized residuals
    int *quantized_residual_mid, // [3] midpoint residuals
    int *predicted_size_inout,   // [1] in: prev pred, out: new pred
    int *prev_ich,               // in/out: prevIchSelected
    int *midpoint_selected,      // out: 1 if MPP
    int bpc, int convert_rgb,
    int first_flat, int flatness_type,
    int prev_first_flat, int prev_flatness_type,
    int group_count,             // 0-based group counter
    int *max_error, int *max_mid_error, int *max_ich_error,
    int *ich_indices, int num_ich_indices,
    int ich_lookup_valid         // whether ICH is valid for this group
);

#endif
