/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/fs.h>
#include <asm/segment.h>
#include <asm/uaccess.h>

#include <linux/slab.h>
#include <linux/workqueue.h>

#include <linux/tegra_nvadsp.h>

#include "adspff.h"

struct file_struct {
	struct file *fp;
	unsigned long long wr_offset;
	unsigned long long rd_offset;
};


static spinlock_t adspff_lock;

/******************************************************************************
* Kernel file functions
******************************************************************************/

struct file *file_open(const char *path, int flags, int rights)
{
	struct file *filp = NULL;
	mm_segment_t oldfs;
	int err = 0;

	oldfs = get_fs();
	set_fs(get_ds());
	filp = filp_open(path, flags, rights);
	set_fs(oldfs);
	if (IS_ERR(filp)) {
		err = PTR_ERR(filp);
		return NULL;
	}
	return filp;
}

void file_close(struct file *file)
{
	filp_close(file, NULL);
}

int file_write(struct file *file, unsigned long long *offset,
				unsigned char *data, unsigned int size)
{
	mm_segment_t oldfs;
	int ret = 0;

	oldfs = get_fs();
	set_fs(get_ds());

	ret = vfs_write(file, data, size, offset);

	set_fs(oldfs);
	return ret;
}

uint32_t file_read(struct file *file, unsigned long long *offset,
				unsigned char *data, unsigned int size)
{
	mm_segment_t oldfs;
	uint32_t ret = 0;

	oldfs = get_fs();
	set_fs(get_ds());

	ret = vfs_read(file, data, size, offset);

	set_fs(oldfs);

	return ret;
}

/******************************************************************************
* ADSPFF file functions
******************************************************************************/

static struct adspff_shared_state_t *adspff;
static struct nvadsp_mbox rx_mbox;

/**																*
 * w  - open for writing (file need not exist)					*
 * a  - open for appending (file need not exist)				*
 * r+ - open for reading and writing, start at beginning		*
 * w+ - open for reading and writing (overwrite file)			*
 * a+ - open for reading and writing (append if file exists)	*/

void set_flags(union adspff_message_t *m, int *flags)
{
	if (0 == strcmp(m->msg.payload.fopen_msg.modes, "r+"))
		*flags = O_RDWR;

	else if (0 == strcmp(m->msg.payload.fopen_msg.modes, "w+"))
		*flags = O_CREAT | O_RDWR | O_TRUNC;

	else if (0 == strcmp(m->msg.payload.fopen_msg.modes, "a+"))
		*flags = O_APPEND | O_RDWR;

	else if (0 == strcmp(m->msg.payload.fopen_msg.modes, "r"))
		*flags = O_RDONLY;

	else if (0 == strcmp(m->msg.payload.fopen_msg.modes, "w"))
		*flags = O_CREAT | O_WRONLY | O_TRUNC;

	else if (0 == strcmp(m->msg.payload.fopen_msg.modes, "a"))
		*flags = O_CREAT | O_APPEND | O_WRONLY;

	else
		*flags = O_CREAT | O_RDWR;
}

void adspff_fopen(struct work_struct *work)
{
	union adspff_message_t *message;
	union adspff_message_t *msg_recv;
	int flags = 0, ret = 0;

	struct file_struct *file = kzalloc(sizeof(struct file_struct), GFP_KERNEL);

	message = kzalloc(sizeof(union adspff_message_t), GFP_KERNEL);
	msg_recv = kzalloc(sizeof(union adspff_message_t), GFP_KERNEL);

	message->msgq_msg.size = MSGQ_MSG_SIZE(struct fopen_msg_t);

	ret = msgq_dequeue_message(&adspff->msgq_send.msgq,
			(msgq_message_t *)message);

	if (ret < 0) {
		pr_err("fopen Dequeue failed %d.", ret);
		kfree(message);
		kfree(msg_recv);
		return;
	}

	set_flags(message, &flags);

	file->fp = file_open(
		(const char *)message->msg.payload.fopen_msg.fname,
		flags, S_IRWXU|S_IRWXG|S_IRWXO);

	file->wr_offset = 0;
	file->rd_offset = 0;

	msg_recv->msgq_msg.size = MSGQ_MSG_SIZE(struct fopen_recv_msg_t);
	msg_recv->msg.payload.fopen_recv_msg.file = (int64_t)file;

	ret = msgq_queue_message(&adspff->msgq_recv.msgq,
			(msgq_message_t *)msg_recv);
	if (ret < 0) {
		pr_err("fopen Enqueue failed %d.", ret);
		kfree(message);
		kfree(msg_recv);
		return;
	}

	nvadsp_mbox_send(&rx_mbox, adspff_cmd_fopen_recv,
				NVADSP_MBOX_SMSG, 0, 0);
	kfree(message);
	kfree(msg_recv);
}

