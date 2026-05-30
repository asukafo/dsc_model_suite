// DSC SystemC — Stage 2: ENCODE (bit-exact VLC with rate-distortion ICH decision)
#include "encode.h"
#include <cstring>
#include <cmath>

extern "C" {
#include "c_model_api.h"
}

static int  g_pred_size[4]={1,1,1,1}, g_prev_qp=0, g_prev_ich=0, g_group_cnt=0;
static int  g_first_flat=-1, g_flatness_type=0, g_prev_first_flat=-1, g_prev_flatness_type=0;

// Compute group bits WITHOUT ICH (for comparison)
static int group_bits_pmode(int nU, GroupPredicted &gp, int qp, int bpc, bool cr) {
    int pred_sz[4]; for(int u=0;u<nU;u++)pred_sz[u]=g_pred_size[u];
    int prev_ich=g_prev_ich, tb=0, max_err[4]={0},max_mid[4]={0},max_ich[4]={0};
    int ich_lut[6]={0};for(int p=0;p<3;p++)ich_lut[p]=p;
    for(int u=0;u<nU;u++){max_err[u]=gp.maxError[u];}
    for(int u=0;u<nU;u++){
        int mpp=0,ps=pred_sz[u];
        tb+=dsc_api_vlc_unit_bits(qp,g_prev_qp,u,(u==0),
            gp.quantizedResidual[u],gp.quantizedResidualMid[u],
            &ps,&prev_ich,&mpp,bpc,cr,
            g_first_flat,g_flatness_type,g_prev_first_flat,g_prev_flatness_type,
            g_group_cnt,&max_err[u],&max_mid[u],&max_ich[u],ich_lut,3,0/*no ICH*/);
        pred_sz[u]=ps;
    }
    return tb;
}

// Compute group bits WITH ICH (for comparison)
static int group_bits_ich(int nU, GroupPredicted &gp, int qp, int bpc, bool cr) {
    int pred_sz[4]; for(int u=0;u<nU;u++)pred_sz[u]=g_pred_size[u];
    int prev_ich=g_prev_ich, tb=0, max_err[4]={0},max_mid[4]={0},max_ich[4]={0};
    int ich_lut[6]={0};for(int p=0;p<3;p++)ich_lut[p]=p;
    for(int u=0;u<nU;u++){max_err[u]=gp.maxError[u];}
    for(int u=0;u<nU;u++){
        int mpp=0,ps=pred_sz[u];
        tb+=dsc_api_vlc_unit_bits(qp,g_prev_qp,u,(u==0),
            gp.quantizedResidual[u],gp.quantizedResidualMid[u],
            &ps,&prev_ich,&mpp,bpc,cr,
            g_first_flat,g_flatness_type,g_prev_first_flat,g_prev_flatness_type,
            g_group_cnt,&max_err[u],&max_mid[u],&max_ich[u],ich_lut,3,1/*ICH*/);
        pred_sz[u]=ps;
    }
    return tb;
}

void Encode::process() {
    GroupPredicted gp; GroupEncoded ge;
    wait(); StageTimer timer(&perf().encode);
    memset(g_pred_size,0,sizeof(g_pred_size));for(int i=0;i<4;i++)g_pred_size[i]=1;
    g_prev_qp=g_prev_ich=g_group_cnt=0;g_first_flat=g_prev_first_flat=-1;
    g_flatness_type=g_prev_flatness_type=0;

    while(true){
        timer.pre_read();gp=in_port->read();timer.post_read();
        memset(&ge,0,sizeof(ge));
        ge.qp=gp.qp;ge.groupCount=gp.groupCount;ge.unitsPerGroup=gp.unitsPerGroup;
        memcpy(ge.recon,gp.recon,sizeof(ge.recon));

        int qp=gp.qp,bpc=cfg->bits_per_component,nU=gp.unitsPerGroup;
        bool cr=cfg->convert_rgb, is422=cfg->native_422;

        // Flatness tracking
        if(g_group_cnt%GROUPS_PER_SUPERGROUP==0){g_first_flat=-1;}
        bool is_flat=true;
        if(is_flat&&g_first_flat<0)g_first_flat=g_group_cnt%GROUPS_PER_SUPERGROUP;

        // === RATE-DISTORTION ICH DECISION ===
        int bits_p = group_bits_pmode(nU, gp, qp, bpc, cr);
        int bits_i = group_bits_ich(nU, gp, qp, bpc, cr);
        bool use_ich = gp.ichSelected && (bits_i < bits_p); // only if cheaper
        ge.ichSelected = use_ich ? 1 : 0;

        // Encode with selected mode
        int tb=0, prev_ich_copy=g_prev_ich;
        int max_err[4]={0},max_mid[4]={0},max_ich[4]={0};
        int ich_lut[6]={0};for(int p=0;p<3;p++)ich_lut[p]=p;
        for(int u=0;u<nU;u++){max_err[u]=gp.maxError[u];}

        for(int u=0;u<nU;u++){
            int mpp=0,ps=g_pred_size[u];
            tb+=dsc_api_vlc_unit_bits(qp,g_prev_qp,u,(u==0),
                gp.quantizedResidual[u],gp.quantizedResidualMid[u],
                &ps,&prev_ich_copy,&mpp,bpc,cr,
                g_first_flat,g_flatness_type,g_prev_first_flat,g_prev_flatness_type,
                g_group_cnt,&max_err[u],&max_mid[u],&max_ich[u],
                ich_lut,3, use_ich?1:0);
            g_pred_size[u]=ps;
            if(mpp){perf().mpp_units++;ge.midpointSelected[u]=1;}
        }

        if(use_ich)perf().ich_groups++;
        g_prev_ich=use_ich?1:0;
        ge.codedGroupSize=tb;perf().total_enc_bits+=tb;
        g_prev_qp=qp;g_group_cnt++;
        g_prev_first_flat=g_first_flat;g_prev_flatness_type=g_flatness_type;
        if(g_group_cnt%GROUPS_PER_SUPERGROUP==0){g_first_flat=-1;}

        wait(nU+1);timer.add_busy(nU+1);
        timer.pre_write();out_mux->write(ge);out_rc->write(ge);timer.post_write();
    }
}
