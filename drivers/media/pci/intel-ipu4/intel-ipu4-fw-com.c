/*
 * Copyright (c) 2013--2016 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 */

#include <asm/cacheflush.h>

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>

#include "intel-ipu4-fw-com.h"
#include "intel-ipu4-bus.h"

/*
 * FWCOM layer is a shared resource between FW and driver. It consist
 * of token queues to both send and receive directions. Queue is simply
 * an array of structures with read and write indexes to the queue.
 * There are 1...n queues to both directions. Queues locates in
 * system ram and are mapped to ISP MMU so that both CPU and ISP can
 * see the same buffer. Indexes are located in ISP DMEM so that FW code
 * can poll those with very low latency and cost. CPU access to indexes is
 * more costly but that happens only at message sending time and
 * interrupt trigged message handling. CPU doesn't need to poll indexes.
 * wr_reg / rd_reg are offsets to those dmem location. They are not
 * the indexes itself.
 */

/* Shared structure between driver and FW - do not modify */
struct sys_queue {
	uint64_t host_address;
	uint32_t vied_address;
	uint32_t size;
	uint32_t token_size;
	uint32_t wr_reg; /* Dmem location for port access */
	uint32_t rd_reg; /* Dmem location for port access */
	uint32_t _align;
};

/* Program load or explicit host setting should init to this */
#define SYSCOM_STATE_UNINIT 0x57A7E000
/* SP Syscom sets this when it is ready for use */
#define SYSCOM_STATE_READY 0x57A7E001
/* SP Syscom sets this when no more syscom accesses will happen */
#define SYSCOM_STATE_INACTIVE 0x57A7E002

/* Program load or explicit host setting should init to this */
#define SYSCOM_COMMAND_UNINIT 0x57A7F000
/* Host Syscom requests syscom to become inactive */
#define SYSCOM_COMMAND_INACTIVE 0x57A7F001

/* firmware config: data that sent from the host to SP via DDR */
/* Cell copies data into a context */

struct ia_css_syscom_config_fw {
	uint32_t firmware_address;

	uint32_t num_input_queues;
	uint32_t num_output_queues;

	/* ISP pointers to an array of sys_queue structures */
	uint32_t input_queue;
	uint32_t output_queue;

	/* ISYS / PSYS private data */
	uint32_t specific_addr;
	uint32_t specific_size;
};

/* End of shared structures / data */

struct intel_ipu4_fw_com_context {
	struct intel_ipu4_bus_device *adev;
	void __iomem *dmem_addr;
	int (*cell_ready)(struct intel_ipu4_bus_device *adev);
	void (*cell_start)(struct intel_ipu4_bus_device *adev);

	void *dma_buffer;
	dma_addr_t dma_addr;
	unsigned int dma_size;

	unsigned int num_input_queues;
	unsigned int num_output_queues;

	struct sys_queue *input_queue;	/* array of host to SP queues */
	struct sys_queue *output_queue; /* array of SP to host */

	void *config_host_addr;
	void *specific_host_addr;
	void *ibuf_host_addr;
	void *obuf_host_addr;

	uint32_t config_vied_addr;
	uint32_t input_queue_vied_addr;
	uint32_t output_queue_vied_addr;
	uint32_t specific_vied_addr;
	uint32_t ibuf_vied_addr;
	uint32_t obuf_vied_addr;
};

#define FW_COM_WR_REG 0
#define FW_COM_RD_REG 4

#define REGMEM_OFFSET 0

/* pass pkg_dir address to SPC in non-secure mode */
#define	PKG_DIR_ADDR_REG    0
/* pass syscom configuration to SPC */
#define	SYSCOM_CONFIG_REG   1
/* syscom state - modified by SP */
#define	SYSCOM_STATE_REG    2
/* syscom commands - modified by the host */
#define	SYSCOM_COMMAND_REG  3
/* first syscom queue pointer register */
#define	SYSCOM_QPR_BASE_REG 4

enum message_direction {
	DIR_RECV = 0,
	DIR_SEND
};

static unsigned int num_messages(unsigned int wr, unsigned int rd,
				 unsigned int size)
{
	if (wr < rd)
		wr += size;
	return wr - rd;
}

