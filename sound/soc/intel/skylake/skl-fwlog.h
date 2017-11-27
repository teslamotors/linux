#include "../common/sst-dsp.h"
#include "../common/sst-dsp-priv.h"
#include "skl.h"

#ifndef __SKL_FWLOG_H__
#define __SKL_FWLOG_H__

int skl_dsp_init_trace_window(struct sst_dsp *sst, u32 *wp, u32 offset,
				u32 size, int nr_cores);
int skl_dsp_init_log_buffer(struct sst_dsp *sst, int size,
				int core, struct snd_compr_stream *stream);
unsigned long skl_dsp_log_avail(struct sst_dsp *sst, int core);
void skl_dsp_write_log(struct sst_dsp *sst, void __iomem *src, int core,
			int count);
int skl_dsp_copy_log_user(struct sst_dsp *sst, int core, void __user *dest,
				int count);
void skl_dsp_get_log_buff(struct sst_dsp *sst, int core);
void skl_dsp_put_log_buff(struct sst_dsp *sst, int core);
void skl_dsp_done_log_buffer(struct sst_dsp *sst, int core);
int skl_dsp_get_buff_users(struct sst_dsp *sst, int core);
int update_dsp_log_priority(int value, struct skl *skl);
int get_dsp_log_priority(struct skl *skl);
#endif /* __SKL_FWLOG_H__ */
