/*
 * Copyright (c) 2014-2015, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/export.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <soc/tegra/tegra_bpmp.h>
#include <soc/tegra/bpmp_abi.h>

#include "clk-t18x.h"
#include "clk-mrq.h"

/**
 * Mutex to prevent concurrent invocations
 * to register a clock.
 */
static DEFINE_MUTEX(clk_reg_lock);

struct bpmp_clk_req {
	u32	cmd;
	u8	args[0];
};

#define BPMP_CLK_CMD(cmd, id) ((id) | ((cmd) << 24))
#define MAX_PARENTS MRQ_CLK_MAX_PARENTS

struct possible_parents {
	u8	num_of_parents;
	s32	clk_ids[MAX_PARENTS];
};

static struct clk **clks;
static const char **clk_names;
static int max_clk_id;
static struct clk_onecell_data clk_data;

static int bpmp_send_clk_message_atomic(struct bpmp_clk_req *req, int size,
				 u8 *reply, int reply_size)
{
	unsigned long flags;
	int err;

	local_irq_save(flags);
	err = tegra_bpmp_send_receive_atomic(MRQ_CLK, req, size, reply,
			reply_size);
	local_irq_restore(flags);

	return err;
}

static int bpmp_send_clk_message(struct bpmp_clk_req *req, int size,
				 u8 *reply, int reply_size)
{
	int err;

	err = tegra_bpmp_send_receive(MRQ_CLK, req, size, reply, reply_size);
	if (err != -EAGAIN)
		return err;

	/*
	 * in case the mail systems worker threads haven't been started yet,
	 * use the atomic send/receive interface. This happens because the
	 * clocks are initialized before the IPC mechanism.
         */
	return bpmp_send_clk_message_atomic(req, size, reply, reply_size);
}

static int clk_bpmp_enable(struct clk_hw *hw)
{
	struct tegra_clk_bpmp *bpmp_clk = to_clk_bpmp(hw);
	struct bpmp_clk_req req;

	req.cmd = BPMP_CLK_CMD(MRQ_CLK_ENABLE, bpmp_clk->clk_num);

	return bpmp_send_clk_message(&req, sizeof(req), NULL, 0);
}

static void clk_bpmp_disable(struct clk_hw *hw)
{
	struct tegra_clk_bpmp *bpmp_clk = to_clk_bpmp(hw);
	struct bpmp_clk_req req;

	req.cmd = BPMP_CLK_CMD(MRQ_CLK_DISABLE, bpmp_clk->clk_num);

	bpmp_send_clk_message(&req, sizeof(req), NULL, 0);
}

static int clk_bpmp_is_enabled(struct clk_hw *hw)
{
	struct tegra_clk_bpmp *bpmp_clk = to_clk_bpmp(hw);
	struct bpmp_clk_req req;
	int err;
	u8 reply[4];

	req.cmd = BPMP_CLK_CMD(MRQ_CLK_IS_ENABLED, bpmp_clk->clk_num);

	err = bpmp_send_clk_message_atomic(&req, sizeof(req),
			reply, sizeof(reply));
	if (err < 0)
		return err;

	return ((s32 *)reply)[0];
}

static u8 clk_bpmp_get_parent(struct clk_hw *hw)
{
	struct tegra_clk_bpmp *bpmp_clk = to_clk_bpmp(hw);
	int parent_id, i;

	parent_id = bpmp_clk->parent;

	if (parent_id < 0)
		goto err_out;

	for (i = 0; i < bpmp_clk->num_parents; i++) {
		if (bpmp_clk->parent_ids[i] == parent_id)
			return i;
	}

err_out:
	pr_err("clk_bpmp_get_parent for %s parent_id: %d, num_parents: %d\n",
		__clk_get_name(hw->clk), parent_id, bpmp_clk->num_parents);
	WARN_ON(1);

	return 0;
}

