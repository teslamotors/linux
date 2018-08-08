/**
* Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2010 - 2018, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
*/

#include "ia_css_syscom.h"

#include "ia_css_syscom_context.h"
#include "ia_css_syscom_config_fw.h"
#include "ia_css_syscom_trace.h"

#include "queue.h"
#include "send_port.h"
#include "recv_port.h"
#include "regmem_access.h"

#include "error_support.h"
#include "cpu_mem_support.h"

#include "queue_struct.h"
#include "send_port_struct.h"
#include "recv_port_struct.h"

#include "type_support.h"
#include <vied/shared_memory_access.h>
#include <vied/shared_memory_map.h>
#include "platform_support.h"

#include "ia_css_cell.h"

/* struct of internal buffer sizes */
struct ia_css_syscom_size_intern {
	unsigned int context;
	unsigned int input_queue;
	unsigned int output_queue;
	unsigned int input_port;
	unsigned int output_port;

	unsigned int fw_config;
	unsigned int specific;

	unsigned int input_buffer;
	unsigned int output_buffer;
};

/* Allocate buffers internally, when no buffers are provided */
static int
ia_css_syscom_alloc(
	unsigned int ssid,
	unsigned int mmid,
	const struct ia_css_syscom_size *size,
	struct ia_css_syscom_buf *buf)
{
	/* zero the buffer to set all pointers to zero */
	memset(buf, 0, sizeof(*buf));

	/* allocate cpu_mem */
	buf->cpu = (char *)ia_css_cpu_mem_alloc(size->cpu);
	if (!buf->cpu)
		goto EXIT7;

	/* allocate and map shared config buffer */
	buf->shm_host = shared_memory_alloc(mmid, size->shm);
	if (!buf->shm_host)
		goto EXIT6;
	buf->shm_cell = shared_memory_map(ssid, mmid, buf->shm_host);
	if (!buf->shm_cell)
		goto EXIT5;

	/* allocate and map input queue buffer */
	buf->ibuf_host = shared_memory_alloc(mmid, size->ibuf);
	if (!buf->ibuf_host)
		goto EXIT4;
	buf->ibuf_cell = shared_memory_map(ssid, mmid, buf->ibuf_host);
	if (!buf->ibuf_cell)
		goto EXIT3;

	/* allocate and map output queue buffer */
	buf->obuf_host = shared_memory_alloc(mmid, size->obuf);
	if (!buf->obuf_host)
		goto EXIT2;
	buf->obuf_cell = shared_memory_map(ssid, mmid, buf->obuf_host);
	if (!buf->obuf_cell)
		goto EXIT1;

	return 0;

EXIT1:	shared_memory_free(mmid, buf->obuf_host);
EXIT2:	shared_memory_unmap(ssid, mmid, buf->ibuf_cell);
EXIT3:	shared_memory_free(mmid, buf->ibuf_host);
EXIT4:	shared_memory_unmap(ssid, mmid, buf->shm_cell);
EXIT5:	shared_memory_free(mmid, buf->shm_host);
EXIT6:	ia_css_cpu_mem_free(buf->cpu);
EXIT7:	return FW_ERROR_NO_MEMORY;
}

static void
ia_css_syscom_size_intern(
	const struct ia_css_syscom_config *cfg,
	struct ia_css_syscom_size_intern *size)
{
	/* convert syscom config into syscom internal size struct */

	unsigned int i;

	size->context = sizeof(struct ia_css_syscom_context);
	size->input_queue = cfg->num_input_queues * sizeof(struct sys_queue);
	size->output_queue = cfg->num_output_queues * sizeof(struct sys_queue);
	size->input_port = cfg->num_input_queues * sizeof(struct send_port);
	size->output_port = cfg->num_output_queues * sizeof(struct recv_port);

	size->fw_config = sizeof(struct ia_css_syscom_config_fw);
	size->specific = cfg->specific_size;

	/* accumulate input queue buffer sizes */
	size->input_buffer = 0;
	for (i = 0; i < cfg->num_input_queues; i++) {
		size->input_buffer +=
			sys_queue_buf_size(cfg->input[i].queue_size,
					cfg->input[i].token_size);
	}