static unsigned num_free(unsigned int wr, unsigned int rd,
			 unsigned int size)
{
	return size - num_messages(wr, rd, size);
}

static unsigned int curr_index(void __iomem *q_dmem, struct sys_queue *q,
			       enum message_direction dir)
{
	return readl(q_dmem +
		     (dir == DIR_RECV ? FW_COM_RD_REG : FW_COM_WR_REG));
}

static unsigned int inc_index(void __iomem *q_dmem, struct sys_queue *q,
			      enum message_direction dir)
{
	unsigned int index;

	index = curr_index(q_dmem, q, dir) + 1;
	return index >= q->size ? 0 : index;
}

static unsigned int token_queue_size(
	struct ia_css_syscom_queue_config *tokenqueue)
{
	return tokenqueue->queue_size * tokenqueue->token_size;
}

static void sys_queue_init(unsigned int dmemindex,
			   struct intel_ipu4_fw_com_context *ctx,
			   struct ia_css_syscom_queue_config *cfg,
			   struct sys_queue *q,
			   void *hostbase,
			   uint32_t vied_base)
{
	q->size	      = cfg->queue_size;
	q->token_size = cfg->token_size;

	q->host_address = (uint64_t)hostbase;
	q->vied_address = vied_base;
	q->wr_reg = SYSCOM_QPR_BASE_REG + dmemindex * 2;
	q->rd_reg = SYSCOM_QPR_BASE_REG + 1 + dmemindex * 2;
}

void *intel_ipu4_fw_com_prepare(struct intel_ipu4_fw_com_cfg *cfg,
				struct intel_ipu4_bus_device *adev,
				void __iomem *base)
{
	struct intel_ipu4_fw_com_context *ctx;
	struct ia_css_syscom_config_fw *fw_cfg;
	unsigned int i, j;
	unsigned int queue_offset;
	unsigned int sizeall, offset;
	unsigned int sizeinput = 0, sizeoutput = 0;

	/* error handling */
	if (!cfg || !cfg->cell_start || !cfg->cell_ready)
		return NULL;

	ctx = kzalloc(sizeof(struct intel_ipu4_fw_com_context), GFP_KERNEL);
	if (!ctx)
		return NULL;
	ctx->dmem_addr = base + cfg->dmem_addr + REGMEM_OFFSET;
	ctx->adev = adev;
	ctx->cell_start = cfg->cell_start;
	ctx->cell_ready = cfg->cell_ready;

	ctx->num_input_queues  = cfg->num_input_queues;
	ctx->num_output_queues = cfg->num_output_queues;

	/*
	 * Allocate DMA mapped memory. Allocate one big chunk.
	 */
	sizeall =
		/* Base cfg for FW */
		roundup(sizeof(struct ia_css_syscom_config_fw), 8) +
		/* Descriptions of the queues */
		cfg->num_input_queues * sizeof(struct sys_queue) +
		cfg->num_output_queues * sizeof(struct sys_queue) +
		/* FW specific information structure */
		roundup(cfg->specific_size, 8);

	for (i = 0; i < cfg->num_input_queues; i++)
		sizeinput += token_queue_size(&cfg->input[i]);

	for (i = 0; i < cfg->num_output_queues; i++)
		sizeoutput += token_queue_size(&cfg->output[i]);

	sizeall += sizeinput + sizeoutput;

	ctx->dma_buffer = dma_alloc_attrs(&ctx->adev->dev, sizeall,
					  &ctx->dma_addr,
					  GFP_KERNEL, NULL);
	ctx->dma_size = sizeall;

	/* This is the address where FW starts to parse allocations */
	ctx->config_host_addr = ctx->dma_buffer;
	ctx->config_vied_addr = ctx->dma_addr;
	fw_cfg = (struct ia_css_syscom_config_fw *)ctx->config_host_addr;
	offset = roundup(sizeof(struct ia_css_syscom_config_fw), 8);

	ctx->input_queue = ctx->dma_buffer + offset;
	ctx->input_queue_vied_addr = ctx->dma_addr + offset;
	offset += cfg->num_input_queues * sizeof(struct sys_queue);

	ctx->output_queue = ctx->dma_buffer + offset;
	ctx->output_queue_vied_addr = ctx->dma_addr + offset;
	offset += cfg->num_output_queues * sizeof(struct sys_queue);

	ctx->specific_host_addr = ctx->dma_buffer + offset;
	ctx->specific_vied_addr = ctx->dma_addr + offset;
	offset += cfg->specific_size;

	ctx->ibuf_host_addr = ctx->dma_buffer + offset;
	ctx->ibuf_vied_addr = ctx->dma_addr + offset;
	offset += sizeinput;

	ctx->obuf_host_addr = ctx->dma_buffer + offset;
	ctx->obuf_vied_addr = ctx->dma_addr + offset;
	offset += sizeoutput;

	/* initialize input queues */
	queue_offset = 0;
	j = 0;
	for (i = 0; i < cfg->num_input_queues; i++, j++) {
		sys_queue_init(j, ctx,
			       &cfg->input[i],
			       &ctx->input_queue[i],
			       ctx->ibuf_host_addr + queue_offset,
			       ctx->ibuf_vied_addr + queue_offset);
		queue_offset += token_queue_size(&cfg->input[i]);
	}

	/* initialize output queues */
	queue_offset = 0;
	for (i = 0; i < cfg->num_output_queues; i++, j++) {
		sys_queue_init(j, ctx,
			       &cfg->output[i],
			       &ctx->output_queue[i],
			       ctx->obuf_host_addr + queue_offset,
			       ctx->obuf_vied_addr + queue_offset);
		queue_offset += token_queue_size(&cfg->output[i]);
	}

	/* copy firmware specific data */
	if (cfg->specific_addr && cfg->specific_size) {
		memcpy((void *)ctx->specific_host_addr,
		       cfg->specific_addr, cfg->specific_size);
	}

	fw_cfg->num_input_queues  = cfg->num_input_queues;
	fw_cfg->num_output_queues = cfg->num_output_queues;
	fw_cfg->input_queue	  = ctx->input_queue_vied_addr;
	fw_cfg->output_queue	  = ctx->output_queue_vied_addr;
	fw_cfg->specific_addr	  = ctx->specific_vied_addr;
	fw_cfg->specific_size	  = cfg->specific_size;

	clflush_cache_range(ctx->dma_buffer, sizeall);

	return ctx;
}
EXPORT_SYMBOL_GPL(intel_ipu4_fw_com_prepare);

