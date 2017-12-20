#if !defined(_TRACE_TEGRA_APB_DMA_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_TEGRA_APM_DMA_H

#include <linux/tracepoint.h>
#include <linux/dmaengine.h>

#undef TRACE_SYSTEM
#define TRACE_SYSTEM tegra_apb_dma

TRACE_EVENT(tegra_dma_tx_status,
	TP_PROTO(struct dma_chan *dc, s32 cookie, u32 residue),
	TP_ARGS(dc, cookie, residue),
	TP_STRUCT__entry(
		__field(struct dma_chan *, dc)
		__field(__s32,	cookie)
		__field(__u32,	residue)
	),
	TP_fast_assign(
		__entry->dc = dc;
		__entry->cookie = cookie;
		__entry->residue = residue;
	),
	TP_printk("channel %s: dma cookie %d, residue %u",
		  dev_name(&__entry->dc->dev->device),
		  __entry->cookie, __entry->residue)
);

TRACE_EVENT(tegra_dma_complete_cb,
	    TP_PROTO(struct dma_chan *dc, int count, void *ptr),
	    TP_ARGS(dc, count, ptr),
	    TP_STRUCT__entry(
		    __field(struct dma_chan *,	dc)
		    __field(int,		count)
		    __field(void *,		ptr)
		    ),
	    TP_fast_assign(
		    __entry->dc = dc;
		    __entry->count = count;
		    __entry->ptr = ptr;
		    ),
	    TP_printk("channel %s: done %d, ptr %p",
		      dev_name(&__entry->dc->dev->device),
		      __entry->count, __entry->ptr)
);

TRACE_EVENT(tegra_dma_tx_state,
	    TP_PROTO(struct dma_chan *dc, unsigned long ahb,
		     unsigned long wc, unsigned int done,
		     unsigned long byte_count, unsigned int residual),
	    TP_ARGS(dc, ahb, wc, done, byte_count, residual),
	    TP_STRUCT__entry(
		    __field(struct dma_chan *,	dc)
		    __field(unsigned long,	ahb)
		    __field(unsigned long,	wc)
		    __field(unsigned long,	done)
		    __field(unsigned int,	residual)
		    __field(unsigned long,	byte_count)
		    ),
	    TP_fast_assign(
		    __entry->dc = dc;
		    __entry->ahb = ahb;
		    __entry->wc = wc;
		    __entry->done = done;
		    __entry->residual = residual;
		    __entry->byte_count = byte_count;
		    ),
	    TP_printk("%s: txresidual: ahb %08lx wc %08lx => done %lu bc %lu residual %u",
		      dev_name(&__entry->dc->dev->device),
		      __entry->ahb, __entry->wc, __entry->done,
		      __entry->byte_count, __entry->residual)
);

TRACE_EVENT(tegra_dma_isr,
	    TP_PROTO(struct dma_chan *dc, int irq),
	    TP_ARGS(dc, irq),
	    TP_STRUCT__entry(
		    __field(struct dma_chan *,	dc)
		    __field(int,		irq)
		    ),
	    TP_fast_assign(
		    __entry->dc = dc;
		    __entry->irq = irq;
		    ),
	    TP_printk("%s: irq %d\n",  dev_name(&__entry->dc->dev->device),
		      __entry->irq));

#endif /*  _TRACE_TEGRADMA_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