	/* accumulate outut queue buffer sizes */
	size->output_buffer = 0;
	for (i = 0; i < cfg->num_output_queues; i++) {
		size->output_buffer +=
			sys_queue_buf_size(cfg->output[i].queue_size,
					cfg->output[i].token_size);
	}
}

static void
ia_css_syscom_size_extern(
	const struct ia_css_syscom_size_intern *i,
	struct ia_css_syscom_size *e)
{
	/* convert syscom internal size struct into external size struct */

	e->cpu = i->context + i->input_queue + i->output_queue +
		 i->input_port + i->output_port;
	e->shm = i->fw_config + i->input_queue + i->output_queue + i->specific;
	e->ibuf = i->input_buffer;
	e->obuf = i->output_buffer;
}

/* Function that provides buffer sizes to be allocated */
void
ia_css_syscom_size(
	const struct ia_css_syscom_config *cfg,
	struct ia_css_syscom_size *size)
{
	struct ia_css_syscom_size_intern i;

	ia_css_syscom_size_intern(cfg, &i);
	ia_css_syscom_size_extern(&i, size);
}

static struct ia_css_syscom_context*
ia_css_syscom_assign_buf(
	const struct ia_css_syscom_size_intern *i,
	const struct ia_css_syscom_buf *buf)
{
	struct ia_css_syscom_context *ctx;
	char *cpu_mem_buf;
	host_virtual_address_t shm_buf_host;
	vied_virtual_address_t shm_buf_cell;

	/* host context */
	cpu_mem_buf = buf->cpu;

	ctx = (struct ia_css_syscom_context *)cpu_mem_buf;
	ia_css_cpu_mem_set_zero(ctx, i->context);
	cpu_mem_buf += i->context;

	ctx->input_queue = (struct sys_queue *) cpu_mem_buf;
	cpu_mem_buf += i->input_queue;

	ctx->output_queue = (struct sys_queue *) cpu_mem_buf;
	cpu_mem_buf += i->output_queue;

	ctx->send_port = (struct send_port *) cpu_mem_buf;
	cpu_mem_buf += i->input_port;

	ctx->recv_port = (struct recv_port *) cpu_mem_buf;


	/* cell config */
	shm_buf_host = buf->shm_host;
	shm_buf_cell = buf->shm_cell;

	ctx->config_host_addr = shm_buf_host;
	shm_buf_host += i->fw_config;
	ctx->config_vied_addr = shm_buf_cell;
	shm_buf_cell += i->fw_config;

	ctx->input_queue_host_addr = shm_buf_host;
	shm_buf_host += i->input_queue;
	ctx->input_queue_vied_addr = shm_buf_cell;
	shm_buf_cell += i->input_queue;

	ctx->output_queue_host_addr = shm_buf_host;
	shm_buf_host += i->output_queue;
	ctx->output_queue_vied_addr = shm_buf_cell;
	shm_buf_cell += i->output_queue;

	ctx->specific_host_addr = shm_buf_host;
	ctx->specific_vied_addr = shm_buf_cell;

	ctx->ibuf_host_addr = buf->ibuf_host;
	ctx->ibuf_vied_addr = buf->ibuf_cell;

	ctx->obuf_host_addr = buf->obuf_host;
	ctx->obuf_vied_addr = buf->obuf_cell;

	return ctx;
}

