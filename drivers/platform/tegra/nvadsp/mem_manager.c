/*
 * mem_manager.c
 *
 * memory manager
 *
 * Copyright (C) 2014-2015 NVIDIA Corporation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt) "%s : %d, " fmt, __func__, __LINE__

#include <linux/list.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/err.h>
#include <linux/seq_file.h>

#include "mem_manager.h"

static void clear_alloc_list(struct mem_manager_info *mm_info);

void *mem_request(void *mem_handle, const char *name, size_t size)
{
	unsigned long flags;
	struct mem_manager_info *mm_info =
		(struct mem_manager_info *)mem_handle;
	struct mem_chunk *mc_iterator = NULL, *best_match_chunk = NULL;
	struct mem_chunk *new_mc = NULL;

	spin_lock_irqsave(&mm_info->lock, flags);

	/* Is mem full? */
	if (list_empty(mm_info->free_list)) {
		pr_err("%s : memory full\n", mm_info->name);
		spin_unlock_irqrestore(&mm_info->lock, flags);
		return ERR_PTR(-ENOMEM);
	}

	/* Find the best size match */
	list_for_each_entry(mc_iterator, mm_info->free_list, node) {
		if (mc_iterator->size >= size) {
			if (best_match_chunk == NULL)
				best_match_chunk = mc_iterator;
			else if (mc_iterator->size < best_match_chunk->size)
				best_match_chunk = mc_iterator;
		}
	}

	/* Is free node found? */
	if (best_match_chunk == NULL) {
		pr_err("%s : no enough memory available\n", mm_info->name);
		spin_unlock_irqrestore(&mm_info->lock, flags);
		return ERR_PTR(-ENOMEM);
	}

	/* Is it exact match? */
	if (best_match_chunk->size == size) {
		list_del(&best_match_chunk->node);
		list_for_each_entry(mc_iterator, mm_info->alloc_list, node) {
			if (best_match_chunk->address < mc_iterator->address) {
				list_add_tail(&best_match_chunk->node,
						&mc_iterator->node);
				strlcpy(best_match_chunk->name, name,
						NAME_SIZE);
				spin_unlock_irqrestore(&mm_info->lock, flags);
				return best_match_chunk;
			}
		}
		list_add(&best_match_chunk->node, mm_info->alloc_list);
		strlcpy(best_match_chunk->name, name, NAME_SIZE);
		spin_unlock_irqrestore(&mm_info->lock, flags);
		return best_match_chunk;
	} else {
		new_mc = kzalloc(sizeof(struct mem_chunk), GFP_ATOMIC);
		if (unlikely(!new_mc)) {
			pr_err("failed to allocate memory for mem_chunk\n");

			spin_unlock_irqrestore(&mm_info->lock, flags);
			return ERR_PTR(-ENOMEM);
		}
		new_mc->address = best_match_chunk->address;
		new_mc->size = size;
		strlcpy(new_mc->name, name, NAME_SIZE);
		best_match_chunk->address += size;
		best_match_chunk->size -= size;
		list_for_each_entry(mc_iterator, mm_info->alloc_list, node) {
			if (new_mc->address < mc_iterator->address) {
				list_add_tail(&new_mc->node,
						&mc_iterator->node);
				spin_unlock_irqrestore(&mm_info->lock, flags);
				return new_mc;
			}
		}
		list_add_tail(&new_mc->node, mm_info->alloc_list);
		spin_unlock_irqrestore(&mm_info->lock, flags);
		return new_mc;
	}
}
EXPORT_SYMBOL(mem_request);

/*
 * Find the node with sepcified address and remove it from list
 */
bool mem_release(void *mem_handle, void *handle)
{
	unsigned long flags;
	struct mem_manager_info *mm_info =
		(struct mem_manager_info *)mem_handle;
	struct mem_chunk *mc_curr = NULL, *mc_prev = NULL;
	struct mem_chunk *mc_free = (struct mem_chunk *)handle;

	pr_debug(" addr = %lu, size = %lu, name = %s\n",
			mc_free->address, mc_free->size, mc_free->name);

	spin_lock_irqsave(&mm_info->lock, flags);

	list_for_each_entry(mc_curr, mm_info->free_list, node) {
		if (mc_free->address < mc_curr->address) {

			strlcpy(mc_free->name, "FREE", NAME_SIZE);

			/* adjacent next free node */
			if (mc_curr->address ==
				(mc_free->address + mc_free->size)) {

				mc_curr->address = mc_free->address;
				mc_curr->size += mc_free->size;
				list_del(&mc_free->node);
				kfree(mc_free);

				/* and adjacent prev free node */
				if ((mc_prev != NULL) &&
				   ((mc_prev->address + mc_prev->size) ==
						mc_curr->address)) {

					mc_prev->size += mc_curr->size;
					list_del(&mc_curr->node);
					kfree(mc_curr);
				}
			}
			/* adjacent prev free node */
			else if ((mc_prev != NULL) &&
				((mc_prev->address + mc_prev->size) ==
						mc_free->address)) {

				mc_prev->size += mc_free->size;
				list_del(&mc_free->node);
				kfree(mc_free);
			} else {
				list_del(&mc_free->node);
				list_add_tail(&mc_free->node,
						&mc_curr->node);
			}
			spin_unlock_irqrestore(&mm_info->lock, flags);
			return true;
		}
		mc_prev = mc_curr;
	}
	spin_unlock_irqrestore(&mm_info->lock, flags);
	return false;
}
EXPORT_SYMBOL(mem_release);

