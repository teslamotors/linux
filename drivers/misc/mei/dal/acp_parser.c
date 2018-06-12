/******************************************************************************
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2016-2017 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * The full GNU General Public License is included in this distribution
 * in the file called LICENSE.GPL.
 *
 * Contact Information:
 *	Intel Corporation.
 *	linux-mei@linux.intel.com
 *	http://www.intel.com
 *
 * BSD LICENSE
 *
 * Copyright(c) 2016-2017 Intel Corporation. All rights reserved.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/

#include <linux/kernel.h>
#include <linux/errno.h>

#include "acp_format.h"
#include "acp_parser.h"

#define PR_ALIGN 4

/* CSS Header + CSS Crypto Block
 * Prefixes each signed ACP package
 */
#define AC_CSS_HEADER_LENGTH    (128 + 520)

/**
 * struct ac_pr_state - admin command pack reader state
 *
 * @cur   : current read position
 * @head  : acp file head
 * @total : size of acp file
 */
struct ac_pr_state {
	const char *cur;
	const char *head;
	unsigned int total;
};

/**
 * ac_pr_init - init pack reader
 *
 * @pr: pack reader
 * @data: acp file content (without CSS header)
 * @n: acp file size (without CSS header)
 *
 * Return: 0 on success
 *         -EINVAL on invalid parameters
 */
static int ac_pr_init(struct ac_pr_state *pr, const char *data,
		      unsigned int n)
{
	/* check integer overflow */
	if ((size_t)data > SIZE_MAX - n)
		return -EINVAL;

	pr->cur = data;
	pr->head = data;
	pr->total = n;
	return 0;
}

/**
 * ac_pr_8b_align_move - update pack reader cur pointer after reading n_move
 *                       bytes. Leave cur aligned to 8 bytes.
 *                       (e.g. when n_move is 3, increase cur by 8)
 *
 * @pr: pack reader
 * @n_move: number of bytes to move cur pointer ahead
 *          will be rounded up to keep cur 8 bytes aligned
 *
 * Return: 0 on success
 *         -EINVAL on invalid parameters
 */
static int ac_pr_8b_align_move(struct ac_pr_state *pr, size_t n_move)
{
	unsigned long offset;
	const char *new_cur = pr->cur + n_move;
	size_t len_from_head = new_cur - pr->head;

	if ((size_t)pr->cur > SIZE_MAX - n_move || new_cur < pr->head)
		return -EINVAL;

	offset = ((8 - (len_from_head & 7)) & 7);
	if ((size_t)new_cur > SIZE_MAX - offset)
		return -EINVAL;

	new_cur = new_cur + offset;
	if (new_cur > pr->head + pr->total)
		return -EINVAL;

	pr->cur = new_cur;
	return 0;
}

/**
 * ac_pr_align_move - update pack reader cur pointer after reading n_move bytes
 *                    Leave cur aligned to 4 bytes.
 *                    (e.g. when n_move is 1, increase cur by 4)
 *
 * @pr: pack reader
 * @n_move: number of bytes to move cur pointer ahead
 *          will be rounded up to keep cur 4 bytes aligned
 *
 * Return: 0 on success
 *         -EINVAL on invalid parameters
 */
static int ac_pr_align_move(struct ac_pr_state *pr, size_t n_move)
{
	const char *new_cur = pr->cur + n_move;
	size_t len_from_head = new_cur - pr->head;
	size_t offset;

	if ((size_t)pr->cur > SIZE_MAX - n_move || new_cur < pr->head)
		return -EINVAL;

	offset = ((4 - (len_from_head & 3)) & 3);
	if ((size_t)new_cur > SIZE_MAX - offset)
		return -EINVAL;

	new_cur = new_cur + offset;
	if (new_cur > pr->head + pr->total)
		return -EINVAL;

	pr->cur = new_cur;
	return 0;
}

/**
 * ac_pr_move - update pack reader cur pointer after reading n_move bytes
 *
 * @pr: pack reader
 * @n_move: number of bytes to move cur pointer ahead
 *
 * Return: 0 on success
 *         -EINVAL on invalid parameters
 */
static int ac_pr_move(struct ac_pr_state *pr, size_t n_move)
{
	const char *new_cur = pr->cur + n_move;

	/* integer overflow or out of acp pkg size */
	if ((size_t)pr->cur > SIZE_MAX - n_move ||
	    new_cur > pr->head + pr->total)
		return -EINVAL;

	pr->cur = new_cur;

	return 0;
}

/**
 * ac_pr_is_safe_to_read - check whether it is safe to read more n_move
 *                         bytes from the acp file
 *
 * @pr: pack reader
 * @n_move: number of bytes to check if it is safe to read
 *
 * Return: true when it is safe to read more n_move bytes
 *         false otherwise
 */
static bool ac_pr_is_safe_to_read(const struct ac_pr_state *pr, size_t n_move)
{
	/* pointer overflow */
	if ((size_t)pr->cur > SIZE_MAX - n_move)
		return false;

	if (pr->cur + n_move > pr->head + pr->total)
		return false;

	return true;
}