static int clk_bpmp_set_parent(struct clk_hw *hw, u8 index)
{
	struct tegra_clk_bpmp *bpmp_clk = to_clk_bpmp(hw);
	u8 req_d[12], reply[4];
	struct bpmp_clk_req *req  = (struct bpmp_clk_req *)&req_d[0];
	int err;

	if (index > bpmp_clk->num_parents - 1)
		return -EINVAL;

	req->cmd = BPMP_CLK_CMD(MRQ_CLK_SET_PARENT, bpmp_clk->clk_num);
	*((u32 *)&req->args[0]) = bpmp_clk->parent_ids[index];

	err = bpmp_send_clk_message(req, sizeof(req_d), reply, sizeof(reply));
	if (!err)
		bpmp_clk->parent = bpmp_clk->parent_ids[index];

	return err;
}

static int clk_bpmp_set_rate(struct clk_hw *hw, unsigned long rate,
			      unsigned long parent_rate)
{
	struct tegra_clk_bpmp *bpmp_clk = to_clk_bpmp(hw);
	u8 req_d[16], reply[8];
	struct bpmp_clk_req *req = (struct bpmp_clk_req *)&req_d[0];

	req->cmd = BPMP_CLK_CMD(MRQ_CLK_SET_RATE, bpmp_clk->clk_num);
	if (rate > S64_MAX)
		rate = S64_MAX;

	*((s64 *)&req->args[4]) = rate;

	return bpmp_send_clk_message(req, sizeof(req_d), reply, sizeof(reply));
}

static long clk_bpmp_round_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long *parent_rate)
{
	struct tegra_clk_bpmp *bpmp_clk = to_clk_bpmp(hw);
	int err;
	u8 req_d[16], reply[8];
	struct bpmp_clk_req *req = (struct bpmp_clk_req *)&req_d[0];

	req->cmd = BPMP_CLK_CMD(MRQ_CLK_ROUND_RATE, bpmp_clk->clk_num);
	if (rate > S64_MAX)
		rate = S64_MAX;

	*((s64 *)&req->args[4]) = rate;
	err = bpmp_send_clk_message(req, sizeof(req_d), reply, sizeof(reply));

	if (err < 0)
		return err;

	return ((s64 *)reply)[0];
}

static unsigned long clk_bpmp_get_rate_clk_num(int clk_num)
{
	u8 reply[8];
	struct bpmp_clk_req req;
	int err;

	req.cmd = BPMP_CLK_CMD(MRQ_CLK_GET_RATE, clk_num);
	err = bpmp_send_clk_message(&req, sizeof(req), reply, sizeof(reply));
	if (err < 0)
		return err;

	return ((s64 *)reply)[0];
}

static unsigned long clk_bpmp_get_rate(struct clk_hw *hw,
				       unsigned long parent_rate)
{
	struct tegra_clk_bpmp *bpmp_clk = to_clk_bpmp(hw);

	return clk_bpmp_get_rate_clk_num(bpmp_clk->clk_num);
}

static int clk_bpmp_get_max_clk_id(u32 *max_id)
{
	struct bpmp_clk_req req;
	u8 reply[4];
	int err;

	req.cmd = BPMP_CLK_CMD(CMD_CLK_GET_MAX_CLK_ID, 0);
	err = bpmp_send_clk_message(&req, sizeof(req), reply, sizeof(reply));
	if (err < 0)
		return err;

	*max_id = ((u32 *)reply)[0];
	return 0;
}

static int clk_bpmp_get_all_info(int clk_num,
				 u32 *flags,
				 u32 *parent,
				 struct possible_parents *parents,
				 char *name)
{
	struct bpmp_clk_req req;
	struct cmd_clk_get_all_info_response resp;
	int i, err;

	req.cmd = BPMP_CLK_CMD(CMD_CLK_GET_ALL_INFO, clk_num);
	err = bpmp_send_clk_message((void *)&req, sizeof(req), (void *)&resp,
				    sizeof(resp));
	if (err < 0)
		return err;
	*flags = resp.flags;
	*parent = resp.parent;
	parents->num_of_parents = resp.num_parents;
	for (i = 0; i < resp.num_parents; ++i)
		parents->clk_ids[i] = resp.parents[i];
	strncpy(name, resp.name, MRQ_CLK_NAME_MAXLEN);
	name[MRQ_CLK_NAME_MAXLEN-1] = 0;

	return 0;
}

const struct clk_ops tegra_clk_bpmp_gate_ops = {
	.is_enabled = clk_bpmp_is_enabled,
	.prepare = clk_bpmp_enable,
	.unprepare = clk_bpmp_disable,
};