void adspff_fclose(struct work_struct *work)
{
	union adspff_message_t *message;
	struct file_struct *file = NULL;
	int32_t ret = 0;

	message = kzalloc(sizeof(union adspff_message_t), GFP_KERNEL);

	message->msgq_msg.size = MSGQ_MSG_SIZE(struct fclose_msg_t);

	ret = msgq_dequeue_message(&adspff->msgq_send.msgq,
				(msgq_message_t *)message);

	if (ret < 0) {
		pr_err("fclose Dequeue failed %d.", ret);
		kfree(message);
		return;
	}
	file = (struct file_struct *)message->msg.payload.fclose_msg.file;
	if (file) {
		file_close(file->fp);
		kfree(file);
		file = NULL;
	}
	kfree(message);
}

void adspff_fwrite(struct work_struct *work)
{
	union adspff_message_t message;
	union adspff_message_t *msg_recv;
	struct file_struct *file = NULL;
	int ret = 0;
	uint32_t size = 0;
	uint32_t bytes_to_write = 0;
	uint32_t bytes_written = 0;

	msg_recv = kzalloc(sizeof(union adspff_message_t), GFP_KERNEL);
	msg_recv->msgq_msg.size = MSGQ_MSG_SIZE(struct ack_msg_t);

	message.msgq_msg.size = MSGQ_MSG_SIZE(struct fwrite_msg_t);
	ret = msgq_dequeue_message(&adspff->msgq_send.msgq,
				(msgq_message_t *)&message);
	if (ret < 0) {
		pr_err("fwrite Dequeue failed %d.", ret);
		return;
	}

	file = (struct file_struct *)message.msg.payload.fwrite_msg.file;
	size = message.msg.payload.fwrite_msg.size;

	bytes_to_write = ((adspff->write_buf.read_index + size) < ADSPFF_SHARED_BUFFER_SIZE) ?
		size : (ADSPFF_SHARED_BUFFER_SIZE - adspff->write_buf.read_index);
	ret = file_write(file->fp, &file->wr_offset,
			adspff->write_buf.data + adspff->write_buf.read_index, bytes_to_write);
	bytes_written += ret;

	if ((size - bytes_to_write) > 0) {
		ret = file_write(file->fp, &file->wr_offset,
				adspff->write_buf.data, size - bytes_to_write);
		bytes_written += ret;
	}

	adspff->write_buf.read_index =
		(adspff->write_buf.read_index + size) % ADSPFF_SHARED_BUFFER_SIZE;

	/* send ack */
	msg_recv->msg.payload.ack_msg.size = bytes_written;
	ret = msgq_queue_message(&adspff->msgq_recv.msgq,
			(msgq_message_t *)msg_recv);

	if (ret < 0) {
		pr_err("fread Enqueue failed %d.", ret);
		kfree(msg_recv);
		return;
	}
	nvadsp_mbox_send(&rx_mbox, adspff_cmd_ack,
			NVADSP_MBOX_SMSG, 0, 0);
	kfree(msg_recv);
}

