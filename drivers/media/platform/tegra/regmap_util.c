/*
 * regmap_util.c - utilities for writing regmap tables
 *
 * Copyright (c) 2013-2015, NVIDIA Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/regmap.h>
#include <media/camera_common.h>

int
regmap_util_write_table_8(struct regmap *regmap,
			  const struct reg_8 table[],
			  const struct reg_8 override_list[],
			  int num_override_regs, u16 wait_ms_addr, u16 end_addr)
{
	int err;
	const struct reg_8 *next;
	int i;
	u8 val;

	int range_start = -1;
	int range_count = 0;
	/* bug 200048392 -
	   the vi i2c cannot take a FIFO buffer bigger than 16 bytes */
	u8 range_vals[16];
	int max_range_vals = ARRAY_SIZE(range_vals);

	for (next = table;; next++) {
		/* If we have a range open and */
		/* either the address doesn't match */
		/* or the temporary storage is full, flush */
		if  ((next->addr != range_start + range_count) ||
		     (next->addr == end_addr) ||
		     (next->addr == wait_ms_addr) ||
		     (range_count == max_range_vals)) {

			if (range_count == 1) {
				err =
				    regmap_write(regmap, range_start,
						 range_vals[0]);
			} else if (range_count > 1) {
				err =
				    regmap_bulk_write(regmap, range_start,
						      &range_vals[0],
						      range_count);
			}

			if (err) {
				pr_err("%s:regmap_util_write_table:%d",
				       __func__, err);
				return err;
			}

			range_start = -1;
			range_count = 0;

			/* Handle special address values */
			if (next->addr == end_addr)
				break;

			if (next->addr == wait_ms_addr) {
				msleep_range(next->val);
				continue;
			}
		}

		val = next->val;

		/* When an override list is passed in, replace the reg */
		/* value to write if the reg is in the list            */
		if (override_list) {
			for (i = 0; i < num_override_regs; i++) {
				if (next->addr == override_list[i].addr) {
					val = override_list[i].val;
					break;
				}
			}
		}

		if (range_start == -1)
			range_start = next->addr;

		range_vals[range_count++] = val;
	}
	return 0;
}

int
regmap_util_write_table_16_as_8(struct regmap *regmap,
				const struct reg_16 table[],
				const struct reg_16 override_list[],
				int num_override_regs,
				u16 wait_ms_addr, u16 end_addr)
{
	int err;
	const struct reg_16 *next;
	int i;
	u16 val;

	int range_start = -1;
	int range_count = 0;
	u8 range_vals[256];
	int max_range_vals = ARRAY_SIZE(range_vals);

	for (next = table;; next++) {
		/* If we have a range open and */
		/* either the address doesn't match */
		/* or the temporary storage is full, flush*/
		if  ((next->addr != range_start + range_count) ||
		     (next->addr == end_addr) ||
		     (next->addr == wait_ms_addr) ||
		     (range_count == max_range_vals)) {

			if (range_count > 1) {
				err =
				    regmap_bulk_write(regmap, range_start,
						      &range_vals[0],
						      range_count);
			}

			if (err) {
				pr_err("%s:regmap_util_write_table:%d",
				       __func__, err);
				return err;
			}

			range_start = -1;
			range_count = 0;

			/* Handle special address values */
			if (next->addr == end_addr)
				break;

			if (next->addr == wait_ms_addr) {
				msleep_range(next->val);
				continue;
			}
		}

		val = next->val;

		/* When an override list is passed in, replace the reg */
		/* value to write if the reg is in the list            */
		if (override_list) {
			for (i = 0; i < num_override_regs; i++) {
				if (next->addr == override_list[i].addr) {
					val = override_list[i].val;
					break;
				}
			}
		}

		if (range_start == -1)
			range_start = next->addr;

		range_vals[range_count++] = (u8) (val >> 8);
		range_vals[range_count++] = (u8) (val & 0xFF);
	}
	return 0;
}