const struct clk_ops tegra_clk_bpmp_mux_rate_ops = {
	.is_enabled = clk_bpmp_is_enabled,
	.prepare = clk_bpmp_enable,
	.unprepare = clk_bpmp_disable,
	.get_parent = clk_bpmp_get_parent,
	.set_parent = clk_bpmp_set_parent,
	.set_rate = clk_bpmp_set_rate,
	.round_rate = clk_bpmp_round_rate,
	.recalc_rate = clk_bpmp_get_rate,
};

const struct clk_ops tegra_clk_bpmp_rate_ops = {
	.is_enabled = clk_bpmp_is_enabled,
	.prepare = clk_bpmp_enable,
	.unprepare = clk_bpmp_disable,
	.set_rate = clk_bpmp_set_rate,
	.round_rate = clk_bpmp_round_rate,
	.recalc_rate = clk_bpmp_get_rate,
};

const struct clk_ops  tegra_clk_bpmp_mux_ops = {
	.get_parent = clk_bpmp_get_parent,
	.set_parent = clk_bpmp_set_parent,
	.is_enabled = clk_bpmp_is_enabled,
	.prepare = clk_bpmp_enable,
	.unprepare = clk_bpmp_disable,
};

struct clk *tegra_clk_register_bpmp(const char *name, int parent,
		const char **parent_names, int *parent_ids, int num_parents,
		unsigned long flags, int clk_num, int bpmp_flags)
{
	struct tegra_clk_bpmp *bpmp_clk;
	struct clk *clk;
	struct clk_init_data init;

	if (num_parents > 1)
		bpmp_clk = kzalloc(sizeof(*bpmp_clk) + num_parents
				   * sizeof(int), GFP_KERNEL);
	else
		bpmp_clk = kzalloc(sizeof(*bpmp_clk), GFP_KERNEL);

	if (!bpmp_clk) {
		pr_err("%s: unable to allocate clock %s\n", __func__, name);
		return ERR_PTR(-ENOMEM);
	}

	init.name = name;

	if (bpmp_flags & BPMP_CLK_IS_ROOT)
		flags |= CLK_IS_ROOT;
	init.flags = flags;

	init.parent_names = parent_names;
	init.num_parents = num_parents;
	if ((bpmp_flags & (BPMP_CLK_HAS_MUX | BPMP_CLK_HAS_SET_RATE))
		== (BPMP_CLK_HAS_MUX | BPMP_CLK_HAS_SET_RATE))
		init.ops = &tegra_clk_bpmp_mux_rate_ops;
	else if (bpmp_flags & BPMP_CLK_HAS_SET_RATE)
		init.ops = &tegra_clk_bpmp_rate_ops;
	else if (bpmp_flags & BPMP_CLK_HAS_MUX)
		init.ops = &tegra_clk_bpmp_mux_ops;
	else
		init.ops = &tegra_clk_bpmp_gate_ops;

	/* Data in .init is copied by clk_register(), so stack variable OK */
	bpmp_clk->clk_num = clk_num;
	bpmp_clk->hw.init = &init;
	bpmp_clk->num_parents = num_parents;
	bpmp_clk->parent = parent;

	if (num_parents > 1)
		memcpy(&bpmp_clk->parent_ids[0], parent_ids,
		       num_parents * sizeof(int));

	clk = clk_register(NULL, &bpmp_clk->hw);
	if (IS_ERR(clk)) {
		pr_err("registration failed for clock %s (%d)\n", name,
		       clk_num);
		kfree(bpmp_clk);
	}

	return clk;
}

