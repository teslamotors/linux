#ifndef _ES_D300_ROUTE_H
#define _ES_D300_ROUTE_H

#define MAX_CMD_LEN 16

struct route_tbl {
	u32 cmd[MAX_CMD_LEN];
	u32 mux_type;
	u32 cmd_len;
	u16 chn_mgr_mask;
};

struct out_mux_map {
	u32 port_desc; /* Combination of Port ID and Channel Number */
	u16 mux_id;
};

struct es_mux_info {
	u32 in_mux_start, in_mux_len;
	u32 out_mux_start, out_mux_len;
	void *in_tbl, *out_tbl;
};

/* Mapping of output MUX text */
enum {
	MUX_NONE,
	VP_CSOUT1,
	VP_CSOUT2,
	VP_FEOUT1,
	VP_FEOUT2,
	AUDIOFOCUS_CSOUT1,
	AUDIOFOCUS_CSOUT2,
	MM_AUDOUT1,
	MM_AUDOUT2,
	MM_PASSOUT1,
	MM_PASSOUT2,
	MM_MONOUT1,
	MM_MONOUT2,
	PASS_AUDOUT1,
	PASS_AUDOUT2,
	PASS_AUDOUT3,
	PASS_AUDOUT4,
	PASS_AO1,
	PASS_AO2,
	MONOUT1,
	MONOUT2,
	MONOUT3,
	MONOUT4,
};

enum {
	CMD_OUTPUT,
	CMD_INPUT,
};

#ifdef ALGO_BASE_ROUTES
/* Algorithms base route */
enum {
	ALGO_INVALID = -1,
	VP,
	PT,
	MM,
	AUDIOFOCUS,
	VP_MM,
	PT_VP,
	PT_VP_DSM,
	VP_DSM,
	AF_DSM,
	PT_COPY,
	PT_4CH,
	VP_2CSOUT,
	BASE_ROUTE_MAX,
};
#endif
/* MUX info structure for each base route */
extern struct es_mux_info es_kalaok_mux_info;
extern struct es_mux_info es_bargein_mux_info;
extern struct es_mux_info es_pt_vp_mux_info;
extern struct es_mux_info es_vp_mux_info;
extern struct es_mux_info es_audiofocus_mux_info;

/* Helper routines to build commands for any base route */
void prepare_mux_cmd(int mux, u32 *msg, u32 *msg_len,
		u16 *chn_mgr_mask, struct es_mux_info *, int type);

#endif /* _ES_D300_ROUTE_H */
