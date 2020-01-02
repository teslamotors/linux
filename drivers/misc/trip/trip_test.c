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

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/scatterlist.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>

#include "trip_ioctl.h"
#include "trip_fw_fmt.h"
#include "trip_fw.h"
#include "trip_reg.h"
#include "trip.h"
#include "trip_dma.h"
#include "trip_test.h"

#include "trip_memtest.h"

int trip_dump_sram(struct platform_device *pdev, u32 offset, u32 len)
{
	struct tripdev *td = platform_get_drvdata(pdev);

	if (((offset & 7) != 0) || ((len & 7) != 0))
		return -EINVAL;

	while (len) {
		uint64_t val = readq(td->td_sram + offset);

		dev_dbg(&pdev->dev, "%08x: %016llx\n", offset, val);
		offset += sizeof(u64);
		len -= sizeof(u64);
	}

	return 0;
}

/* Prevent RCU related slowdown by periodically forcing a grace period */
#define RELAX_QUANTA  (((1 << 12) - 1) & ~7)

int trip_sram_test(struct platform_device *pdev, u64 start, u64 len)
{

	struct tripdev *td = platform_get_drvdata(pdev);
	u64 cur;
	u64 val;

	/* address in address */
	dev_dbg(&pdev->dev, "starting sram test 1, len 0x%llx\n", len);
	for (cur = start; cur < start + len; cur += sizeof(val)) {
		val = cur;
		writeq_relaxed(val, td->td_sram + cur);
		if ((cur & RELAX_QUANTA) == RELAX_QUANTA)
			cond_resched();
	}
	for (cur = start; cur < start + len; cur += sizeof(val)) {
		val = readq_relaxed(td->td_sram + cur);
		if (val != cur) {
			dev_err(&pdev->dev, "sram miscompare @%llx: read %llx, expected %llx\n",
				cur, val, cur);
			return -EIO;
		}
		if ((cur & RELAX_QUANTA) == RELAX_QUANTA)
			cond_resched();
	}

	/* address not in address */
	dev_dbg(&pdev->dev, "starting sram test 2, len 0x%llx\n", len);
	for (cur = start; cur < start + len; cur += sizeof(val)) {
		val = ~cur;
		writeq_relaxed(val, td->td_sram + cur);
		if ((cur & RELAX_QUANTA) == RELAX_QUANTA)
			cond_resched();

	}
	for (cur = start; cur < start + len; cur += sizeof(val)) {
		val = readq_relaxed(td->td_sram + cur);
		if (val != ~cur) {
			dev_err(&pdev->dev, "sram miscompare @%llx: read %llx, expected %llx\n",
				cur, val, ~cur);
			return -EIO;
		}
		if ((cur & RELAX_QUANTA) == RELAX_QUANTA)
			cond_resched();
	}

	/* zeros */
	dev_dbg(&pdev->dev, "starting sram test 3, len 0x%llx\n", len);
	for (cur = start; cur < start + len; cur += sizeof(val)) {
		val = 0;
		writeq_relaxed(val, td->td_sram + cur);
		if ((cur & RELAX_QUANTA) == RELAX_QUANTA)
			cond_resched();
	}
	for (cur = start; cur < start + len; cur += sizeof(val)) {
		val = readq_relaxed(td->td_sram + cur);
		if (val != 0) {
			dev_err(&pdev->dev, "sram miscompare @%llx: read %llx, expected %llx\n",
				cur, val, 0ULL);
			return -EIO;
		}
		if ((cur & RELAX_QUANTA) == RELAX_QUANTA)
			cond_resched();
	}
	dev_dbg(&pdev->dev, "finished sram tests\n");
	return 0;
}

int trip_run_memtest(struct platform_device *pdev, int pass)
{
	struct tripdev *td = platform_get_drvdata(pdev);
	struct trip_req *req;
	u32 offset;
	u32 status;
	int ret = 0;

	/* run the test once with parity errors disabled */
	req = trip_net_submit(td, 0, trip_memtest_ofs, 0, 0, 0);
	if (IS_ERR(req))
		return PTR_ERR(req);

	wait_for_completion(&req->completion);

	ret = trip_check_status(req->nw_status, &status);
	if (ret != 0) {
		dev_err(&pdev->dev, "memtest pass %d failed %d, 0x%08x\n",
					pass, status, req->nw_status);
		kfree(req);
		td->post_status = (pass == 0) ?
			TRIP_POST_STATUS_RUN_NOPARITY_FAILED :
			TRIP_POST_STATUS_RUN_PARITY_FAILED;
		return ret;
	}
	kfree(req);

	/* result of test should be all zeros */
	for (offset = 0; offset < trip_memtest_out_len; offset +=
							sizeof(u64)) {
		u64 val = readq(td->td_sram + trip_memtest_out_ofs + offset);

		if (val != 0) {
			dev_err(&pdev->dev, "memtest pass %d failed @ res %d, 0x%llx\n",
						pass, offset, val);
			ret = -EIO;
			td->post_status = (pass == 0) ?
				TRIP_POST_STATUS_NOPARITY_SRAM_MISCOMPARE :
				TRIP_POST_STATUS_PARITY_SRAM_MISCOMPARE;
		}
	}

	return ret;
}

int trip_sram_test_fast(struct platform_device *pdev)
{
	struct tripdev *td = platform_get_drvdata(pdev);
	int ret;

	td->post_status = TRIP_POST_STATUS_INIT_SRAM;

	/* special watchdog mode for memtest */
	trip_init_wdt_memtest(pdev);

	/* mask trip parity errors before SRAM is zero'd */
	trip_enable_parity(pdev, false);

	/* manually init the portion we'll use for program/data */
	ret = trip_sram_test(pdev, trip_memtest_manual_ofs,
				trip_memtest_manual_len);
	if (ret != 0) {
		td->post_status = TRIP_POST_STATUS_INIT_SRAM_FAILED;
		return ret;
	}

	/* copy over the program and data */
	ret = trip_copy_to_sram_verified(pdev,
					 (const uint64_t *) trip_memtest_data,
					 trip_memtest_data_len,
					 trip_memtest_data_ofs);
	if (ret != 0) {
		td->post_status = TRIP_POST_STATUS_INIT_SRAM_FAILED;
		return ret;
	}
	ret = trip_copy_to_sram_verified(pdev,
					 (const uint64_t *) trip_memtest,
					 trip_memtest_len,
					 trip_memtest_ofs);
	if (ret != 0) {
		td->post_status = TRIP_POST_STATUS_INIT_SRAM_FAILED;
		return ret;
	}

	/* run the test once with parity errors disabled */
	td->post_status = TRIP_POST_STATUS_RUN_NOPARITY;
	ret = trip_run_memtest(pdev, 0);
	if (ret != 0)
		return ret;

	/* re-enable trip parity errors, all SRAM should have valid parity */
	td->post_status = TRIP_POST_STATUS_RUN_PARITY;
	trip_enable_parity(pdev, true);

	/* run the test once again with parity errors enabled */
	ret = trip_run_memtest(pdev, 1);
	if (ret != 0)
		return ret;

	/* test+zero program memory again with parity enabled */
	td->post_status = TRIP_POST_STATUS_INIT_SRAM_FINAL;
	ret = trip_sram_test(pdev, trip_memtest_manual_ofs,
				trip_memtest_manual_len);
	td->post_status = (ret == 0) ? TRIP_POST_STATUS_WAITING_ROOT
				     : TRIP_POST_STATUS_INIT_SRAM_FINAL_FAILED;

	return ret;
}