static int clk_bpmp_init(int clk_num)
{
	struct clk *clk;
	const char *parent_names[MAX_PARENTS];
	struct possible_parents parents;
	u32 flags, parent;
	int j, num_parents, err;
	char name[MRQ_CLK_NAME_MAXLEN];

	if (clk_num > max_clk_id || clk_num < 0) {
		pr_err("clk_bpmp_init: invalid clk_num %d\n", clk_num);
		return -EINVAL;
	}

	if (!IS_ERR_OR_NULL(clks[clk_num]))
		return 0;

	err = clk_bpmp_get_all_info(clk_num, &flags, &parent,
				    &parents, name);
	if (err < 0)
		return 0;

	num_parents = parents.num_of_parents;
	if (num_parents > 1 && !(flags & BPMP_CLK_HAS_MUX)) {
		pr_err("clk-bpmp: inconsistent data from BPMP."
		       " Clock %d has more than one parent but no mux.\n",
			clk_num);
		return -EINVAL;
	}
	if (num_parents > 0 && (flags & BPMP_CLK_IS_ROOT)) {
		pr_err("clk-bpmp: inconsistent data from BPMP."
		       " Clock %d has parents but it's declared as root.\n",
			clk_num);
		return -EINVAL;
	}
	if (num_parents > MAX_PARENTS || num_parents < 0) {
		pr_err("clk-bpmp: inconsistent data from BPMP."
		       " Clock %d has too many parents.\n",
		       clk_num);
		return -EINVAL;
	}

	for (j = 0; j < num_parents; j++) {
		int p_id = parents.clk_ids[j];
		err = clk_bpmp_init(p_id);
		if (err) {
			pr_err("clk-bpmp: unable to initialize clk %d\n",
			       p_id);
			parent_names[j] = "ERR!";
			continue;
		}
		if (IS_ERR_OR_NULL(clks[p_id])) {
			pr_err("clk-bpmp: clk %d not initialized."
			       " How did this happen?\n",
			       p_id);
			WARN_ON(1);
			parent_names[j] = "ERR!";
			continue;
		}
		parent_names[j] = clk_names[p_id];
	}

	if (flags & BPMP_CLK_IS_ROOT && !(flags & BPMP_CLK_HAS_SET_RATE)) {
		int64_t rate;
		rate = clk_bpmp_get_rate_clk_num(clk_num);
		clk = clk_register_fixed_rate(NULL, name, NULL, CLK_IS_ROOT,
					      rate);
	} else if (num_parents == 1) {
		clk = tegra_clk_register_bpmp(name, parent, parent_names, NULL,
					      num_parents, 0, clk_num, flags);
	} else {
		clk = tegra_clk_register_bpmp(name, parent,
					      parent_names, &parents.clk_ids[0],
					      num_parents, 0, clk_num, flags);
	}

	err = clk_register_clkdev(clk, name, "tegra-clk-debug");
	if (err)
		pr_err("clk_register_clkdev() returned %d for clk %s\n",
		       err, name);

	clks[clk_num] = clk;
	clk_names[clk_num] = kstrdup(name, GFP_KERNEL);
	return 0;
}

static struct clk *tegra_of_clk_src_onecell_get(struct of_phandle_args *clkspec,
	void *data)
{
	struct clk_onecell_data *clk_data = data;
	unsigned int idx = clkspec->args[0];
	int err;

	if (idx >= clk_data->clk_num) {
		pr_err("%s: invalid clock index %d\n", __func__, idx);
		return ERR_PTR(-EINVAL);
	}

	BUG_ON(clk_data->clks != clks);
	mutex_lock(&clk_reg_lock);
	if (!clks[idx]) {
		err = clk_bpmp_init(idx);
		if (err < 0) {
			pr_err("clk-bpmp: failed to initialize clk %d\n", idx);
			clks[idx] = ERR_PTR(-EINVAL);
		}
	}
	mutex_unlock(&clk_reg_lock);

	return clks[idx];
}

struct clk **tegra_bpmp_clk_init(struct device_node *np)
{
	if (clk_bpmp_get_max_clk_id(&max_clk_id) || max_clk_id < 0) {
		pr_err("clk-bpmp: unable to retrieve clk data\n");
		return ERR_PTR(-ENODEV);
	}

	clks = kzalloc((max_clk_id + 1) * sizeof(struct clk *), GFP_KERNEL);
	if (!clks) {
		WARN_ON(1);
		return ERR_PTR(-ENOMEM);
	}

	clk_names = kzalloc((max_clk_id + 1) * sizeof(char *), GFP_KERNEL);
	if (!clk_names) {
		WARN_ON(1);
		kfree(clks);
		return ERR_PTR(-ENOMEM);
	}

	clk_data.clks = clks;
	clk_data.clk_num = max_clk_id + 1;
	of_clk_add_provider(np, tegra_of_clk_src_onecell_get, &clk_data);

	return clks;
}