int intel_ipu4_fw_com_open(struct intel_ipu4_fw_com_context *ctx)
{
	/* Check if SP is in valid state */
	if (!ctx->cell_ready(ctx->adev))
		return -EIO;

	/* store syscom uninitialized state */
	writel(SYSCOM_STATE_UNINIT,
	       ctx->dmem_addr + SYSCOM_STATE_REG * 4);
	/* store syscom uninitialized command */
	writel(SYSCOM_COMMAND_UNINIT,
	       ctx->dmem_addr + SYSCOM_COMMAND_REG * 4);
	/* store firmware configuration address */
	writel(ctx->config_vied_addr,
	       ctx->dmem_addr + SYSCOM_CONFIG_REG * 4);

	ctx->cell_start(ctx->adev);

	return 0;
}
EXPORT_SYMBOL_GPL(intel_ipu4_fw_com_open);

int intel_ipu4_fw_com_close(struct intel_ipu4_fw_com_context *ctx)
{
	int state;

	state = readl(ctx->dmem_addr + 4 * SYSCOM_STATE_REG);
	if (state != SYSCOM_STATE_READY)
		return -EBUSY;

	/* set close request flag */
	writel(SYSCOM_COMMAND_INACTIVE, ctx->dmem_addr +
	       SYSCOM_COMMAND_REG * 4);

	return 0;
}
EXPORT_SYMBOL_GPL(intel_ipu4_fw_com_close);