/**
 * ac_pr_is_end - check if cur is at the end of the acp file
 *
 * @pr: pack reader
 *
 * Return: true when cur is at the end of the acp
 *         false otherwise
 */
static bool ac_pr_is_end(struct ac_pr_state *pr)
{
	return (pr->cur == pr->head + pr->total);
}

/**
 * acp_load_reasons - load list of event codes that can be
 *                    received or posted by ta
 *
 * @pr: pack reader
 * @reasons: out param to hold the list of event codes
 *
 * Return: 0 on success
 *         -EINVAL on invalid parameters
 */
static int acp_load_reasons(struct ac_pr_state *pr,
			    struct ac_ins_reasons **reasons)
{
	size_t len;
	struct ac_ins_reasons *r;

	if (!ac_pr_is_safe_to_read(pr, sizeof(*r)))
		return -EINVAL;

	r = (struct ac_ins_reasons *)pr->cur;

	if (r->len > AC_MAX_INS_REASONS_LENGTH)
		return -EINVAL;

	len = sizeof(*r) + r->len * sizeof(r->data[0]);
	if (!ac_pr_is_safe_to_read(pr, len))
		return -EINVAL;

	*reasons = r;
	return ac_pr_align_move(pr, len);
}

/**
 * acp_load_taid_list - load list of ta ids which ta is allowed
 *                      to communicate with
 *
 * @pr: pack reader
 * @taid_list: out param to hold the loaded ta ids
 *
 * Return: 0 on success
 *         -EINVAL on invalid parameters
 */
static int acp_load_taid_list(struct ac_pr_state *pr,
			      struct ac_ta_id_list **taid_list)
{
	size_t len;
	struct ac_ta_id_list *t;

	if (!ac_pr_is_safe_to_read(pr, sizeof(*t)))
		return -EINVAL;

	t = (struct ac_ta_id_list *)pr->cur;
	if (t->num > AC_MAX_USED_SERVICES)
		return -EINVAL;

	len = sizeof(*t) + t->num * sizeof(t->list[0]);

	if (!ac_pr_is_safe_to_read(pr, len))
		return -EINVAL;

	*taid_list = t;
	return ac_pr_align_move(pr, len);
}

/**
 * acp_load_prop - load property from acp
 *
 * @pr: pack reader
 * @prop: out param to hold the loaded property
 *
 * Return: 0 on success
 *         -EINVAL on invalid parameters
 */
static int acp_load_prop(struct ac_pr_state *pr, struct ac_prop_list **prop)
{
	size_t len;
	struct ac_prop_list *p;

	if (!ac_pr_is_safe_to_read(pr, sizeof(*p)))
		return -EINVAL;

	p = (struct ac_prop_list *)pr->cur;
	if (p->len > AC_MAX_PROPS_LENGTH)
		return -EINVAL;

	len = sizeof(*p) + p->len * sizeof(p->data[0]);

	if (!ac_pr_is_safe_to_read(pr, len))
		return -EINVAL;

	*prop = p;
	return ac_pr_align_move(pr, len);
}

/**
 * acp_load_ta_pack - load ta pack from acp
 *
 * @pr: pack reader
 * @ta_pack: out param to hold the ta pack
 *
 * Return: 0 on success
 *         -EINVAL on invalid parameters
 */
static int acp_load_ta_pack(struct ac_pr_state *pr, char **ta_pack)
{
	size_t len;
	char *t;

	/*8 byte align to obey jeff rule*/
	if (ac_pr_8b_align_move(pr, 0))
		return -EINVAL;

	t = (char *)pr->cur;

	/*
	 *assume ta pack is the last item of one package,
	 *move cursor to the end directly
	 */
	if (pr->cur > pr->head + pr->total)
		return -EINVAL;

	len = pr->head + pr->total - pr->cur;
	if (!ac_pr_is_safe_to_read(pr, len))
		return -EINVAL;

	*ta_pack = t;
	return ac_pr_move(pr, len);
}

/**
 * acp_load_ins_jta_prop_head - load ta manifest header
 *
 * @pr: pack reader
 * @head: out param to hold manifest header
 *
 * Return: 0 on success
 *         -EINVAL on invalid parameters
 */
static int acp_load_ins_jta_prop_head(struct ac_pr_state *pr,
				      struct ac_ins_jta_prop_header **head)
{
	if (!ac_pr_is_safe_to_read(pr, sizeof(**head)))
		return -EINVAL;

	*head = (struct ac_ins_jta_prop_header *)pr->cur;
	return ac_pr_align_move(pr, sizeof(**head));
}

/**
 * acp_load_ins_jta_prop - load ta properties information (ta manifest)
 *
 * @pr: pack reader
 * @pack: out param to hold ta manifest
 *
 * Return: 0 on success
 *         -EINVAL on invalid parameters
 */
static int acp_load_ins_jta_prop(struct ac_pr_state *pr,
				 struct ac_ins_jta_prop *pack)
{
	int ret;