struct ia_css_syscom_context*
ia_css_syscom_open(
	struct ia_css_syscom_config *cfg,
	struct ia_css_syscom_buf  *buf_extern
)
{
	struct ia_css_syscom_size_intern size_intern;
	struct ia_css_syscom_size size;
	struct ia_css_syscom_buf buf_intern;
	struct ia_css_syscom_buf *buf;
	struct ia_css_syscom_context *ctx;
	struct ia_css_syscom_config_fw fw_cfg;
	unsigned int i;
	struct sys_queue_res res;

	IA_CSS_TRACE_0(SYSCOM, INFO, "Entered: ia_css_syscom_open\n");

	/* error handling */
	if (cfg == NULL)
		return NULL;

	IA_CSS_TRACE_1(SYSCOM, INFO, "ia_css_syscom_open (secure %d) start\n", cfg->secure);

	/* check members of cfg: TBD */

	/*
	 * Check if SP is in valid state, have to wait if not ready.
	 * In some platform (Such as VP), it will need more time to wait due to system performance;
	 * If return NULL without wait for SPC0 ready, Driver load FW will failed
	 */
	ia_css_cell_wait(cfg->ssid, SPC0);

	ia_css_syscom_size_intern(cfg, &size_intern);
	ia_css_syscom_size_extern(&size_intern, &size);

	if (buf_extern) {
		/* use externally allocated buffers */
		buf = buf_extern;
	} else {
		/* use internally allocated buffers */
		buf = &buf_intern;
		if (ia_css_syscom_alloc(cfg->ssid, cfg->mmid, &size, buf) != 0)
			return NULL;
	}

	/* assign buffer pointers */
	ctx = ia_css_syscom_assign_buf(&size_intern, buf);
	/* only need to free internally allocated buffers */
	ctx->free_buf = !buf_extern;

	ctx->cell_regs_addr = cfg->regs_addr;
	/* regmem is at cell_dmem_addr + REGMEM_OFFSET */
	ctx->cell_dmem_addr = cfg->dmem_addr;

	ctx->num_input_queues		= cfg->num_input_queues;
	ctx->num_output_queues		= cfg->num_output_queues;

	ctx->env.mmid = cfg->mmid;
	ctx->env.ssid = cfg->ssid;
	ctx->env.mem_addr = cfg->dmem_addr;

	ctx->regmem_idx = SYSCOM_QPR_BASE_REG;

	/* initialize input queues */
	res.reg = SYSCOM_QPR_BASE_REG;
	res.host_address = ctx->ibuf_host_addr;
	res.vied_address = ctx->ibuf_vied_addr;
	for (i = 0; i < cfg->num_input_queues; i++) {
		sys_queue_init(ctx->input_queue + i,
			cfg->input[i].queue_size,
			cfg->input[i].token_size, &res);
	}

	/* initialize output queues */
	res.host_address = ctx->obuf_host_addr;
	res.vied_address = ctx->obuf_vied_addr;
	for (i = 0; i < cfg->num_output_queues; i++) {
		sys_queue_init(ctx->output_queue + i,
			cfg->output[i].queue_size,
			cfg->output[i].token_size, &res);
	}

	/* fill shared queue structs */
	shared_memory_store(cfg->mmid, ctx->input_queue_host_addr,
			    ctx->input_queue,
			    cfg->num_input_queues * sizeof(struct sys_queue));
	ia_css_cpu_mem_cache_flush(
		(void *)HOST_ADDRESS(ctx->input_queue_host_addr),
		cfg->num_input_queues * sizeof(struct sys_queue));
	shared_memory_store(cfg->mmid, ctx->output_queue_host_addr,
			    ctx->output_queue,
			    cfg->num_output_queues * sizeof(struct sys_queue));
	ia_css_cpu_mem_cache_flush(
		(void *)HOST_ADDRESS(ctx->output_queue_host_addr),
		cfg->num_output_queues * sizeof(struct sys_queue));

	/* Zero the queue buffers. Is this really needed?  */
	shared_memory_zero(cfg->mmid, buf->ibuf_host, size.ibuf);
	ia_css_cpu_mem_cache_flush((void *)HOST_ADDRESS(buf->ibuf_host),
				   size.ibuf);
	shared_memory_zero(cfg->mmid, buf->obuf_host, size.obuf);
	ia_css_cpu_mem_cache_flush((void *)HOST_ADDRESS(buf->obuf_host),
				   size.obuf);

	/* copy firmware specific data */
	if (cfg->specific_addr && cfg->specific_size) {
		shared_memory_store(cfg->mmid, ctx->specific_host_addr,
				    cfg->specific_addr, cfg->specific_size);
		ia_css_cpu_mem_cache_flush(
			(void *)HOST_ADDRESS(ctx->specific_host_addr),
			cfg->specific_size);
	}

	fw_cfg.num_input_queues  = cfg->num_input_queues;
	fw_cfg.num_output_queues = cfg->num_output_queues;
	fw_cfg.input_queue       = ctx->input_queue_vied_addr;
	fw_cfg.output_queue      = ctx->output_queue_vied_addr;
	fw_cfg.specific_addr     = ctx->specific_vied_addr;
	fw_cfg.specific_size     = cfg->specific_size;

	shared_memory_store(cfg->mmid, ctx->config_host_addr,
			    &fw_cfg, sizeof(struct ia_css_syscom_config_fw));
	ia_css_cpu_mem_cache_flush((void *)HOST_ADDRESS(ctx->config_host_addr),
				    sizeof(struct ia_css_syscom_config_fw));

#if !HAS_DUAL_CMD_CTX_SUPPORT
	/* store syscom uninitialized state */
	IA_CSS_TRACE_3(SYSCOM, INFO, "ia_css_syscom_open store STATE_REG (%#x) @ dmem_addr %#x ssid %d\n",
		       SYSCOM_STATE_UNINIT, ctx->cell_dmem_addr, cfg->ssid);
	regmem_store_32(ctx->cell_dmem_addr, SYSCOM_STATE_REG,
			SYSCOM_STATE_UNINIT, cfg->ssid);
	/* store syscom uninitialized command */
	IA_CSS_TRACE_3(SYSCOM, INFO, "ia_css_syscom_open store COMMAND_REG (%#x) @ dmem_addr %#x ssid %d\n",
		       SYSCOM_COMMAND_UNINIT, ctx->cell_dmem_addr, cfg->ssid);
	regmem_store_32(ctx->cell_dmem_addr, SYSCOM_COMMAND_REG,
			SYSCOM_COMMAND_UNINIT, cfg->ssid);
	/* store firmware configuration address */
	IA_CSS_TRACE_3(SYSCOM, INFO, "ia_css_syscom_open store CONFIG_REG (%#x) @ dmem_addr %#x ssid %d\n",
		       ctx->config_vied_addr, ctx->cell_dmem_addr, cfg->ssid);
	regmem_store_32(ctx->cell_dmem_addr, SYSCOM_CONFIG_REG,
			ctx->config_vied_addr, cfg->ssid);
#endif

	/* Indicate if ctx is created for secure stream purpose */
	ctx->secure = cfg->secure;

	IA_CSS_TRACE_1(SYSCOM, INFO, "ia_css_syscom_open (secure %d) completed\n", cfg->secure);
	return ctx;
}


