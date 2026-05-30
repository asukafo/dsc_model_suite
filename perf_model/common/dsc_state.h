// DSC SystemC Performance Model — Pipeline State / Register Transfer Data

#ifndef DSC_STATE_H
#define DSC_STATE_H

#include "dsc_config.h"

// ---- Constants (from dsc_types.h) ----
#define NUM_BUF_RANGES        15
#define NUM_COMPONENTS        4
#define MAX_UNITS_PER_GROUP   4
#define SAMPLES_PER_UNIT      3
#define MAX_PIXELS_PER_GROUP  6
#define ICH_BITS              5
#define ICH_SIZE              (1 << ICH_BITS)
#define BP_RANGE              13
#define BP_SIZE                3
#define PRED_BLK_SIZE          3
#define OFFSET_FRACTIONAL_BITS 11
#define PADDING_LEFT           5
#define PADDING_RIGHT          10
#define GROUPS_PER_SUPERGROUP   4
#define OVERFLOW_AVOID_THRESHOLD_422  (-224)
#define OVERFLOW_AVOID_THRESHOLD_444  (-172)

// ---- Quantization tables ----
extern const int QuantDivisor[];
extern const int QuantOffset[];

// ---- TLM Payload: Input to PREDICT stage ----
struct GroupInput {
    int orig[3][MAX_PIXELS_PER_GROUP];   // [component][pixel] original values
    int prevLine[3][MAX_PIXELS_PER_GROUP + 5];  // prev line samples around this group
    int hPos, vPos;
    int qp;                               // QP from rate control
    int prevLinePred;                     // BP vector for this block
    int groupCount;
    bool firstLine;
    bool native420, native422;
};

// ---- TLM Payload: Output from PREDICT → ENCODE ----
struct GroupPredicted {
    int quantizedResidual[MAX_UNITS_PER_GROUP][SAMPLES_PER_UNIT];
    int quantizedResidualMid[MAX_UNITS_PER_GROUP][SAMPLES_PER_UNIT];
    int maxError[MAX_UNITS_PER_GROUP];
    int maxMidError[MAX_UNITS_PER_GROUP];
    int maxIchError[MAX_UNITS_PER_GROUP];
    int predictedSize[MAX_UNITS_PER_GROUP];
    int leftRecon[MAX_UNITS_PER_GROUP];    // left-most reconstructed value per component
    int ichLookup[MAX_PIXELS_PER_GROUP];
    int ichPixels[MAX_PIXELS_PER_GROUP][NUM_COMPONENTS];
    int origWithinQerr[MAX_PIXELS_PER_GROUP];
    int recon[3][MAX_PIXELS_PER_GROUP];    // reconstructed pixels
    int firstFlat, flatnessType;
    int ichSelected;
    int qp;                               // echo QP back for encode
    int groupCount;
    int unitsPerGroup;
};

// ---- TLM Payload: Output from ENCODE → MUX + RATECTL ----
struct GroupEncoded {
    int codedGroupSize;
    int rcSizeUnit[MAX_UNITS_PER_GROUP];
    int midpointSelected[MAX_UNITS_PER_GROUP];
    int ichSelected;
    int useMidpoint[MAX_UNITS_PER_GROUP];
    int ichLookup[MAX_PIXELS_PER_GROUP];
    int qp;
    int recon[3][MAX_PIXELS_PER_GROUP];    // final reconstructed pixels (after MPP/ICH overwrite)
    int groupCount;
    int unitsPerGroup;
    // SSP FIFO data (per sub-stream)
    int encBalanceFullness[MAX_UNITS_PER_GROUP];
    int seSizePerUnit[MAX_UNITS_PER_GROUP];
};

// ---- QP feedback from RATECTL → PREDICT ----
struct QPUpdate {
    int primaryQp;
    int prevQp;
    int groupCount;
};

// ---- Line buffer access request/response ----
struct LineBufReq {
    int lineIdx;                          // which line to read
    int xStart, xEnd;                     // pixel range
    bool write;
    int data[MAX_PIXELS_PER_GROUP * 3];   // pixel data for writes
};

struct LineBufResp {
    int data[MAX_PIXELS_PER_GROUP * 3 + 10];  // pixel data (+margin)
    bool valid;
};

// ---- ostream operators for SystemC sc_fifo tracing ----
#include <ostream>
inline std::ostream &operator<<(std::ostream &os, const GroupInput &) {
    return os << "GroupInput";
}
inline std::ostream &operator<<(std::ostream &os, const GroupPredicted &) {
    return os << "GroupPredicted";
}
inline std::ostream &operator<<(std::ostream &os, const GroupEncoded &) {
    return os << "GroupEncoded";
}
inline std::ostream &operator<<(std::ostream &os, const QPUpdate &) {
    return os << "QPUpdate";
}

#endif // DSC_STATE_H
