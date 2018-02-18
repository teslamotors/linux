#ifndef BCMDHD_DYNAMIC_H
#define BCMDHD_DYNAMIC_H

#ifdef CONFIG_BCMDYNAMIC
extern int bcmdhd_chipid;
extern int bcmdhd_custom_ampdu_ba_wsize;
extern int bcmdhd_custom_glom_setting;
extern int bcmdhd_custom_rxchain;
extern int bcmdhd_custom_ampdu_mpdu;
extern int bcmdhd_custom_pspretend_thr;
extern int bcmdhd_wifi_turnon_delay;
extern int bcmdhd_wifi_turnoff_delay;

extern bool bcmdhd_use_custom_ampdu_mpdu;
extern bool bcmdhd_use_custom_pspretend_thr;

extern bool bcmdhd_custom_rxcb;
extern bool bcmdhd_prop_txstatus_vsdb;
extern bool bcmdhd_vsdb_bw_allocate_enable;
extern bool bcmdhd_bcmsdioh_txglom;
extern bool bcmdhd_use_wl_txbf;
extern bool bcmdhd_use_wl_frameburst;
extern bool bcmdhd_disable_roam_event;
extern bool bcmdhd_support_p2p_go_ps;
extern bool bcmdhd_wl11u;
extern bool bcmdhd_dhd_enable_lpc;
#else

/* Define the boolean vars so we don't have to depend on the dynamic config
   option everywhere */
#define bcmdhd_use_custom_ampdu_mpdu true
#define bcmdhd_use_custom_pspretend_thr true

#define bcmdhd_custom_rxcb true
#define bcmdhd_prop_txstatus_vsdb true
#define bcmdhd_vsdb_bw_allocate_enable true
#define bcmdhd_bcmsdioh_txglom true
#define bcmdhd_use_wl_txbf true
#define bcmdhd_use_wl_frameburst true
#define bcmdhd_disable_roam_event false
#define bcmdhd_support_p2p_go_ps true
#define bcmdhd_wl11u true
#define bcmdhd_dhd_enable_lpc true

#endif

#endif