int intel_ipu4_fw_com_release(struct intel_ipu4_fw_com_context *ctx,
			      unsigned int force)
{
	/* check if release is forced, an verify cell state if it is not */
	if (!force && !ctx->cell_ready(ctx->adev))
		return -EBUSY;

	dma_free_attrs(&ctx->adev->dev, ctx->dma_size,
		       ctx->dma_buffer, ctx->dma_addr, NULL);
	kfree(ctx);
	return 0;
}
EXPORT_SYMBOL_GPL(intel_ipu4_fw_com_release);

int intel_ipu4_fw_com_ready(struct intel_ipu4_fw_com_context *ctx)
{
	int state;

	/* check if SP syscom is ready to open the queue */
	state = readl(ctx->dmem_addr +	SYSCOM_STATE_REG * 4);
	if (state != SYSCOM_STATE_READY)
		return -EBUSY; /* SPC is not ready to handle messages yet */

	return 0;
}
EXPORT_SYMBOL_GPL(intel_ipu4_fw_com_ready);

static bool is_index_valid(struct sys_queue *q, unsigned int index)
{
	if (index >= q->size)
		return false;
	return true;
}

void *intel_ipu4_send_get_token(struct intel_ipu4_fw_com_context *ctx,
				int q_nbr)
{
	struct sys_queue *q = &ctx->input_queue[q_nbr];
	void __iomem *q_dmem = ctx->dmem_addr + q->wr_reg * 4;
	unsigned int wr, rd;
	unsigned int packets;
	unsigned int index;

	wr = readl(q_dmem + FW_COM_WR_REG);
	rd = readl(q_dmem + FW_COM_RD_REG);

	/* Catch indexes in dmem */
	if (!is_index_valid(q, wr) || !is_index_valid(q, rd))
		return NULL;

	packets = num_free(wr + 1, rd, q->size);
	if (packets <= 0)
		return NULL;

	index = curr_index(q_dmem, q, DIR_SEND);

	return (void *)q->host_address + (index * q->token_size);
}
EXPORT_SYMBOL_GPL(intel_ipu4_send_get_token);

void intel_ipu4_send_put_token(struct intel_ipu4_fw_com_context *ctx, int q_nbr)
{

	struct sys_queue *q = &ctx->input_queue[q_nbr];
	void __iomem *q_dmem = ctx->dmem_addr + q->wr_reg * 4;
	int index = curr_index(q_dmem, q, DIR_SEND);
	void *addr = (void *)q->host_address + (index * q->token_size);

	clflush_cache_range(addr, q->token_size);

	/* Increment index */
	index = inc_index(q_dmem, q, DIR_SEND);

	writel(index, q_dmem + FW_COM_WR_REG);
}
EXPORT_SYMBOL_GPL(intel_ipu4_send_put_token);

void *intel_ipu4_recv_get_token(struct intel_ipu4_fw_com_context *ctx,
				int q_nbr)
{
	struct sys_queue *q = &ctx->output_queue[q_nbr];
	void __iomem *q_dmem = ctx->dmem_addr + q->wr_reg * 4;
	unsigned int wr, rd;
	unsigned int packets;
	void *addr;

	wr = readl(q_dmem + FW_COM_WR_REG);
	rd = readl(q_dmem + FW_COM_RD_REG);

	/* Catch indexes in dmem? */
	if (!is_index_valid(q, wr) || !is_index_valid(q, rd))
		return NULL;

	packets = num_messages(wr, rd, q->size);
	if (packets <= 0)
		return NULL;

	rd = curr_index(q_dmem, q, DIR_RECV);

	addr = (void *)q->host_address + (rd * q->token_size);
	clflush_cache_range(addr,  q->token_size);

	return addr;
}
EXPORT_SYMBOL_GPL(intel_ipu4_recv_get_token);

void intel_ipu4_recv_put_token(struct intel_ipu4_fw_com_context *ctx, int q_nbr)
{
	struct sys_queue *q = &ctx->output_queue[q_nbr];
	void __iomem *q_dmem = ctx->dmem_addr + q->wr_reg * 4;
	unsigned int rd = inc_index(q_dmem, q, DIR_RECV);

	/* Release index */
	writel(rd, q_dmem + FW_COM_RD_REG);
}
EXPORT_SYMBOL_GPL(intel_ipu4_recv_put_token);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Intel intel_ipu4 fw comm library");