inline unsigned long mem_get_address(void *handle)
{
	struct mem_chunk *mc = (struct mem_chunk *)handle;
	return mc->address;
}
EXPORT_SYMBOL(mem_get_address);

void mem_print(void *mem_handle)
{
	struct mem_manager_info *mm_info =
		(struct mem_manager_info *)mem_handle;
	struct mem_chunk *mc_iterator = NULL;

	pr_info("------------------------------------\n");
	pr_info("%s ALLOCATED\n", mm_info->name);
	list_for_each_entry(mc_iterator, mm_info->alloc_list, node) {
		pr_info("  addr = %lu, size = %lu, name = %s\n",
			mc_iterator->address, mc_iterator->size,
			mc_iterator->name);
	}

	pr_info("%s FREE\n", mm_info->name);
	list_for_each_entry(mc_iterator, mm_info->free_list, node) {
		pr_info("  addr = %lu, size = %lu, name = %s\n",
			mc_iterator->address, mc_iterator->size,
			mc_iterator->name);
	}

	pr_info("------------------------------------\n");
}
EXPORT_SYMBOL(mem_print);

void mem_dump(void *mem_handle, struct seq_file *s)
{
	struct mem_manager_info *mm_info =
		(struct mem_manager_info *)mem_handle;
	struct mem_chunk *mc_iterator = NULL;

	seq_puts(s, "---------------------------------------\n");
	seq_printf(s, "%s ALLOCATED\n", mm_info->name);
	list_for_each_entry(mc_iterator, mm_info->alloc_list, node) {
		seq_printf(s, "  addr = %lu, size = %lu, name = %s\n",
			mc_iterator->address, mc_iterator->size,
			mc_iterator->name);
	}

	seq_printf(s, "%s FREE\n", mm_info->name);
	list_for_each_entry(mc_iterator, mm_info->free_list, node) {
		seq_printf(s, "  addr = %lu, size = %lu, name = %s\n",
			mc_iterator->address, mc_iterator->size,
			mc_iterator->name);
	}

	seq_puts(s, "---------------------------------------\n");
}
EXPORT_SYMBOL(mem_dump);

static void clear_alloc_list(struct mem_manager_info *mm_info)
{
	struct list_head *curr, *next;
	struct mem_chunk *mc = NULL;

	list_for_each_safe(curr, next, mm_info->alloc_list) {
		mc = list_entry(curr, struct mem_chunk, node);
		pr_debug("  addr = %lu, size = %lu, name = %s\n",
			mc->address, mc->size,
			mc->name);
		mem_release(mm_info, mc);
	}
}

void *create_mem_manager(const char *name, unsigned long start_address,
				unsigned long size)
{
	void *ret = NULL;
	struct mem_chunk *mc;
	struct mem_manager_info *mm_info =
			kzalloc(sizeof(struct mem_manager_info), GFP_KERNEL);
	if (unlikely(!mm_info)) {
		pr_err("failed to allocate memory for mem_manager_info\n");
		return ERR_PTR(-ENOMEM);
	}

	strlcpy(mm_info->name, name, NAME_SIZE);

	mm_info->alloc_list = kzalloc(sizeof(struct list_head), GFP_KERNEL);
	if (unlikely(!mm_info->alloc_list)) {
		pr_err("failed to allocate memory for alloc_list\n");
		ret = ERR_PTR(-ENOMEM);
		goto free_mm_info;
	}

	mm_info->free_list = kzalloc(sizeof(struct list_head), GFP_KERNEL);
	if (unlikely(!mm_info->free_list)) {
		pr_err("failed to allocate memory for free_list\n");
		ret = ERR_PTR(-ENOMEM);
		goto free_alloc_list;
	}

	INIT_LIST_HEAD(mm_info->alloc_list);
	INIT_LIST_HEAD(mm_info->free_list);

	mm_info->start_address = start_address;
	mm_info->size = size;

	/* Add whole memory to free list */
	mc = kzalloc(sizeof(struct mem_chunk), GFP_KERNEL);
	if (unlikely(!mc)) {
		pr_err("failed to allocate memory for mem_chunk\n");
		ret = ERR_PTR(-ENOMEM);
		goto free_free_list;
	}

	mc->address = mm_info->start_address;
	mc->size = mm_info->size;
	strlcpy(mc->name, "FREE", NAME_SIZE);
	list_add(&mc->node, mm_info->free_list);
	spin_lock_init(&mm_info->lock);

	return (void *)mm_info;

free_free_list:
	kfree(mm_info->free_list);
free_alloc_list:
	kfree(mm_info->alloc_list);
free_mm_info:
	kfree(mm_info);

	return ret;
}
EXPORT_SYMBOL(create_mem_manager);

void destroy_mem_manager(void *mem_handle)
{
	struct mem_manager_info *mm_info =
		(struct mem_manager_info *)mem_handle;
	struct mem_chunk *mc_last = NULL;

	/* Clear all allocated memory */
	clear_alloc_list(mm_info);

	mc_last = list_entry((mm_info->free_list)->next,
			struct mem_chunk, node);
	list_del(&mc_last->node);

	kfree(mc_last);
	kfree(mm_info->alloc_list);
	kfree(mm_info->free_list);
	kfree(mm_info);
}
EXPORT_SYMBOL(destroy_mem_manager);
