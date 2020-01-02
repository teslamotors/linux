/*
 * Copyright (C) 2018 Tesla, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __TRIP_DMA_H__
#define __TRIP_DMA_H__

void trip_disable_wdt(struct platform_device *pdev);
void trip_init_wdt_memtest(struct platform_device *pdev);

int trip_copy_to_sram_verified(struct platform_device *pdev,
					const u64 *buf, u32 len, u32 offset);

void trip_enable_parity(struct platform_device *pdev, bool enabled);

int trip_check_status(u32 hw_status, u32 *xfer_status);

struct trip_req *trip_net_submit(struct tripdev *td, int n,
				 u32 sram_addr, dma_addr_t data_base,
				 dma_addr_t weight_base,
				 dma_addr_t output_base);

struct trip_req *trip_net_submit(struct tripdev *td, int n,
				 u32 sram_addr, dma_addr_t data_base,
				 dma_addr_t weight_base,
				 dma_addr_t output_base);

void trip_submit_req(struct tripdev *td, struct trip_req *req, int n);

void trip_submit_lock(struct tripdev *td);
void trip_submit_unlock(struct tripdev *td);

int trip_set_serialized_mode(struct tripdev *td, bool serialized);

int trip_sg_handle_dma(struct platform_device *pdev,
			struct trip_ioc_xfer *xfer,
			struct sg_table *sgt,
			int nr_dma,
			int dma_to_sram);

int trip_dma_init(struct platform_device *pdev);
int trip_dma_suspend(struct platform_device *pdev);
int trip_dma_resume(struct platform_device *pdev);

int trip_wdt_setup(struct platform_device *pdev, uint32_t network);

int trip_set_clk_rate(struct platform_device *pdev, unsigned long rate);
unsigned long trip_get_clk_rate(struct platform_device *pdev);


#endif /* __TRIP_DMA_H__ */