int
ia_css_syscom_close(
	struct ia_css_syscom_context *ctx
) {
	int state;

	state = regmem_load_32(ctx->cell_dmem_addr, SYSCOM_STATE_REG,
				ctx->env.ssid);
	if (state != SYSCOM_STATE_READY) {
		/* SPC is not ready to handle close request yet */
		return FW_ERROR_BUSY;
	}

	/* set close request flag */
	regmem_store_32(ctx->cell_dmem_addr, SYSCOM_COMMAND_REG,
			SYSCOM_COMMAND_INACTIVE, ctx->env.ssid);

	return 0;
}

static void
ia_css_syscom_free(struct ia_css_syscom_context *ctx)
{
	shared_memory_unmap(ctx->env.ssid, ctx->env.mmid, ctx->ibuf_vied_addr);
	shared_memory_free(ctx->env.mmid, ctx->ibuf_host_addr);
	shared_memory_unmap(ctx->env.ssid, ctx->env.mmid, ctx->obuf_vied_addr);
	shared_memory_free(ctx->env.mmid, ctx->obuf_host_addr);
	shared_memory_unmap(ctx->env.ssid, ctx->env.mmid,
			    ctx->config_vied_addr);
	shared_memory_free(ctx->env.mmid, ctx->config_host_addr);
	ia_css_cpu_mem_free(ctx);
}

int
ia_css_syscom_release(
	struct ia_css_syscom_context *ctx,
	unsigned int force
) {
	/* check if release is forced, an verify cell state if it is not */
	if (!force) {
		if (!ia_css_cell_is_ready(ctx->env.ssid, SPC0))
			return FW_ERROR_BUSY;
	}

	/* Reset the regmem idx */
	ctx->regmem_idx = 0;

	if (ctx->free_buf)
		ia_css_syscom_free(ctx);

	return 0;
}