void adspff_fread(struct work_struct *work)
{
	union adspff_message_t *message;
	union adspff_message_t *msg_recv;
	struct file_struct *file = NULL;
	uint32_t bytes_free;
	uint32_t wi = adspff->read_buf.write_index;
	uint32_t ri = adspff->read_buf.read_index;
	uint8_t can_wrap = 0;
	uint32_t size = 0, size_read = 0;
	int32_t ret = 0;

	if (ri <= wi) {
		bytes_free = ADSPFF_SHARED_BUFFER_SIZE - wi + ri - 1;
		can_wrap = 1;
	} else {
		bytes_free = ri - wi - 1;
		can_wrap = 0;
	}
	message = kzalloc(sizeof(union adspff_message_t), GFP_KERNEL);
	msg_recv = kzalloc(sizeof(union adspff_message_t), GFP_KERNEL);

	msg_recv->msgq_msg.size = MSGQ_MSG_SIZE(struct ack_msg_t);
	message->msgq_msg.size = MSGQ_MSG_SIZE(struct fread_msg_t);

	ret = msgq_dequeue_message(&adspff->msgq_send.msgq,
				(msgq_message_t *)message);

	if (ret < 0) {
		pr_err("fread Dequeue failed %d.", ret);
		kfree(message);
		kfree(msg_recv);
		return;
	}

	file = (struct file_struct *)message->msg.payload.fread_msg.file;
	size = message->msg.payload.fread_msg.size;
	if (bytes_free < size) {
		size_read = 0;
		goto send_ack;
	}

	if (can_wrap) {
		uint32_t bytes_to_read = (size < (ADSPFF_SHARED_BUFFER_SIZE - wi)) ?
			size : (ADSPFF_SHARED_BUFFER_SIZE - wi);
		ret = file_read(file->fp, &file->rd_offset,
				adspff->read_buf.data + wi, bytes_to_read);
		size_read = ret;
		if (ret < bytes_to_read)
			goto send_ack;
		if ((size - bytes_to_read) > 0) {
			ret = file_read(file->fp, &file->rd_offset,
					adspff->read_buf.data, size - bytes_to_read);
			size_read += ret;
			goto send_ack;
		}
	} else {
		ret = file_read(file->fp, &file->rd_offset,
				adspff->read_buf.data + wi, size);
		size_read = ret;
		goto send_ack;
	}
send_ack:
	msg_recv->msg.payload.ack_msg.size = size_read;
	ret = msgq_queue_message(&adspff->msgq_recv.msgq,
			(msgq_message_t *)msg_recv);

	if (ret < 0) {
		pr_err("fread Enqueue failed %d.", ret);
		kfree(message);
		kfree(msg_recv);
		return;
	}
	adspff->read_buf.write_index =
		(adspff->read_buf.write_index + size_read) % ADSPFF_SHARED_BUFFER_SIZE;

	nvadsp_mbox_send(&rx_mbox, adspff_cmd_ack,
			NVADSP_MBOX_SMSG, 0, 0);
	kfree(message);
	kfree(msg_recv);
}

static struct workqueue_struct *adspff_wq;
DECLARE_WORK(fopen_work, adspff_fopen);
DECLARE_WORK(fwrite_work, adspff_fwrite);
DECLARE_WORK(fread_work, adspff_fread);
DECLARE_WORK(fclose_work, adspff_fclose);

/******************************************************************************
* ADSP mailbox message handler
******************************************************************************/


static int adspff_msg_handler(uint32_t msg, void *data)
{
	unsigned long flags;

	spin_lock_irqsave(&adspff_lock, flags);
	switch (msg) {
	case adspff_cmd_fopen: {
		queue_work(adspff_wq, &fopen_work);
	}
	break;
	case adspff_cmd_fclose: {
		queue_work(adspff_wq, &fclose_work);
	}
	break;
	case adspff_cmd_fwrite: {
		queue_work(adspff_wq, &fwrite_work);
	}
	break;
	case adspff_cmd_fread: {
		queue_work(adspff_wq, &fread_work);
	}
	break;
	default:
	pr_err("Unsupported mbox msg %d.\n", msg);
	}
	spin_unlock_irqrestore(&adspff_lock, flags);

	return 0;
}

int adspff_init(void)
{
	int ret = 0;
	nvadsp_app_handle_t handle;
	nvadsp_app_info_t *app_info;

	handle = nvadsp_app_load("adspff", "adspff.elf");
	if (!handle)
		return -1;

	app_info = nvadsp_app_init(handle, NULL);
	if (!app_info) {
		pr_err("unable to init app adspff\n");
		return -1;
	}

	adspff = ADSPFF_SHARED_STATE(app_info->mem.shared);

	ret = nvadsp_mbox_open(&rx_mbox, &adspff->mbox_id,
			"adspff", adspff_msg_handler, NULL);

	if (ret < 0) {
		pr_err("Failed to open mbox %d", adspff->mbox_id);
		return -1;
	}

	if (adspff_wq == NULL)
		adspff_wq = create_singlethread_workqueue("adspff_wq");

	spin_lock_init(&adspff_lock);

	return 0;
}

void adspff_exit(void)
{
	nvadsp_mbox_close(&rx_mbox);
	flush_workqueue(adspff_wq);
	destroy_workqueue(adspff_wq);
}
