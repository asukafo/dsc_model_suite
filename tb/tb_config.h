// Standalone DSC configuration setup (no dependency on codec_main.c)
#ifndef TB_CONFIG_H
#define TB_CONFIG_H
#include "dsc_tb.h"

// Fill dsc_cfg_t from TbConfig + RC tables
// Returns 0 on success, -1 on error
int  dsc_setup_config(dsc_cfg_t *cfg, const TbConfig *tb);
void dsc_setup_rc_8bpc_8bpp(dsc_cfg_t *cfg);
void dsc_setup_rc_8bpc_12bpp(dsc_cfg_t *cfg);
void dsc_setup_rc_10bpc_8bpp(dsc_cfg_t *cfg);
void dsc_setup_rc_10bpc_12bpp(dsc_cfg_t *cfg);

#endif