int ia_css_syscom_send_port_open(
	struct ia_css_syscom_context *ctx,
	unsigned int port
)
{
	int state;

	/* check parameters */
	verifret(ctx != NULL, FW_ERROR_BAD_ADDRESS);
	verifret(port < ctx->num_input_queues, FW_ERROR_INVALID_PARAMETER);

	/* check if SP syscom is ready to open the queue */
	state = regmem_load_32(ctx->cell_dmem_addr, SYSCOM_STATE_REG,
			       ctx->env.ssid);
	if (state != SYSCOM_STATE_READY) {
		/* SPC is not ready to handle messages yet */
		return FW_ERROR_BUSY;
	}

	/* initialize the port */
	send_port_open(ctx->send_port + port,
		       ctx->input_queue + port, &(ctx->env));

	return 0;
}

int ia_css_syscom_send_port_close(
	struct ia_css_syscom_context *ctx,
	unsigned int port
)
{
	/* check parameters */
	verifret(ctx != NULL, FW_ERROR_BAD_ADDRESS);
	verifret(port < ctx->num_input_queues, FW_ERROR_INVALID_PARAMETER);

	return 0;
}

int ia_css_syscom_send_port_available(
	struct ia_css_syscom_context *ctx,
	unsigned int port
)
{
	/* check params */
	verifret(ctx != NULL, FW_ERROR_BAD_ADDRESS);
	verifret(port < ctx->num_input_queues, FW_ERROR_INVALID_PARAMETER);

	return send_port_available(ctx->send_port + port);
}

int ia_css_syscom_send_port_transfer(
	struct ia_css_syscom_context *ctx,
	unsigned int port,
	const void *token
)
{
	/* check params */
	verifret(ctx != NULL, FW_ERROR_BAD_ADDRESS);
	verifret(port < ctx->num_input_queues, FW_ERROR_INVALID_PARAMETER);

	return send_port_transfer(ctx->send_port + port, token);
}

int ia_css_syscom_recv_port_open(
	struct ia_css_syscom_context *ctx,
	unsigned int port
)
{
	int state;

	/* check parameters */
	verifret(ctx != NULL, FW_ERROR_BAD_ADDRESS);
	verifret(port < ctx->num_output_queues, FW_ERROR_INVALID_PARAMETER);

	/* check if SP syscom is ready to open the queue */
	state = regmem_load_32(ctx->cell_dmem_addr,
				SYSCOM_STATE_REG, ctx->env.ssid);
	if (state != SYSCOM_STATE_READY) {
		/* SPC is not ready to handle messages yet */
		return FW_ERROR_BUSY;
	}

	/* initialize the port */
	recv_port_open(ctx->recv_port + port,
		       ctx->output_queue + port, &(ctx->env));

	return 0;
}

int ia_css_syscom_recv_port_close(
	struct ia_css_syscom_context *ctx,
	unsigned int port
)
{
	/* check parameters */
	verifret(ctx != NULL, FW_ERROR_BAD_ADDRESS);
	verifret(port < ctx->num_output_queues, FW_ERROR_INVALID_PARAMETER);

	return 0;
}

/*
 * Get the number of responses in the response queue
 */
int
ia_css_syscom_recv_port_available(
	struct ia_css_syscom_context *ctx,
	unsigned int port
)
{
	/* check params */
	verifret(ctx != NULL, FW_ERROR_BAD_ADDRESS);
	verifret(port < ctx->num_output_queues, FW_ERROR_INVALID_PARAMETER);

	return recv_port_available(ctx->recv_port + port);
}


/*
 * Dequeue the head of the response queue
 * returns an error when the response queue is empty
 */
int
ia_css_syscom_recv_port_transfer(
	struct ia_css_syscom_context *ctx,
	unsigned int port,
	void *token
)
{
	/* check params */
	verifret(ctx != NULL, FW_ERROR_BAD_ADDRESS);
	verifret(port < ctx->num_output_queues, FW_ERROR_INVALID_PARAMETER);

	return recv_port_transfer(ctx->recv_port + port, token);
}