	ret = acp_load_ins_jta_prop_head(pr, &pack->head);
	if (ret)
		return ret;

	ret = acp_load_reasons(pr, &pack->post_reasons);
	if (ret)
		return ret;

	ret = acp_load_reasons(pr, &pack->reg_reasons);
	if (ret)
		return ret;

	ret = acp_load_prop(pr, &pack->prop);
	if (ret)
		return ret;

	ret = acp_load_taid_list(pr, &pack->used_service_list);

	return ret;
}

/**
 * acp_load_ins_jta_head - load ta installation header
 *
 * @pr: pack reader
 * @head: out param to hold the installation header
 *
 * Return: 0 on success
 *         -EINVAL on invalid parameters
 */
static int acp_load_ins_jta_head(struct ac_pr_state *pr,
				 struct ac_ins_ta_header **head)
{
	if (!ac_pr_is_safe_to_read(pr, sizeof(**head)))
		return -EINVAL;

	*head = (struct ac_ins_ta_header *)pr->cur;
	return ac_pr_align_move(pr, sizeof(**head));
}

/**
 * acp_load_ins_jta - load ta installation information from acp
 *
 * @pr: pack reader
 * @pack: out param to hold install information
 *
 * Return: 0 on success
 *         -EINVAL on invalid parameters
 */
static int acp_load_ins_jta(struct ac_pr_state *pr,
			    struct ac_ins_jta_pack *pack)
{
	int ret;

	ret = acp_load_prop(pr, &pack->ins_cond);
	if (ret)
		return ret;

	ret = acp_load_ins_jta_head(pr, &pack->head);

	return ret;
}

/**
 * acp_load_pack_head - load acp pack header
 *
 * @pr: pack reader
 * @head: out param to hold the acp header
 *
 * Return: 0 on success
 *         -EINVAL on invalid parameters
 */
static int acp_load_pack_head(struct ac_pr_state *pr,
			      struct ac_pack_header **head)
{
	if (!ac_pr_is_safe_to_read(pr, sizeof(**head)))
		return -EINVAL;

	*head = (struct ac_pack_header *)pr->cur;
	return ac_pr_align_move(pr, sizeof(**head));
}

/**
 * acp_load_pack - load and parse pack from acp file
 *
 * @raw_pack: acp file content, without the acp CSS header
 * @size: acp file size (without CSS header)
 * @cmd_id: command id
 * @pack: out param to hold the loaded pack
 *
 * Return: 0 on success
 *         -EINVAL on invalid parameters
 */
static int acp_load_pack(const char *raw_pack, unsigned int size,
			 unsigned int cmd_id, struct ac_pack *pack)
{
	int ret;
	struct ac_pr_state pr;
	struct ac_ins_jta_pack_ext *pack_ext;
	struct ac_ins_jta_prop_ext *prop_ext;

	ret = ac_pr_init(&pr, raw_pack, size);
	if (ret)
		return ret;

	if (cmd_id != AC_INSTALL_JTA_PROP) {
		ret = acp_load_pack_head(&pr, &pack->head);
		if (ret)
			return ret;
	}

	if (cmd_id != AC_INSTALL_JTA_PROP && cmd_id != pack->head->cmd_id)
		return -EINVAL;

	switch (cmd_id) {
	case AC_INSTALL_JTA:
		pack_ext = (struct ac_ins_jta_pack_ext *)pack;
		ret = acp_load_ins_jta(&pr, &pack_ext->cmd_pack);
		if (ret)
			break;
		ret = acp_load_ta_pack(&pr, &pack_ext->ta_pack);
		break;
	case AC_INSTALL_JTA_PROP:
		prop_ext = (struct ac_ins_jta_prop_ext *)pack;
		ret = acp_load_ins_jta_prop(&pr, &prop_ext->cmd_pack);
		if (ret)
			break;
		/* Note: the next section is JEFF file,
		 * and not ta_pack(JTA_properties+JEFF file),
		 * but we could reuse the ACP_load_ta_pack() here.
		 */
		ret = acp_load_ta_pack(&pr, &prop_ext->jeff_pack);
		break;
	default:
		return -EINVAL;
	}

	if (!ac_pr_is_end(&pr))
		return -EINVAL;

	return ret;
}

/**
 * acp_pload_ins_jta - load and parse ta pack from acp file
 *
 * Exported function in acp parser API
 *
 * @raw_data: acp file content
 * @size: acp file size
 * @pack: out param to hold the ta pack
 *
 * Return: 0 on success
 *         -EINVAL on invalid parameters
 */
int acp_pload_ins_jta(const void *raw_data, unsigned int size,
		      struct ac_ins_jta_pack_ext *pack)
{
	int ret;

	if (!raw_data || size <= AC_CSS_HEADER_LENGTH || !pack)
		return -EINVAL;

	ret = acp_load_pack((const char *)raw_data + AC_CSS_HEADER_LENGTH,
			    size - AC_CSS_HEADER_LENGTH,
			    AC_INSTALL_JTA, (struct ac_pack *)pack);

	return ret;
}
