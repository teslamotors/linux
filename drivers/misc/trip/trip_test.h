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

#ifndef __TRIP_TEST_H__
#define __TRIP_TEST_H__

int trip_dump_sram(struct platform_device *pdev, u32 offset, u32 len);

int trip_sram_test(struct platform_device *pdev, u64 start, u64 len);
int trip_sram_test_fast(struct platform_device *pdev);

#endif /* __TRIP_TEST_H__ */