#if HAS_DUAL_CMD_CTX_SUPPORT
/*
 * store subsystem context information in DMEM
 */
int
ia_css_syscom_store_dmem(
	struct ia_css_syscom_context *ctx,
	unsigned int ssid,
	unsigned int vtl0_addr_mask
)
{
	unsigned int read_back;

	NOT_USED(vtl0_addr_mask);
	NOT_USED(read_back);

	if (ctx->secure) {
		/* store VTL0 address mask in 'secure' context */
		IA_CSS_TRACE_3(SYSCOM, INFO, "ia_css_syscom_store_dmem VTL0_ADDR_MASK (%#x) @ dmem_addr %#x ssid %d\n",
			      vtl0_addr_mask, ctx->cell_dmem_addr, ssid);
		regmem_store_32(ctx->cell_dmem_addr, SYSCOM_VTL0_ADDR_MASK, vtl0_addr_mask, ssid);
	}
	/* store firmware configuration address */
	IA_CSS_TRACE_3(SYSCOM, INFO, "ia_css_syscom_store_dmem CONFIG_REG (%#x) @ dmem_addr %#x ssid %d\n",
		       ctx->config_vied_addr, ctx->cell_dmem_addr, ssid);
	regmem_store_32(ctx->cell_dmem_addr, SYSCOM_CONFIG_REG,
			ctx->config_vied_addr, ssid);
	/* store syscom uninitialized state */
	IA_CSS_TRACE_3(SYSCOM, INFO, "ia_css_syscom_store_dmem STATE_REG (%#x) @ dmem_addr %#x ssid %d\n",
		       SYSCOM_STATE_UNINIT, ctx->cell_dmem_addr, ssid);
	regmem_store_32(ctx->cell_dmem_addr, SYSCOM_STATE_REG,
			SYSCOM_STATE_UNINIT, ssid);
	/* store syscom uninitialized command */
	IA_CSS_TRACE_3(SYSCOM, INFO, "ia_css_syscom_store_dmem COMMAND_REG (%#x) @ dmem_addr %#x ssid %d\n",
		       SYSCOM_COMMAND_UNINIT, ctx->cell_dmem_addr, ssid);
	regmem_store_32(ctx->cell_dmem_addr, SYSCOM_COMMAND_REG,
			SYSCOM_COMMAND_UNINIT, ssid);

	return 0;
}

/*
 * store truslet configuration status setting
 */
void
ia_css_syscom_set_trustlet_status(
	unsigned int dmem_addr,
	unsigned int ssid,
	bool trustlet_exist
)
{
	unsigned int value;

	value = trustlet_exist ? TRUSTLET_EXIST : TRUSTLET_NOT_EXIST;
	IA_CSS_TRACE_3(SYSCOM, INFO,
		       "ia_css_syscom_set_trustlet_status TRUSTLET_STATUS (%#x) @ dmem_addr %#x ssid %d\n",
		       value, dmem_addr, ssid);
	regmem_store_32(dmem_addr, TRUSTLET_STATUS, value, ssid);
}

/*
 * check if SPC access blocker programming is completed
 */
bool
ia_css_syscom_is_ab_spc_ready(
	struct ia_css_syscom_context *ctx
)
{
	unsigned int value;

	/* We only expect the call from non-secure context only */
	if (ctx->secure) {
		IA_CSS_TRACE_0(SYSCOM, ERROR, "ia_css_syscom_is_spc_ab_ready - Please call from non-secure context\n");
		return false;
	}

	value = regmem_load_32(ctx->cell_dmem_addr, AB_SPC_STATUS, ctx->env.ssid);
	IA_CSS_TRACE_3(SYSCOM, INFO,
		       "ia_css_syscom_is_spc_ab_ready AB_SPC_STATUS @ dmem_addr %#x ssid %d - value %#x\n",
		       ctx->cell_dmem_addr, ctx->env.ssid, value);

	return (value == AB_SPC_READY);
}
#endif /* HAS_DUAL_CMD_CTX_SUPPORT */
