// DSC Algorithm Functions — embedded in each SC_MODULE
// All DSC 1.2a algorithmic constants and helpers for prediction/VLC/RC.
// No external C model dependency.
#ifndef DSC_ALGO_H
#define DSC_ALGO_H
#include <cstdlib>
#include <cmath>

// ---- Quantization tables (from VESA DSC 1.2a) ----
inline const int QuantDivisor[] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512,
                             1024, 2048, 4096, 8192, 16384, 32768, 65536};
inline const int QuantOffset[]  = {0, 0, 1, 3, 7, 15, 31, 63, 127, 255,
                             511, 1023, 2047, 4095, 8191, 16383, 32767};

// QP→qlevel LUTs (8bpc)
inline const int ql_luma_8[]   = {0,0,0,1,1,2,2,3,3,4,4,5,5,5,6,7};
inline const int ql_chroma_8[] = {0,1,2,2,3,3,4,4,5,5,6,6,7,8,8,8};

inline int qlevel(int qp, int cpnt) {
    qp = qp<0?0:(qp>15?15:qp);
    return cpnt==0 ? ql_luma_8[qp] : ql_chroma_8[qp];
}

// ---- Quantize ----
inline int quantize(int residual, int ql) {
    if (residual > 0) return (residual + QuantOffset[ql]) >> ql;
    return -((QuantOffset[ql] - residual) >> ql);
}

// ---- Dequantize ----
inline int dequant(int eq, int ql) { return eq * QuantDivisor[ql]; }

// ---- FindResidualSize (from dsc_codec.c) ----
inline int residual_size(int eq) {
    if (eq == 0) return 0;
    int a = abs(eq);
    if (a <= 1)  return 1;  if (a <= 2)  return 2;
    if (a <= 4)  return 3;  if (a <= 8)  return 4;
    if (a <= 16) return 5;  if (a <= 32) return 6;
    if (a <= 64) return 7;
    return 8;
}

// ---- Max residual size ----
inline int max_residual_sz(int bpc, int cpnt, int qp, bool convert_rgb) {
    int bd = bpc + ((convert_rgb && cpnt!=0 && bpc!=16) ? 1 : 0);
    int ql = qlevel(qp, cpnt);
    return bd - ql;
}

// ---- MAP prediction (from dsc_codec.c SamplePredict) ----
#define FILT3(a,b,c) (((a)+2*(b)+(c)+2)>>2)
#define CLAMP3(X,LO,HI) ((X)>(HI)?(HI):((X)<(LO)?(LO):(X)))
#define MAX3(X,Y) ((X)>(Y)?(X):(Y))
#define MIN3(X,Y) ((X)<(Y)?(X):(Y))

inline int map_predict(int prev[6], int curr_left, int hpos, int ql,
                       const int qres[3]) {
    int p0=prev[0],p1=prev[1],p2=prev[2],p3=prev[3],p4=prev[4],p5=prev[5];
    int b=p2,c=p1,d=p3,e=p4,a=curr_left;
    int fc=FILT3(p0,p1,p2),fb=FILT3(p1,p2,p3),fd=FILT3(p2,p3,p4),fe=FILT3(p3,p4,p5);
    int bc=c+CLAMP3(fc-c,-(QuantDivisor[ql]/2),QuantDivisor[ql]/2);
    int bb=b+CLAMP3(fb-b,-(QuantDivisor[ql]/2),QuantDivisor[ql]/2);
    int bd=d+CLAMP3(fd-d,-(QuantDivisor[ql]/2),QuantDivisor[ql]/2);
    int be=e+CLAMP3(fe-e,-(QuantDivisor[ql]/2),QuantDivisor[ql]/2);
    if (hpos/3==0) bc=a;
    if ((hpos%3)==0)
        return CLAMP3(a+bb-bc,MIN3(a,bb),MAX3(a,bb));
    else if ((hpos%3)==1)
        return CLAMP3(a+bd-bc+qres[0]*QuantDivisor[ql],
                      MIN3(MIN3(a,bb),bd),MAX3(MAX3(a,bb),bd));
    else
        return CLAMP3(a+be-bc+(qres[0]+qres[1])*QuantDivisor[ql],
                      MIN3(MIN3(a,bb),MIN3(bd,be)),MAX3(MAX3(a,bb),MAX3(bd,be)));
}

inline int left_predict(int a, int hpos, int ql, const int qres[3], int maxv) {
    if ((hpos%3)==0) return a;
    if ((hpos%3)==1) return CLAMP3(a+qres[0]*QuantDivisor[ql],0,maxv);
    return CLAMP3(a+(qres[0]+qres[1])*QuantDivisor[ql],0,maxv);
}

// ---- Midpoint predictor (MPP) ----
inline int midpoint_pred(int maxv, int left_recon, int ql) {
    return maxv/2 + (left_recon % (1<<ql));
}

// ---- VLC prefix size ----
inline int prefix_size(int pred, int req) {
    return req <= pred ? 1 : req - pred + 1;
}

// ---- VLC predicted size update (PredictSize) ----
inline int predict_size(const int req_sz[3]) {
    int m = req_sz[0];
    for (int i=1;i<3;i++) if(req_sz[i]>m) m=req_sz[i];
    return m;
}

// ---- ICH escape code size ----
inline int escape_code_size(int bpc, int qp) {
    return bpc + 1 - qlevel(qp, 0);
}

// ---- Clamp ----
inline int clp(int v, int lo, int hi) { return v<lo?lo:(v>hi?hi:v); }

#endif
