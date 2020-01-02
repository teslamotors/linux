/*
 * scs_flash - mtd device for SCS qspi flash
 *
 * Copyright (c) 2017 Tesla Inc
 *
 * This code is GPL
 *
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/vmalloc.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/of_reserved_mem.h>
#include <linux/dma-mapping.h>

#include "MBOX.h"
#include "mailbox_A72_SCS_interface.h"

#define read32(v) ioread32(v)
#define write32(addr, v) iowrite32(v, addr)

#define NN_MAX		12
#define RSP_NN_MAX	11

enum {
        MBOX_INT_PRIO_LOW,
        MBOX_INT_PRIO_HIGH,
        MBOX_INT_PRIO_MAX,
};

struct trav_mbox {
        struct device *dev;
        void __iomem *mbox_base;
        int irq[MBOX_INT_PRIO_MAX];

	/* lower level locks for mbox transport */
	spinlock_t lock;
        struct mutex mbox_mtx;

	/* higher level lock for flash access */
        struct mutex flash_mtx;

        struct mbox_issr *issr_tx_lo;
        struct mbox_issr *issr_rx_lo;
        struct mbox_issr *issr_tx_hi;
        struct mbox_issr *issr_rx_hi;
        struct mbox_int *int_rx;
        struct mbox_int *int_tx;

	int sysfs_inited;

	struct completion tx_completion[MBOX_INT_PRIO_MAX];
	struct completion rx_completion[MBOX_INT_PRIO_MAX];

	/* TODO: should change to list and queue instead */
	uint32_t recv_service_id_hi;
	uint32_t recv_service_id_lo;
	uint32_t recv_request_id_hi;
	uint32_t recv_request_id_lo;
	uint32_t recv_resp_hi;
	uint32_t recv_resp_lo;

	/* return */
	uint32_t *response_status;
	uint32_t *return_value;
	int return_nn;
	uint32_t *resp_response_status;
	uint32_t *resp_return_value;
	int resp_return_nn;
};

static void add_rsp_info(struct trav_mbox *mbox, int pri, uint32_t service_id, uint32_t request_id, uint32_t resp)
{
	if (pri) {
		mbox->recv_service_id_hi = service_id;
		mbox->recv_request_id_hi = request_id;
		mbox->recv_resp_hi = resp;
	} else {
		mbox->recv_service_id_lo = service_id;
		mbox->recv_request_id_lo = request_id;
		mbox->recv_resp_lo = resp;
	}
}

static void remove_rsp_info(struct trav_mbox *mbox, int pri, uint32_t service_id, uint32_t request_id, uint32_t resp)
{
	return add_rsp_info(mbox, pri, 0, 0, 0);
}

/* 0: match */
static int compare_rsp_info(struct trav_mbox *mbox, int pri, uint32_t service_id, uint32_t request_id, uint32_t resp)
{
	if (pri) {
		if (mbox->recv_service_id_hi == service_id &&
		    mbox->recv_request_id_hi == request_id &&
		    mbox->recv_resp_hi == resp)
                        return 0;
	} else {
		if (mbox->recv_service_id_lo == service_id &&
		    mbox->recv_request_id_lo == request_id &&
		    mbox->recv_resp_lo == resp)
			return 0;
	}

	return 1;
}

static uint32_t request_id_counter = 0;

#define MAILBOX_TX_TIMEOUT	(msecs_to_jiffies(1000))

static int mailbox_send_request_nn(struct trav_mbox *mbox, int pri, uint32_t service_id, uint32_t request_id, uint32_t argc, const uint32_t * args, uint32_t * response_status, uint32_t * return_value, int return_nn, uint32_t resp, uint32_t *resp_response_status, uint32_t *resp_return_value, int resp_return_nn)
{
	uint32_t n;
	unsigned long timeout;

        struct mbox_issr *issr_tx;

	spin_lock_irq(&mbox->lock);

	if (pri)
                issr_tx = mbox->issr_tx_hi;
	else
                issr_tx = mbox->issr_tx_lo;

	reinit_completion(&mbox->tx_completion[pri]);

	mbox->response_status = response_status;
	mbox->return_value = return_value;
	mbox->return_nn = return_nn;

	if (resp) {
		mbox->resp_response_status = resp_response_status;
		mbox->resp_return_value = resp_return_value;
		mbox->resp_return_nn = resp_return_nn;
		add_rsp_info(mbox, pri, service_id, request_id, resp);
		reinit_completion(&mbox->rx_completion[pri]);
        }

	// copy the arguments
	for (n = 0; n < argc; n++)
		write32(&issr_tx->issr[ISSR_INDEX_DATA0 + n], args[n]);

	// set the request ID
	write32(&issr_tx->issr[ISSR_INDEX_REQUEST], request_id);

	// set the command - this will trigger the operation in polling mode
	write32(&issr_tx->issr[ISSR_INDEX_SERVICE], service_id);

	// send an interrupt - this will trigger the operation in interrupt mode
	write32(&mbox->int_tx->intgr, 1U<<(pri ? 16 : 0));

	spin_unlock_irq(&mbox->lock);

	// wait for the other side to acknowledge the request
	timeout = wait_for_completion_timeout(&mbox->tx_completion[pri], MAILBOX_TX_TIMEOUT);

	// we timed out while waiting for the SCS to acknowlege the receipt of the request
	if (0 == timeout) {
		pr_info("mailbox_send_request: timeout! request: 0x%08x\n", request_id);
		spin_lock_irq(&mbox->lock);
		mbox->response_status = NULL;
		mbox->return_value = NULL;
		mbox->return_nn = 0;
		if (resp) {
			mbox->resp_response_status = NULL;
			mbox->resp_return_value = NULL;
			mbox->resp_return_nn = 0;
			remove_rsp_info(mbox, pri, service_id, request_id, resp);
		}
		spin_unlock_irq(&mbox->lock);
		return -1;
	}

	return 0;
}

static irqreturn_t mailbox_scs_isr(int pri, int irq, void *dev_id)
{
	struct trav_mbox *mbox = (struct trav_mbox *)dev_id;
	uint32_t recv_service_id;

	struct mbox_issr *issr_tx;
	struct mbox_issr *issr_rx;
	unsigned long flags;
	uint32_t ack_mask;
	uint32_t rsp_mask;
	uint32_t rx_intsr;

	spin_lock_irqsave(&mbox->lock, flags);

	if (pri) {
		issr_tx = mbox->issr_tx_hi;
		issr_rx = mbox->issr_rx_hi;
	} else {
		issr_tx = mbox->issr_tx_lo;
		issr_rx = mbox->issr_rx_lo;
	}

	ack_mask = 1U<<(pri ? 31 : 15);
	rsp_mask = 1U<<(pri ? 16 : 0);

	rx_intsr = read32(&mbox->int_rx->intsr);
	pr_debug("[mailbox-scs] %s, %d, pri=%d rx intgr 0x%08x intmr: 0x%08x intcr: 0x%08x intsr: 0x%08x\n", __func__, __LINE__, pri, read32(&mbox->int_rx->intgr), read32(&mbox->int_rx->intmr), read32(&mbox->int_rx->intcr), rx_intsr);

	/* ack */
	if (0 == read32(&issr_tx->issr[ISSR_INDEX_SERVICE]) && ack_mask == (rx_intsr & ack_mask)) {
		int handled = 0;

		pr_debug("[mailbox-scs] %s, %d, pri=%d\n", __func__, __LINE__, pri);

		if (NULL != mbox->response_status) {
			*mbox->response_status = read32(&issr_tx->issr[ISSR_INDEX_STATUS]);
			mbox->response_status = NULL;
			handled = 1;
		}

		if (NULL != mbox->return_value) {
			int n;

			if (mbox->return_nn > NN_MAX)
				mbox->return_nn = NN_MAX;
			for (n = 0; n < mbox->return_nn; n++)
				mbox->return_value[n] = read32(&issr_tx->issr[ISSR_INDEX_DATA1 + n]);
			mbox->return_value = NULL;
			mbox->return_nn = 0;
			handled = 1;
		}
		if (handled)
			complete(&mbox->tx_completion[pri]);
		write32(&mbox->int_rx->intcr, ack_mask);  /* ack */
	}

	/* rsp */
	recv_service_id = read32(&issr_rx->issr[ISSR_INDEX_SERVICE]);
	if (0 != recv_service_id && rsp_mask == (rx_intsr & rsp_mask)) {
		uint32_t recv_request_id = read32(&issr_rx->issr[ISSR_INDEX_REQUEST]);
		uint32_t response_code = read32(&issr_rx->issr[ISSR_INDEX_DATA0]);
		int handled = 0;

		pr_debug("[mailbox-scs] %s, %d, pri=%d\n", __func__, __LINE__, pri);

		/* check if it is response at first */
		if ((response_code & MBOX_COMMAND_RESPONSE_ID) == MBOX_COMMAND_RESPONSE_ID) {
			/* check if is the exact one */
			if (0 == compare_rsp_info(mbox, pri, recv_service_id, recv_request_id, response_code)) {
				remove_rsp_info(mbox, pri, recv_service_id, recv_request_id, response_code);

				if (NULL != mbox->resp_response_status) {
					*mbox->resp_response_status = read32(&issr_rx->issr[ISSR_INDEX_DATA1]);
					mbox->resp_response_status =NULL;
				}

				if (NULL != mbox->resp_return_value) {
					int n;

					if (mbox->resp_return_nn > RSP_NN_MAX)
						mbox->resp_return_nn = RSP_NN_MAX;
					for (n = 0; n < mbox->resp_return_nn; n++)
						mbox->resp_return_value[n] = read32(&issr_rx->issr[ISSR_INDEX_DATA2 + n]);
					mbox->resp_return_value = NULL;
					mbox->resp_return_nn = 0;
				}
				handled = 1;
				complete(&mbox->rx_completion[pri]);
			}
		}

		write32(&mbox->int_rx->intcr, rsp_mask);   /* rsp */
		if (!handled) {
			write32(&issr_rx->issr[ISSR_INDEX_SERVICE], 0);
			write32(&mbox->int_tx->intgr, ack_mask);
		}

		/* TODO: handle request from SCS ? */
	}

	spin_unlock_irqrestore(&mbox->lock, flags);
	return IRQ_HANDLED;
}

static irqreturn_t mailbox_scs_isr_hi(int irq, void *dev_id)
{
	return mailbox_scs_isr(MBOX_INT_PRIO_HIGH, irq, dev_id);
}

static irqreturn_t mailbox_scs_isr_lo(int irq, void *dev_id)
{
	return mailbox_scs_isr(MBOX_INT_PRIO_LOW, irq, dev_id);
}

static int mailbox_wait_for_response(struct trav_mbox *mbox, int pri, uint32_t expected_service_id, uint32_t request_id, uint32_t expected_response_code, unsigned long rx_timeout)
{
	unsigned long timeout;
	unsigned long flags;

	struct mbox_issr *issr_rx;

	timeout = wait_for_completion_timeout(&mbox->rx_completion[pri], rx_timeout);

	spin_lock_irqsave(&mbox->lock, flags);

	if (pri)
		issr_rx = mbox->issr_rx_hi;
	else
		issr_rx = mbox->issr_rx_lo;

	if (0 == timeout) {
		pr_info("mailbox_wait_for_response: timeout! expected: 0x%08x\n", request_id);
		mbox->resp_response_status = NULL;
		mbox->resp_return_value = NULL;
		mbox->resp_return_nn = 0;
		remove_rsp_info(mbox, pri, expected_service_id, request_id, expected_response_code);
		spin_unlock_irqrestore(&mbox->lock, flags);
		return -1;
	}

	write32(&issr_rx->issr[ISSR_INDEX_SERVICE], 0);

	write32(&mbox->int_tx->intgr, 1U<<(pri ? 31 : 15));

	spin_unlock_irqrestore(&mbox->lock, flags);

	return 0;
}

static void test_op_nn_with_nn_timeout(struct trav_mbox *mbox, int pri, uint32_t service_id,
		uint32_t prov_cmd, uint32_t *prov_cmd_arg, int nn,
		uint32_t *p_prov_cmd_status, uint32_t *p_prov_cmd_return_arg, int return_nn,
		uint32_t prov_resp, uint32_t *p_prov_resp_status, uint32_t *p_prov_resp_arg, int resp_return_nn,
		unsigned long rx_timeout)
{
	int rv, resp_rv;
	uint32_t request_args[NN_MAX + 1];
	uint32_t response_status;
	uint32_t return_value[NN_MAX];
	uint32_t resp_response_status;
	uint32_t resp_return_value[RSP_NN_MAX];
	int i;

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// request SCS to for prov
	////////////////////////////////////////////////////////////////////////////////////////////////////

	if (nn > NN_MAX)
		nn = NN_MAX;
	if (return_nn > NN_MAX)
		return_nn = NN_MAX;
	if (resp_return_nn > RSP_NN_MAX)
		resp_return_nn = RSP_NN_MAX;

	request_id_counter++;
	request_args[0] = prov_cmd;
	for (i = 0; i < nn; i++)
		request_args[i + 1] = prov_cmd_arg[i];
	response_status = 0;

	resp_rv = -1;
	resp_response_status = -1;

	mutex_lock(&mbox->mbox_mtx);

	rv = mailbox_send_request_nn(mbox, pri, service_id, request_id_counter, 1 + nn, request_args, &response_status, prov_resp ? NULL : return_value, prov_resp ? 0 : return_nn, prov_resp, &resp_response_status, prov_resp ? resp_return_value : NULL, prov_resp ? resp_return_nn : 0);

	/* sync ? */
	if (!prov_resp) {
		if (0 != rv) {
			pr_info("mailbox_send_request(0x%08x) failed with error 0x%08x\n", request_id_counter, rv);
			goto out;
		}

		if (A72_MAILBOX_STATUS_OK != response_status) {
			pr_info("mailbox_send_request(0x%08x) failed with response status 0x%08x\n", request_id_counter, response_status);
			goto out;
		}

		for (i = 0; i < return_nn; i++) {
			p_prov_cmd_return_arg[i] = return_value[i];
			pr_debug("test_op_nn_with_nn: return_value[%d]: 0x%08x\n", i, return_value[i]);
		}
		goto out;
	}

	/* async */
	if (0 != rv || A72_MAILBOX_STATUS_REQUEST_PENDING != response_status) {
		pr_info("mailbox_send_request(0x%08x) failed with response status 0x%08x and error 0x%08x\n", request_id_counter, response_status, rv);
		goto out;
	}

	// wait for the asynchronous response
	resp_rv = mailbox_wait_for_response(mbox, pri, service_id, request_id_counter, prov_resp, rx_timeout);
	if (0 != resp_rv) {
		pr_info("mailbox_wait_for_response(0x%08x) failed!\n", request_id_counter);
		goto out;
	}

	if (0 != resp_response_status) {
		pr_info("mailbox_wait_for_response(0x%08x) returned response status 0x%08x\n", request_id_counter, resp_response_status);
		goto out;
	}

	for (i = 0; i < resp_return_nn; i++) {
		p_prov_resp_arg[i] = resp_return_value[i];
		pr_debug("test_op_nn_with_nn: resp_return_value[%d]: 0x%08x\n", i, resp_return_value[i]);
	}

out:
	mutex_unlock(&mbox->mbox_mtx);
	if (p_prov_cmd_status)
		*p_prov_cmd_status = rv ? : response_status;
	if (prov_resp && p_prov_resp_status)
		*p_prov_resp_status = resp_rv ? : resp_response_status;
}

/* default timeout for response to 3s */
#define MAILBOX_RX_TIMEOUT	(msecs_to_jiffies(3000))
static void test_op_nn_with_nn(struct trav_mbox *mbox, int pri, uint32_t service_id,
		uint32_t prov_cmd, uint32_t *prov_cmd_arg, int nn,
		uint32_t *p_prov_cmd_status, uint32_t *p_prov_cmd_return_arg, int return_nn,
		uint32_t prov_resp, uint32_t *p_prov_resp_status, uint32_t *p_prov_resp_arg, int resp_return_nn)
{
	test_op_nn_with_nn_timeout(mbox, pri, service_id,
			   prov_cmd, prov_cmd_arg, nn,
			   p_prov_cmd_status, p_prov_cmd_return_arg, return_nn,
			   prov_resp, p_prov_resp_status, p_prov_resp_arg, resp_return_nn,
			   MAILBOX_RX_TIMEOUT);
}

static int test_service_version(struct trav_mbox *mbox, int pri, uint32_t service_id, uint32_t cmd, char *name)
{
	uint32_t response_status;
	uint32_t return_value;

	test_op_nn_with_nn(mbox, pri, service_id, cmd, NULL, 0, &response_status, &return_value, 1, 0, NULL, NULL, 0);

	if (0 == response_status)
		pr_info("MAILBOX_SERVICE %s version: %u\n", name, return_value);

	return 0;
}

static int test_get_one_with_one(struct trav_mbox *mbox, int pri, uint32_t service_id, uint32_t cmd, uint32_t index, uint32_t *val)
{
	uint32_t response_status;
	uint32_t return_value;

	test_op_nn_with_nn(mbox, pri, service_id, cmd, &index, 1, &response_status, &return_value, 1, 0, NULL, NULL, 0);

	if (A72_MAILBOX_STATUS_OK != response_status)
		return -1;

	if (val)
		*val = return_value;

	return 0;
}

/*  resp: response cmd for async, and 0 means sync */
static int test_set_with_x(struct trav_mbox *mbox, int pri, uint32_t service_id, uint32_t cmd, int n_param,
				 uint32_t comp_id, uint32_t val, uint32_t resp)
{
	uint32_t response_status;
	uint32_t resp_response_status;
	uint32_t valx[2];

	switch (n_param) {
	case 2:
		valx[0] = comp_id;
		valx[1] = val;
		break;
	case 1:
		valx[0] = val;
		break;
	default:
		n_param = 0;
	}

	test_op_nn_with_nn(mbox, pri, service_id, cmd, valx, n_param, &response_status, NULL, 0, resp, resp ? &resp_response_status : NULL, NULL, 0);

	if (!resp && 0 != response_status)
		return -1;

	if (resp && 0 != resp_response_status)
		return -1;

	return 0;
}

static int test_get_with_one(struct trav_mbox *mbox, int pri, uint32_t service_id, uint32_t cmd, uint32_t comp_id,
				 uint32_t resp, uint32_t *val)
{
	uint32_t response_status;
	uint32_t resp_response_status;
	uint32_t ret_val;

	test_op_nn_with_nn(mbox, pri, service_id, cmd, &comp_id, 1, &response_status, NULL, 0, resp, &resp_response_status, &ret_val, 1);
	if (0 != resp_response_status)
		return -1;

	if (val)
		*val = ret_val;

	return 0;
}

/* raw file */
static int test_op_file(struct trav_mbox *mbox, int pri,
			uint32_t file_service_cmd_load, uint32_t file_service_rsp_load,
			uint32_t file_id, const uint8_t *buffer, uint32_t buffer_size)
{
	uint32_t valx[4];
	uint32_t response_status;
	uint32_t resp_response_status;

	/* request SCS to update the A72 coreboot file in SCS QSPI flash */

	valx[0] = file_id;                                   // file id
	valx[1] = (uint32_t)((uint64_t)buffer);              // image address lo
	valx[2] = (uint32_t)(((uint64_t)buffer) >> 32lu);    // image address hi - in the current version of SCS this should be zero
	valx[3] = buffer_size;                               // image size

	test_op_nn_with_nn(mbox, pri, MAILBOX_SERVICE_ID_FILE, file_service_cmd_load, valx, 4, &response_status, NULL, 0, file_service_rsp_load, &resp_response_status, NULL, 0);
	if (0 != resp_response_status)
		return -1;

	return 0;
}

static int test_load_raw_file(struct trav_mbox *mbox, int pri, uint32_t file_id, uint8_t *buffer, uint32_t buffer_size, uint32_t *pfile_size)
{
	uint32_t file_size;

	/* request from SCS for raw file size */

	if (0 != test_get_one_with_one(mbox, pri, MAILBOX_SERVICE_ID_FILE, FILE_SERVICE_CMD_GET_FILE_SIZE, file_id, &file_size))
		return -1;

	pr_info("RAW file size: 0x%08x\n", file_size);

	if (file_size > buffer_size) {
		pr_info("buffer_size is too small : 0x%08x < 0x%08x\n", buffer_size, file_size);
		return -1;
	}

	/* request SCS to load the A72 coreboot file into A72 RAM */
	if (0 != test_op_file(mbox, pri, FILE_SERVICE_CMD_GET_FILE, FILE_SERVICE_RSP_GET_FILE, file_id,
				buffer, buffer_size))
		return -1;

	if (pfile_size)
		*pfile_size = file_size;

    return 0;
}

static int test_update_firmware(struct trav_mbox *mbox, int pri, const uint8_t * firmware_image, uint32_t firmware_image_size, const uint8_t *key, uint32_t key_size, uint32_t flags, unsigned long rx_timeout)
{
	uint32_t valx[7];
	uint32_t response_status;
	uint32_t resp_response_status;

	/* request SCS to update the A72 coreboot file in SCS QSPI flash */

	valx[0] = (uint32_t)((uint64_t)firmware_image);              // image address lo
	valx[1] = (uint32_t)(((uint64_t)firmware_image) >> 32lu);    // image address hi - in the current version of SCS this should be zero
	valx[2] = firmware_image_size;                               // image size
	valx[3] = (uint32_t)((uint64_t)key);                         // key address lo
	valx[4] = (uint32_t)(((uint64_t)key) >> 32lu);               // key address hi - in the current version of SCS this should be zero
	valx[5] = key_size;                                          // key size
	valx[6] = flags;                                              // bank a or bank b

	test_op_nn_with_nn_timeout(mbox, pri, MAILBOX_SERVICE_ID_FILE, FILE_SERVICE_CMD_UPDATE_FIRMWARE, valx, 7, &response_status, NULL, 0, FILE_SERVICE_RSP_UPDATE_FIRMWARE, &resp_response_status, NULL, 0, rx_timeout);
	if (0 != resp_response_status)
		return -1;

	return 0;
}

static int scs_file_service_inited;

static void scs_service_init(struct trav_mbox *mbox)
{
	int ret;

        pr_info("SCS issr_tx_hi=0x%08lx\n", (unsigned long) mbox->issr_tx_hi);
        pr_info("SCS issr_rx_hi=0x%08lx\n", (unsigned long) mbox->issr_rx_hi);
        pr_info("SCS issr_tx_lo=0x%08lx\n", (unsigned long) mbox->issr_tx_lo);
        pr_info("SCS issr_rx_lo=0x%08lx\n", (unsigned long) mbox->issr_rx_lo);

	/* Clear all pending interrupts and masks */
	write32(&mbox->int_rx->intcr, 0xffffffffU);
	write32(&mbox->int_rx->intmr, 0);
	write32(&mbox->int_tx->intcr, 0xffffffffU);
	write32(&mbox->int_tx->intmr, 0);

	ret = test_service_version(mbox, 0, MAILBOX_SERVICE_ID_FILE, FILE_SERVICE_CMD_GET_VERSION, "FILE");
	if (0 != ret) {
		scs_file_service_inited = -1;
		pr_info("SCS File service is not working\n");
		return;
	}

	scs_file_service_inited = 1;
	pr_info("SCS File service is working now\n");
}

static struct trav_mbox *scs_mbox;

static int copy_from_qspi(size_t buffer, uint32_t file_id, size_t in_size, uint32_t *pfile_size)
{
	int ret = -1;

	if (scs_file_service_inited != 1)
		return -1;

	pr_info("copy_from_qspi: %02x [0, 0x%x] to [0x%x, 0x%x] starting...\n",
		 (unsigned int)file_id, (unsigned int)(in_size - 1),
		 (unsigned int)buffer, (unsigned int)(buffer + in_size - 1) );

	ret = test_load_raw_file(scs_mbox, 0, file_id, (uint8_t *) buffer, (uint32_t) in_size, pfile_size);

	pr_info("copy_from_qspi: %02x [0, 0x%x] to [0x%x, 0x%x] Done wth status=%d\n",
		 (unsigned int)file_id, (unsigned int)(in_size - 1),
		 (unsigned int)buffer, (unsigned int)(buffer + in_size - 1),
		 ret);

	return ret;
}

static int save_to_qspi(size_t buffer, uint32_t file_id, size_t in_size)
{
	int ret;

	if (scs_file_service_inited != 1)
		return -1;

	pr_info("save_to_qspi: [0x%x, 0x%x] to 0x%02x [0, 0x%x] starting...\n",
			 (unsigned int)buffer, (unsigned int)(buffer + in_size - 1),
			 (unsigned int)file_id, (unsigned int)(in_size - 1));

	ret = test_op_file(scs_mbox, 0, FILE_SERVICE_CMD_UPDATE_FILE, FILE_SERVICE_RSP_UPDATE_FILE, file_id, (uint8_t *) buffer, (uint32_t) in_size);

	pr_info("save_to_qspi: [0x%x, 0x%x] to 0x%02x [0, 0x%x] Done with status=%d\n",
			 (unsigned int)buffer, (unsigned int)(buffer + in_size - 1),
			 (unsigned int)file_id, (unsigned int)(in_size - 1),
			 ret);

	return ret;
}

static int save_to_qspi_firmware(size_t buffer, size_t in_size, size_t key, size_t key_size, uint32_t flags, unsigned long rx_timeout)
{
	int ret;

	if (scs_file_service_inited != 1)
		return -1;

	pr_info("save_to_qspi: [0x%x, 0x%x] to firmware starting...\n",
			 (unsigned int)buffer, (unsigned int)(buffer + in_size - 1));

	ret = test_update_firmware(scs_mbox, 0, (uint8_t *) buffer, (uint32_t) in_size,
					(uint8_t *) key, (uint32_t) key_size, flags, rx_timeout);

	pr_info("save_to_qspi: [0x%x, 0x%x] to firmware Done with status=%d\n",
			 (unsigned int)buffer, (unsigned int)(buffer + in_size - 1),
			 ret);

	return ret;
}

static int bl1_data_ver = 2;

static int boot_bank = -10;
static int get_boot_bank(void)
{
	/*  bootfrom=ra: 0, bootfrom=rb: 1, other is -1 */
	if (boot_bank >= -1)
		return boot_bank;

	boot_bank = -1;

	if (strstr(saved_command_line, "bootfrom=ra"))
		boot_bank = 0;
	else if (strstr(saved_command_line, "bootfrom=rb"))
		boot_bank = 1;

	return boot_bank;
}

static int bootx_bank = -1;
static int get_bootx_bank(void)
{
	if (bootx_bank >= 0)
		return bootx_bank;

	/* should be the same as bank of qspi, and ufs booting */
	if (get_boot_bank() >= 0)
		bootx_bank = get_boot_bank();
	else if (strstr(saved_command_line, "bootfrom=ua"))
		bootx_bank = 0;
	else if (strstr(saved_command_line, "bootfrom=ub"))
		bootx_bank = 1;

	return bootx_bank;
}

#define BOOT_ENV_START_MiB	 2
#define BOOT_BANK_START_MiB	 4
#define BOOT_BANK_SIZE_MiB	16
#define BOOT_BANK_X_START_MiB	36
#define BOOT_BANK_X_SIZE_MiB	 4
#define BOOT_BANK_P_START_MiB	40
#define BOOT_BANK_P_SIZE_MiB	24
#define SCS_FLASH_TOTAL_SIZE	(64<<20)

#define BOOT_SLOG_START_MiB	 3
#define BOOT_SLOG_SIZE_KiB	 256

static struct mtd_partition scs_partitions[] = {
	{
		.name   = "whole_device",
		.offset = 0,
		.size   = SCS_FLASH_TOTAL_SIZE,
	},
	{
		.name   = "bootenv-r",
		.offset = BOOT_ENV_START_MiB << 20,
		.size   = (BOOT_SLOG_START_MiB - BOOT_ENV_START_MiB) << 20,
	},
	{
		.name   = "boot-r-a",
		.offset = BOOT_BANK_START_MiB << 20,
		.size   = BOOT_BANK_SIZE_MiB << 20,
	},
	{
		.name   = "boot-r-b",
		.offset = (BOOT_BANK_START_MiB + BOOT_BANK_SIZE_MiB) << 20,
		.size   = BOOT_BANK_SIZE_MiB << 20,
	},
	{
		.name   = "boot-x",
		.offset = BOOT_BANK_X_START_MiB << 20,
		.size   = BOOT_BANK_X_SIZE_MiB << 20,
	},
	{
		.name   = "boot-p",
		.offset = BOOT_BANK_P_START_MiB << 20,
		.size   = BOOT_BANK_P_SIZE_MiB << 20,
	},
	{
		.name   = "boot-slog",
		.offset = BOOT_SLOG_START_MiB << 20,
		.size   = BOOT_SLOG_SIZE_KiB << 10,
	}
};

/* reserved in dtsi */
static uint32_t bounce;
static unsigned char *p_bounce;
static uint32_t bounce_size = SCS_FLASH_TOTAL_SIZE;

struct boot_env {
	char signature[16]; /* BOOTTURBOTRAVENV */
	unsigned char boot_bank;
	unsigned char tried_num[2];
	unsigned char successful[2];
	unsigned char boot_from[1];
	unsigned char pad[4096-16-1-2-2-1];
} __attribute__((packed));

/* from 2MiB, 4KiB */
static int read_bootenv(void)
{
	size_t buffer = bounce + (BOOT_ENV_START_MiB<<20);
	size_t size = 4096;

	return copy_from_qspi(buffer, FILE_SERVICE_FILE_A72_CONFIG_ID, size, NULL);
}

static int write_bootenv(void)
{
	size_t buffer = bounce + (BOOT_ENV_START_MiB<<20);
	size_t size = 4096;
	struct boot_env *env = (struct boot_env *)(p_bounce + (BOOT_ENV_START_MiB<<20));

	if (get_boot_bank() < 0) {
		/* only allow update bootfrom */
		char boot_from = env->boot_from[0];

		if (boot_from != 'r' && boot_from != 'u' &&  boot_from != 0x00) {
			pr_info("wrong bootfrom in bootenv-r\n");
			return -1;
		}

		read_bootenv();
		if (env->boot_from[0] == boot_from) {
			pr_info("bootfrom in bootenv-r is not changed\n");
			return -1;
		}
		pr_info("bootfrom in bootenv-r: change to %c\n", (boot_from == 0) ? '0' : boot_from);
		env->boot_from[0] = boot_from;
	}

	/* hard coded */
	if (strncmp(env->signature, "BOOTTURBOTRAVENV", 16)) {
		pr_info("wrong bootenv sig\n");
		return -1;
	}

	if (env->boot_bank > 1) {
		pr_info("wrong bootenv boot_bank\n");
		return -1;
	}

	if (env->tried_num[0] > 3 || env->tried_num[1] > 3) {
		pr_info("wrong bootenv tried num\n");
		return -1;
	}

	if (env->successful[0] > 1 || env->successful[1] > 1) {
		pr_info("wrong bootenv successful\n");
		return -1;
	}

	return save_to_qspi(buffer, FILE_SERVICE_FILE_A72_CONFIG_ID, size);
}

static int read_slog(void)
{
	size_t buffer = bounce + (BOOT_SLOG_START_MiB<<20);
	size_t size = BOOT_SLOG_SIZE_KiB<<10;

	return copy_from_qspi(buffer, FILE_SERVICE_FILE_SLOG_ID, size, NULL);
}

struct offset_size {
	uint32_t offset;
	uint32_t size;
} __attribute__((packed));

struct boot_file_layout {
        char signature[16]; /* BOOT_FILE_LAYOUT */
        char tag;
        unsigned char reserved_1[15];
	struct offset_size pos[7];
	unsigned char pad[4096-16-1-15-7*8];
} __attribute__((packed));

static int read_bank(int bank)
{
	struct boot_file_layout *file_layout;
	uint32_t bank_base = (BOOT_BANK_START_MiB + bank * BOOT_BANK_SIZE_MiB) << 20;
	int ret;

	file_layout = (struct boot_file_layout *)(p_bounce + bank_base);

	/* hard coded */
	strncpy(file_layout->signature, "BOOT_FILE_LAYOUT", 16);
	file_layout->pos[0].offset = 1<<12;
	file_layout->pos[1].offset = 5<<12;
	file_layout->pos[2].offset = 9<<12;
	file_layout->pos[3].offset = 256<<12;

	ret = copy_from_qspi(bounce + bank_base + file_layout->pos[0].offset,
			bank ? FILE_SERVICE_FILE_A72_BOOTBLOCK_B_CRT_ID : FILE_SERVICE_FILE_A72_BOOTBLOCK_A_CRT_ID ,
			16<<10, &file_layout->pos[0].size);
	ret |= copy_from_qspi(bounce + bank_base + file_layout->pos[1].offset,
			bank ? FILE_SERVICE_FILE_A72_COREBOOT_B_CRT_ID : FILE_SERVICE_FILE_A72_COREBOOT_A_CRT_ID ,
			16<<10, &file_layout->pos[1].size);
	ret |= copy_from_qspi(bounce + bank_base + file_layout->pos[2].offset,
			bank ? FILE_SERVICE_FILE_A72_BOOTBLOCK_B_ID : FILE_SERVICE_FILE_A72_BOOTBLOCK_A_ID ,
			(128<<10)+0x2d8, &file_layout->pos[2].size);
	ret |= copy_from_qspi(bounce + bank_base + file_layout->pos[3].offset,
			bank ? FILE_SERVICE_FILE_A72_COREBOOT_B_ID : FILE_SERVICE_FILE_A72_COREBOOT_A_ID ,
			(12<<20)+0x2d8, &file_layout->pos[3].size);

	return ret;
}

#define FW_RA_FILE_OFFSET (2<<12)

/* Erase/Write flash for boot.rom.fw.ra need about 360s */
#define MAILBOX_UPDATE_RA_RX_TIMEOUT      (msecs_to_jiffies(360000))
static int write_bank_2(int bank)
{
	struct boot_file_layout *file_layout;
	uint32_t bank_base = (BOOT_BANK_START_MiB + bank * BOOT_BANK_SIZE_MiB) << 20;
	int ver = bl1_data_ver;
	int boot_from_bank = (ver > 1) ? get_bootx_bank(): get_boot_bank();
	int ret;

	if (boot_from_bank < 0) {
		pr_info("Need to boot from one recovery to update other recovery bank\n");
		return -1;
	}

	if (bank == boot_from_bank) {
		pr_info("only can update other recovery bank\n");
		return -1;
	}

	file_layout = (struct boot_file_layout *)(p_bounce + bank_base);

	/* hard coded */
	if (strncmp(file_layout->signature, "BOOT_FILE_LAYOUT", 16)) {
		pr_info("wrong file_layout sig\n");
		return -1;
	}

	if ((file_layout->pos[0].offset != FW_RA_FILE_OFFSET && file_layout->tag != 'r') ||
	    file_layout->pos[0].size > (BOOT_BANK_SIZE_MiB<<20) - file_layout->pos[0].offset) {
		pr_info("wrong file_layout firmware offset/size\n");
		return -1;
	}

	/* last 8 KiB for key */
	if (file_layout->pos[1].size != 0 &&  (file_layout->pos[1].offset != (4094<<12) ||
	    file_layout->pos[1].size > ((BOOT_BANK_SIZE_MiB<<20) - file_layout->pos[1].offset))) {
		pr_info("wrong file_layout key offset/size\n");
		return -1;
	}

	/*
	 * flags:
	 * 1 – only BANK A dest are updated (the firmware package must not contain any BANK B files)
	 * 2 – only BANK B dest are updated (the firmware package must not contain any BANK B files,
	 *	 BANK A files will be written as BANK B dest)
	 */
	ret = save_to_qspi_firmware(bounce + bank_base + file_layout->pos[0].offset,
					file_layout->pos[0].size,
					file_layout->pos[1].size ? (bounce + bank_base + file_layout->pos[1].offset) : 0,
					file_layout->pos[1].size, bank + 1, MAILBOX_UPDATE_RA_RX_TIMEOUT);

	return ret;
}

static int read_bank_x(void)
{
	struct boot_file_layout *file_layout;
	uint32_t bank_base = BOOT_BANK_X_START_MiB << 20;
	int ret;

	file_layout = (struct boot_file_layout *)(p_bounce + bank_base);

	/* hard coded */
	strncpy(file_layout->signature, "BOOT_FILE_LAYOUT", 16);
	file_layout->pos[0].offset = 1<<12;
	file_layout->pos[1].offset = 5<<12;
	file_layout->pos[2].offset = 9<<12;
	file_layout->pos[3].offset = 16<<12;
	file_layout->pos[4].offset = 144<<12;
	file_layout->pos[5].offset = 272<<12;
	file_layout->pos[6].offset = 512<<12;

	ret = copy_from_qspi(bounce + bank_base + file_layout->pos[0].offset,
			FILE_SERVICE_FILE_SCS_BL2_CRT_ID, 16<<10, &file_layout->pos[0].size);
	ret |= copy_from_qspi(bounce + bank_base + file_layout->pos[1].offset,
			FILE_SERVICE_FILE_SMS_BL1_CRT_ID, 16<<10, &file_layout->pos[1].size);
	ret |= copy_from_qspi(bounce + bank_base + file_layout->pos[2].offset,
			FILE_SERVICE_FILE_A72_BL1_CRT_ID, 16<<10, &file_layout->pos[2].size);
	ret |= copy_from_qspi(bounce + bank_base + file_layout->pos[3].offset,
			FILE_SERVICE_FILE_SCS_BL2_ID, (144-16)<<12, &file_layout->pos[3].size);
	ret |= copy_from_qspi(bounce + bank_base + file_layout->pos[4].offset,
			FILE_SERVICE_FILE_SMS_BL1_ID, (272-144)<<12, &file_layout->pos[4].size);
	ret |= copy_from_qspi(bounce + bank_base + file_layout->pos[5].offset,
			FILE_SERVICE_FILE_A72_BL1_ID, (512-272)<<12, &file_layout->pos[5].size);
	ret |= copy_from_qspi(bounce + bank_base + file_layout->pos[6].offset,
			FILE_SERVICE_FILE_SCS_BL1_ID, (1024-512)<<12, &file_layout->pos[6].size);

	return ret;
}

#define FW_XA_FILE_OFFSET (4<<12)

/* Erase/Write flash for boot.rom.fw.xa need about 30s */
#define MAILBOX_UPDATE_XA_RX_TIMEOUT      (msecs_to_jiffies(30000))
static int write_bank_x2(void)
{
	struct boot_file_layout *file_layout;
	uint32_t bank_base = BOOT_BANK_X_START_MiB << 20;
	int ret;
	int to_bank = 0;
	int flags = 0;
	int ver = bl1_data_ver;

	file_layout = (struct boot_file_layout *)(p_bounce + bank_base);

	/* hard coded */
	if (strncmp(file_layout->signature, "BOOT_FILE_LAYOUT", 16)) {
		pr_info("wrong file_layout sig\n");
		return -1;
	}

	if ((file_layout->pos[0].offset != FW_XA_FILE_OFFSET && file_layout->tag != 'x') ||
	    file_layout->pos[0].size > ((BOOT_BANK_X_SIZE_MiB<<20) - file_layout->pos[0].offset)) {
		pr_info("wrong file_layout firmware offset/size\n");
		return -1;
	}

	/* last 8 KiB for key */
	if (file_layout->pos[1].size != 0 &&  ( file_layout->pos[1].offset != (1022<<12) ||
	    file_layout->pos[1].size > ((BOOT_BANK_X_SIZE_MiB<<20) - file_layout->pos[1].offset))) {
		pr_info("wrong file_layout key offset/size\n");
		return -1;
	}

	/*
	 * flags:
	 * 0 – default behavior
	 * 1 – only BANK A dest are updated (the firmware package must not contain any BANK B files)
	 * 2 – only BANK B dest are updated (the firmware package must not contain any BANK B files,
	 *	 BANK A files will be written as BANK B dest)
	 */
	if (ver > 1) {
		int bootx_bank = get_bootx_bank();

		if (bootx_bank < 0) {
			pr_info("unknown bootx_bank\n");
			return bootx_bank;
		}
		to_bank = 1 - bootx_bank; /* other bootx bank */
		flags = to_bank + 1;
	}

	ret = save_to_qspi_firmware(bounce + bank_base + file_layout->pos[0].offset,
					file_layout->pos[0].size,
					file_layout->pos[1].size ? (bounce + bank_base + file_layout->pos[1].offset) : 0,
					file_layout->pos[1].size, flags, MAILBOX_UPDATE_XA_RX_TIMEOUT);

	return ret;
}

/* Device write buf size in Bytes (Default: 512) */
static unsigned long writebuf_size = 512;
#define SCS_FLASH_ERASE_SIZE 512

/* only one file in scs */
static struct mtd_info *mtd_info;

static int check_offs_len(struct mtd_info *mtd, loff_t ofs, uint64_t len)
{
	int ret = 0;

	/* Start address must align on block boundary */
	if (mtd_mod_by_eb(ofs, mtd)) {
		pr_debug("%s: unaligned address\n", __func__);
		ret = -EINVAL;
	}

	/* Length must align on block boundary */
	if (mtd_mod_by_eb(len, mtd)) {
		pr_debug("%s: length not block aligned\n", __func__);
		ret = -EINVAL;
	}

	return ret;
}

static int scs_flash_erase(struct mtd_info *mtd, struct erase_info *instr)
{
	if (check_offs_len(mtd, instr->addr, instr->len))
		return -EINVAL;

#if 0
	memset((char *)p_bounce + instr->addr, 0xff, instr->len);
#endif
	instr->state = MTD_ERASE_DONE;
	mtd_erase_callback(instr);
	return 0;
}


static int scs_flash_read(struct mtd_info *mtd, loff_t from, size_t len,
		size_t *retlen, u_char *buf)
{
	loff_t start;

	mutex_lock(&scs_mbox->flash_mtx);

	start = BOOT_ENV_START_MiB << 20;
	if (from <= start && (from + len) > start)
		read_bootenv();

	start = BOOT_SLOG_START_MiB << 20;
	if (from <= start && (from + len) > start)
		read_slog();

	start = BOOT_BANK_START_MiB << 20;
	if (from <= start && (from + len) > start)
		read_bank(0);

	start = (BOOT_BANK_START_MiB + BOOT_BANK_SIZE_MiB) << 20;
	if (from <= start && (from + len) > start)
		read_bank(1);

	start = BOOT_BANK_X_START_MiB << 20;
	if (from <= start && (from + len) > start)
		read_bank_x();

	memcpy(buf, p_bounce + from, len);

	mutex_unlock(&scs_mbox->flash_mtx);

	*retlen = len;
	return 0;
}

static int scs_flash_write(struct mtd_info *mtd, loff_t to, size_t len,
		size_t *retlen, const u_char *buf)
{
	loff_t end;

	mutex_lock(&scs_mbox->flash_mtx);

	memcpy((char *)p_bounce + to, buf, len);

	end = (BOOT_ENV_START_MiB << 20) + 4095;
	if (to <= end && (to + len) > end)
		write_bootenv();

	end = ((BOOT_BANK_START_MiB + BOOT_BANK_SIZE_MiB) << 20) - 1;
	if (to <= end && (to + len) > end)
		write_bank_2(0);

	end = ((BOOT_BANK_START_MiB + 2 * BOOT_BANK_SIZE_MiB) << 20) - 1;
	if (to <= end && (to + len) > end)
		write_bank_2(1);

	end = ((BOOT_BANK_X_START_MiB + BOOT_BANK_X_SIZE_MiB) << 20) - 1;
	if (to <= end && (to + len) > end)
		write_bank_x2();

	mutex_unlock(&scs_mbox->flash_mtx);

	*retlen = len;
	return 0;
}

#define DRIVER_NAME     "trav-mailbox-scs"

/* sysfs */

/* num need to be less 16 */
#define MBOX_GENEALOGY_SHOW(value, pri, service_id, cmd, num)					\
static ssize_t value##_show(struct device *dev, struct device_attribute *attr, char *buf)	\
{												\
	struct trav_mbox *mbox = dev_get_drvdata(dev);						\
	uint32_t val[4];									\
	int nn = round_up(num, 4)/4;								\
	uint32_t cmd_status;									\
												\
	test_op_nn_with_nn(mbox, pri, service_id, cmd, NULL, 0,					\
				&cmd_status, val, nn, 0, NULL, NULL, 0);			\
												\
	if (cmd_status != 0)									\
		return 0;									\
												\
	return snprintf(buf, num+1, "%s\n", (char *)val);					\
}

/* num need to be <= 48 */
#define MBOX_GENERIC_STORE(value, pri, service_id, cmd_set, resp, num)				\
static ssize_t value##_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) \
{												\
	struct trav_mbox *mbox = dev_get_drvdata(dev);						\
	int ret = count;									\
	uint32_t val[NN_MAX];									\
	int nn = round_up(num, sizeof(uint32_t))/sizeof(uint32_t);				\
	uint32_t cmd_status, resp_status;							\
	uint32_t dest_size = sizeof(uint32_t) * nn;						\
												\
	if (count > 0 && buf[count - 1] == 0x0a)						\
		count--;									\
	if (count > dest_size)									\
		count = dest_size;								\
												\
	memcpy(val, buf, count);								\
												\
	if (count < dest_size)									\
		memset((unsigned char *)val + count, ' ', dest_size - count);			\
												\
	test_op_nn_with_nn(mbox, pri, service_id, cmd_set, val, nn,				\
				&cmd_status, NULL, 0, resp, &resp_status, NULL, 0);		\
												\
	if (resp_status != 0)									\
		return (int32_t) resp_status;							\
												\
	return ret;										\
}

#define MBOX_GENEALOGY_ATTR_SHOW_STORE(value, pri, service_id, cmd, service_id_set, cmd_set, resp_set, num) \
MBOX_GENEALOGY_SHOW(value, pri, service_id, cmd, num)						\
MBOX_GENERIC_STORE(value, pri, service_id_set, cmd_set, resp_set, num)			\
static DEVICE_ATTR(value, S_IRUGO | S_IWUSR, value##_show, value##_store);

MBOX_GENEALOGY_ATTR_SHOW_STORE(genealogy_pcba_version, 0, MAILBOX_SERVICE_ID_OTP, OTP_SERVICE_CMD_GET_GENEALOGY_PCBA_VERSION, MAILBOX_SERVICE_ID_PROVISIONING, PROVISIONING_SERVICE_CMD_SET_PCBA_VERSION, PROVISIONING_SERVICE_RSP_SET_PCBA_VERSION, 12);
MBOX_GENEALOGY_ATTR_SHOW_STORE(genealogy_pcba_serial,  0, MAILBOX_SERVICE_ID_OTP, OTP_SERVICE_CMD_GET_GENEALOGY_PCBA_SERIAL,  MAILBOX_SERVICE_ID_PROVISIONING, PROVISIONING_SERVICE_CMD_SET_PCBA_SERIAL,  PROVISIONING_SERVICE_RSP_SET_PCBA_SERIAL,  14);
MBOX_GENEALOGY_ATTR_SHOW_STORE(genealogy_tla_version,  0, MAILBOX_SERVICE_ID_OTP, OTP_SERVICE_CMD_GET_GENEALOGY_TLA_VERSION,  MAILBOX_SERVICE_ID_PROVISIONING, PROVISIONING_SERVICE_CMD_SET_TLA_VERSION,  PROVISIONING_SERVICE_RSP_SET_TLA_VERSION,  12);
MBOX_GENEALOGY_ATTR_SHOW_STORE(genealogy_tla_serial,   0, MAILBOX_SERVICE_ID_OTP, OTP_SERVICE_CMD_GET_GENEALOGY_TLA_SERIAL,   MAILBOX_SERVICE_ID_PROVISIONING, PROVISIONING_SERVICE_CMD_SET_TLA_SERIAL,   PROVISIONING_SERVICE_RSP_SET_TLA_SERIAL,   14);

#define MBOX_SLOG_ATTR_STORE(value, pri, service_id_set, cmd_set, resp_set)			\
MBOX_GENERIC_STORE(value, pri, service_id_set, cmd_set, resp_set, (sizeof(uint32_t) * NN_MAX))	\
static DEVICE_ATTR(value, S_IWUSR, NULL, value##_store);

MBOX_SLOG_ATTR_STORE(slog_log_message, 0, MAILBOX_SERVICE_ID_SLOG, SLOG_SERVICE_CMD_LOG_MESSAGE, SLOG_SERVICE_RSP_LOG_MESSAGE);

static uint32_t ret_v(uint32_t val)
{
	return val;
}

#define MBOX_CACHED_SHOW(power, pri, service_id, cmd, comp_id, resp, from_str, v)		\
static ssize_t power##_show(struct device *dev, struct device_attribute *attr, char *buf)	\
{												\
	struct trav_mbox *mbox = dev_get_drvdata(dev);						\
	static uint32_t val = 0;								\
												\
	if (!val) {										\
		if (resp)									\
			test_get_with_one(mbox, pri, service_id, cmd, comp_id, resp, &val);	\
		else										\
			test_get_one_with_one(mbox, pri, service_id, cmd, comp_id, &val);	\
	}											\
												\
	return snprintf(buf, PAGE_SIZE, from_str, v(val));					\
}

#define MBOX_SHOW(power, pri, service_id, cmd, comp_id, resp, from_str, v)			\
static ssize_t power##_show(struct device *dev, struct device_attribute *attr, char *buf)	\
{												\
	struct trav_mbox *mbox = dev_get_drvdata(dev);						\
	uint32_t val = 0;									\
												\
	if (resp)										\
		test_get_with_one(mbox, pri, service_id, cmd, comp_id, resp, &val);		\
	else											\
		test_get_one_with_one(mbox, pri, service_id, cmd, comp_id, &val);		\
												\
	return snprintf(buf, PAGE_SIZE, from_str, v(val));					\
}

#define MBOX_STORE(power, pri, service_id, cmd_set, comp_id, resp_set, set_x, v)	\
static ssize_t power##_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) \
{												\
	struct trav_mbox *mbox = dev_get_drvdata(dev);						\
	int ret;										\
	uint32_t val;										\
												\
	ret = kstrtou32(buf, 0, &val);								\
	if (ret)										\
		return ret;									\
	test_set_with_x(mbox, pri, service_id, cmd_set, set_x, comp_id, v(val), resp_set);	\
												\
	return count;										\
}

#define MBOX_CACHED_ATTR_SHOW_V(power, pri, service_id, cmd, comp_id, resp, from_str, from_l16)	\
MBOX_CACHED_SHOW(power, pri, service_id, cmd, comp_id, resp, from_str, from_l16)		\
static DEVICE_ATTR(power, 0444, power##_show, NULL)

#define MBOX_CACHED_ATTR_SHOW(power, pri, service_id, cmd, comp_id, resp)			\
MBOX_CACHED_ATTR_SHOW_V(power, pri, service_id, cmd, comp_id, resp, "%d\n", ret_v)

#define MBOX_CACHED_ATTR_SHOW_STORE(power, pri, service_id, cmd_get, cmd_set, comp_id, resp, resp_set, set_x) \
MBOX_CACHED_SHOW(power, pri, service_id, cmd_get, comp_id, resp, "%d\n", ret_v)			\
MBOX_STORE(power, pri, service_id, cmd_set, comp_id, resp_set, set_x, ret_v)			\
static DEVICE_ATTR(power, 0600, power##_show, power##_store)

#define MBOX_ATTR_SHOW_V(power, pri, service_id, cmd, comp_id, resp, from_str, from_l16)	\
MBOX_SHOW(power, pri, service_id, cmd, comp_id, resp, from_str, from_l16)			\
static DEVICE_ATTR(power, 0444, power##_show, NULL)

#define MBOX_ATTR_SHOW(power, pri, service_id, cmd, comp_id, resp)				\
MBOX_ATTR_SHOW_V(power, pri, service_id, cmd, comp_id, resp, "%d\n", ret_v)

#define MBOX_ATTR_STORE(power, pri, service_id, cmd_set, comp_id, resp_set, set_x)		\
MBOX_STORE(power, pri, service_id, cmd_set, comp_id, resp_set, set_x, ret_v)			\
static DEVICE_ATTR(power, 0200, NULL, power##_store)

#define MBOX_ATTR_SHOW_STORE(power, pri, service_id, cmd_get, cmd_set, comp_id, resp, resp_set, set_x)	\
MBOX_SHOW(power, pri, service_id, cmd_get, comp_id, resp, "%d\n", ret_v)				\
MBOX_STORE(power, pri, service_id, cmd_set, comp_id, resp_set, set_x, ret_v)				\
static DEVICE_ATTR(power, 0600, power##_show, power##_store)

MBOX_ATTR_STORE(bootx_forced_recovery_mode_set, 0, MAILBOX_SERVICE_ID_BOOT, BOOT_SERVICE_CMD_FORCED_RECOVERY_MODE_SET, 0, 0, 0);
MBOX_ATTR_STORE(bootx_recovery_mode_clear, 0, MAILBOX_SERVICE_ID_BOOT, BOOT_SERVICE_CMD_RECOVERY_MODE_CLEAR, 0, 0, 0);
MBOX_ATTR_STORE(bootx_bank_switch, 0, MAILBOX_SERVICE_ID_BOOT, BOOT_SERVICE_CMD_BOOT_BANK_SWITCH, 0, 0, 1);
MBOX_ATTR_STORE(bootx_a72_completed_ok, 0, MAILBOX_SERVICE_ID_BOOT, BOOT_SERVICE_CMD_A72_COMPLETED_OK, 0, 0, 0);
MBOX_ATTR_SHOW(bootx_need_reset, 0, MAILBOX_SERVICE_ID_BOOT, BOOT_SERVICE_CMD_GET_NEED_RESET, 0, 0);

MBOX_ATTR_STORE(slog_reset_log, 0, MAILBOX_SERVICE_ID_SLOG, SLOG_SERVICE_CMD_RESET_LOG, 0, SLOG_SERVICE_RSP_RESET_LOG, 1);

MBOX_ATTR_STORE(bootx_unlock_token_clear, 0, MAILBOX_SERVICE_ID_BOOT, BOOT_SERVICE_CMD_UNLOCK_TOKEN_CLEAR, 0, 0, 0);

MBOX_CACHED_ATTR_SHOW(bootx_otp_state, 0, MAILBOX_SERVICE_ID_BOOT, BOOT_SERVICE_CMD_GET_OTP_STATE, 0, 0);
MBOX_CACHED_ATTR_SHOW(bootx_scsbl2_crt_usage_restrictions, 0, MAILBOX_SERVICE_ID_BOOT, BOOT_SERVICE_CMD_GET_SCSBL2_CRT_USAGE_RESTRICTIONS, 0, 0);
MBOX_CACHED_ATTR_SHOW(bootx_sms_crt_usage_restrictions, 0, MAILBOX_SERVICE_ID_BOOT, BOOT_SERVICE_CMD_GET_SMS_CRT_USAGE_RESTRICTIONS, 0, 0);
MBOX_CACHED_ATTR_SHOW(bootx_a72bl1_crt_usage_restrictions, 0, MAILBOX_SERVICE_ID_BOOT, BOOT_SERVICE_CMD_GET_A72BL1_CRT_USAGE_RESTRICTIONS, 0, 0);
MBOX_CACHED_ATTR_SHOW(bootblock_crt_usage_restrictions, 0, MAILBOX_SERVICE_ID_BOOT, BOOT_SERVICE_CMD_GET_BOOTBLOCK_CRT_USAGE_RESTRICTIONS, 0, 0);
MBOX_CACHED_ATTR_SHOW(coreboot_crt_usage_restrictions, 0, MAILBOX_SERVICE_ID_BOOT, BOOT_SERVICE_CMD_GET_COREBOOT_CRT_USAGE_RESTRICTIONS, 0, 0);

MBOX_ATTR_SHOW_STORE(bootx_failed_count_a, 0, MAILBOX_SERVICE_ID_BOOT, BOOT_SERVICE_CMD_GET_FAILED_COUNT, BOOT_SERVICE_CMD_SET_FAILED_COUNT, 0, 0, BOOT_SERVICE_RSP_SET_FAILED_COUNT, 2);
MBOX_ATTR_SHOW_STORE(bootx_failed_count_b, 0, MAILBOX_SERVICE_ID_BOOT, BOOT_SERVICE_CMD_GET_FAILED_COUNT, BOOT_SERVICE_CMD_SET_FAILED_COUNT, 1, 0, BOOT_SERVICE_RSP_SET_FAILED_COUNT, 2);

MBOX_CACHED_ATTR_SHOW(bootx_crl_rollback_counter, 0, MAILBOX_SERVICE_ID_OTP, OTP_SERVICE_CMD_GET_MONOTONIC_COUNTER, OTP_MONOTONIC_COUNTER_CRL, 0);
MBOX_CACHED_ATTR_SHOW(bootx_bl1_rollback_counter, 0, MAILBOX_SERVICE_ID_OTP, OTP_SERVICE_CMD_GET_MONOTONIC_COUNTER, OTP_MONOTONIC_COUNTER_BL1, 0);
MBOX_CACHED_ATTR_SHOW(bootx_bl2_rollback_counter, 0, MAILBOX_SERVICE_ID_OTP, OTP_SERVICE_CMD_GET_MONOTONIC_COUNTER, OTP_MONOTONIC_COUNTER_BL2, 0);
MBOX_CACHED_ATTR_SHOW(bootx_sms_rollback_counter, 0, MAILBOX_SERVICE_ID_OTP, OTP_SERVICE_CMD_GET_MONOTONIC_COUNTER, OTP_MONOTONIC_COUNTER_SMS, 0);
MBOX_CACHED_ATTR_SHOW(bootx_a72_rollback_counter, 0, MAILBOX_SERVICE_ID_OTP, OTP_SERVICE_CMD_GET_MONOTONIC_COUNTER, OTP_MONOTONIC_COUNTER_A72, 0);
MBOX_CACHED_ATTR_SHOW(bootblock_rollback_counter, 0, MAILBOX_SERVICE_ID_OTP, OTP_SERVICE_CMD_GET_MONOTONIC_COUNTER, OTP_MONOTONIC_COUNTER_BOOTBLOCK, 0);
MBOX_CACHED_ATTR_SHOW(coreboot_rollback_counter, 0, MAILBOX_SERVICE_ID_OTP, OTP_SERVICE_CMD_GET_MONOTONIC_COUNTER, OTP_MONOTONIC_COUNTER_COREBOOT, 0);

MBOX_CACHED_ATTR_SHOW(genealogy_is_jtag_locked, 0, MAILBOX_SERVICE_ID_OTP, OTP_SERVICE_CMD_GET_GENEALOGY_IS_JTAG_LOCKED, 0, 0);
MBOX_CACHED_ATTR_SHOW(genealogy_is_duk_blank, 0, MAILBOX_SERVICE_ID_OTP, OTP_SERVICE_CMD_GET_GENEALOGY_IS_DUK_BLANK, 0, 0);
MBOX_CACHED_ATTR_SHOW(genealogy_is_root_ca_blank, 0, MAILBOX_SERVICE_ID_OTP, OTP_SERVICE_CMD_GET_GENEALOGY_IS_ROOT_CA_BLANK, 0, 0);
MBOX_CACHED_ATTR_SHOW(genealogy_is_comm_ca_blank, 0, MAILBOX_SERVICE_ID_OTP, OTP_SERVICE_CMD_GET_GENEALOGY_IS_COMM_CA_BLANK, 0, 0);
MBOX_CACHED_ATTR_SHOW(genealogy_is_duk_locked, 0, MAILBOX_SERVICE_ID_OTP, OTP_SERVICE_CMD_GET_GENEALOGY_IS_DUK_LOCKED, 0, 0);
MBOX_CACHED_ATTR_SHOW(genealogy_is_root_ca_locked, 0, MAILBOX_SERVICE_ID_OTP, OTP_SERVICE_CMD_GET_GENEALOGY_IS_ROOT_CA_LOCKED, 0, 0);
MBOX_CACHED_ATTR_SHOW(genealogy_is_other_soc_fused, 0, MAILBOX_SERVICE_ID_OTP, OTP_SERVICE_CMD_GET_GENEALOGY_IS_OTHER_SOC_FUSED, 0, 0);

MBOX_CACHED_ATTR_SHOW_STORE(factory_gated, 0, MAILBOX_SERVICE_ID_OTP, OTP_SERVICE_CMD_GET_FACTORY_GATED, OTP_SERVICE_CMD_FUSE_FACTORY_GATED, 0, 0, 0, 1);

static DEVICE_INT_ATTR(bootx_bl1_data_ver,	S_IRUGO          , bl1_data_ver);

static uint32_t prov_cmd_arg[12];
static uint32_t prov_cmd;
static uint32_t prov_cmd_status;
static uint32_t prov_cmd_return_arg_1;
static uint32_t prov_resp;
static uint32_t prov_resp_arg_1;
static uint32_t prov_resp_status;

static DEVICE_UINT_ATTR(prov_cmd_arg_1,		S_IRUGO | S_IWUSR, prov_cmd_arg[0]);
static DEVICE_UINT_ATTR(prov_cmd_arg_2,		S_IRUGO | S_IWUSR, prov_cmd_arg[1]);
static DEVICE_UINT_ATTR(prov_cmd_arg_3,		S_IRUGO | S_IWUSR, prov_cmd_arg[2]);
static DEVICE_UINT_ATTR(prov_cmd_arg_4,		S_IRUGO | S_IWUSR, prov_cmd_arg[3]);
static DEVICE_UINT_ATTR(prov_cmd_arg_5,		S_IRUGO | S_IWUSR, prov_cmd_arg[4]);
static DEVICE_UINT_ATTR(prov_cmd_arg_6,		S_IRUGO | S_IWUSR, prov_cmd_arg[5]);
static DEVICE_UINT_ATTR(prov_cmd_arg_7,		S_IRUGO | S_IWUSR, prov_cmd_arg[6]);
static DEVICE_UINT_ATTR(prov_cmd_arg_8,		S_IRUGO | S_IWUSR, prov_cmd_arg[7]);
static DEVICE_UINT_ATTR(prov_cmd_arg_9,		S_IRUGO | S_IWUSR, prov_cmd_arg[8]);
static DEVICE_UINT_ATTR(prov_cmd_arg_10,	S_IRUGO | S_IWUSR, prov_cmd_arg[9]);
static DEVICE_UINT_ATTR(prov_cmd_arg_11,	S_IRUGO | S_IWUSR, prov_cmd_arg[10]);
static DEVICE_UINT_ATTR(prov_cmd_arg_12,	S_IRUGO | S_IWUSR, prov_cmd_arg[11]);
static DEVICE_UINT_ATTR(prov_cmd,		S_IRUGO | S_IWUSR, prov_cmd);
static DEVICE_UINT_ATTR(prov_cmd_status,	S_IRUGO          , prov_cmd_status);
static DEVICE_UINT_ATTR(prov_cmd_return_arg_1,	S_IRUGO          , prov_cmd_return_arg_1);
static DEVICE_UINT_ATTR(prov_resp,		S_IRUGO | S_IWUSR, prov_resp);
static DEVICE_UINT_ATTR(prov_resp_arg_1,	S_IRUGO          , prov_resp_arg_1);
static DEVICE_UINT_ATTR(prov_resp_status,	S_IRUGO          , prov_resp_status);

/* Erase/Write flash need about 600s for provisioning firmware update */
#define MAILBOX_UPDATE_PROV_RX_TIMEOUT      (msecs_to_jiffies(600000))
static ssize_t prov_cmd_start_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct trav_mbox *mbox = dev_get_drvdata(dev);
	int ret = count;
	uint32_t val;

	sscanf(buf, "%x", &val);

	mutex_lock(&mbox->flash_mtx);
	test_op_nn_with_nn_timeout(mbox, 0, MAILBOX_SERVICE_ID_PROVISIONING, prov_cmd, prov_cmd_arg, 12,
				&prov_cmd_status, &prov_cmd_return_arg_1, 1,
				prov_resp, &prov_resp_status, &prov_resp_arg_1, 1, MAILBOX_UPDATE_PROV_RX_TIMEOUT);
	mutex_unlock(&mbox->flash_mtx);

	return ret;
}

static DEVICE_ATTR_WO(prov_cmd_start);

static ssize_t fmp_wrapped_key_show(struct device *dev,
	struct device_attribute *attr, 
	char *buf)
{
	struct trav_mbox *mbox = dev_get_drvdata(dev);
	uint32_t args[3];
	uint32_t rets[3];
	uint32_t response_status;
	uint8_t *p_shared_buffer =
		(p_bounce + (BOOT_BANK_P_START_MiB<<20));
	uint32_t shared_buffer_physical_addr =
		bounce + (BOOT_BANK_P_START_MiB << 20);
	uint32_t shared_buffer_size = BOOT_BANK_P_SIZE_MiB << 20;
	uint32_t key_wrapped_key_size;
	ssize_t ret_val;

	/* buffer address lo */
	args[0] = (uint32_t)((uint64_t)shared_buffer_physical_addr);
	/* buffer address hi - should be 0 in the current version of SCS */
	args[1] = (uint32_t)(((uint64_t)shared_buffer_physical_addr) >> 32lu);
	/* buffer size */
	args[2] = shared_buffer_size;

	memset(rets, 0, sizeof(rets));

	/* Acquire the flash_mtx mutex so that another process doesn't */
	/* use the boot-p buffer while its being used here. */
	mutex_lock(&scs_mbox->flash_mtx);

	memset(p_shared_buffer, 0, shared_buffer_size);

	test_op_nn_with_nn(mbox,
			0,
			MAILBOX_SERVICE_ID_OTP,
			OTP_SERVICE_CMD_GET_KEY_WRAPPED_FMP_KEY,
			args,
			3,
			&response_status,
			rets,
			3,
			0,
			NULL,
			NULL,
			0);

	if (response_status != 0) {
		ret_val = -EINVAL;
		goto cleanup;
	}

	key_wrapped_key_size = rets[2];
	if (key_wrapped_key_size > PAGE_SIZE) {
		ret_val = -ENOMEM;
		goto cleanup;
	}

	memcpy(buf, p_shared_buffer, key_wrapped_key_size);

	ret_val = key_wrapped_key_size;

cleanup:

	mutex_unlock(&scs_mbox->flash_mtx);
	return ret_val;
}
static DEVICE_ATTR(fmp_wrapped_key, 0400, fmp_wrapped_key_show, NULL);

/*
 * Sets/gets status code indicating whether FMP is enabled.
 *
 * 0 = disable
 * 1 = enables with OTP key
 * 2 = enabled with SFR key
 */
MBOX_ATTR_SHOW_STORE(fmp_enabled, 0, MAILBOX_SERVICE_ID_BOOT, BOOT_SERVICE_CMD_FMP_GET_ENABLED, BOOT_SERVICE_CMD_FMP_SET_ENABLED, 0, 0, 0, 1);
MBOX_ATTR_STORE(fmp_fuse_otp_key_force_bit, 0, MAILBOX_SERVICE_ID_OTP, OTP_SERVICE_CMD_FUSE_FMP_OTP_KEY_FORCE_BIT, 0, 0, 1);
MBOX_CACHED_ATTR_SHOW(fmp_is_otp_key_force_bit_fused, 0, MAILBOX_SERVICE_ID_OTP, OTP_SERVICE_CMD_IS_FMP_OTP_KEY_FORCE_BIT_FUSED, 0, 0);
MBOX_CACHED_ATTR_SHOW(fmp_is_otp_key_locked_fused, 0, MAILBOX_SERVICE_ID_OTP, OTP_SERVICE_CMD_GET_FMP_KEY_LOCK_BIT, 0, 0);
MBOX_ATTR_STORE(fmp_fuse_otp_key_lock, 0, MAILBOX_SERVICE_ID_OTP, OTP_SERVICE_CMD_FUSE_FMP_KEY_LOCK_BIT, 0, 0, 1);
MBOX_ATTR_STORE(fmp_fuse_key, 0, MAILBOX_SERVICE_ID_OTP, OTP_SERVICE_CMD_FUSE_FMP_KEY, 0, 0, 1);

static struct attribute *mbox_sysfs_attrs[] = {
	&dev_attr_genealogy_pcba_version.attr,
	&dev_attr_genealogy_pcba_serial.attr,
	&dev_attr_genealogy_tla_version.attr,
	&dev_attr_genealogy_tla_serial.attr,
	&dev_attr_slog_log_message.attr,
	&dev_attr_prov_cmd_arg_1.attr.attr,
	&dev_attr_prov_cmd_arg_2.attr.attr,
	&dev_attr_prov_cmd_arg_3.attr.attr,
	&dev_attr_prov_cmd_arg_4.attr.attr,
	&dev_attr_prov_cmd_arg_5.attr.attr,
	&dev_attr_prov_cmd_arg_6.attr.attr,
	&dev_attr_prov_cmd_arg_7.attr.attr,
	&dev_attr_prov_cmd_arg_8.attr.attr,
	&dev_attr_prov_cmd_arg_9.attr.attr,
	&dev_attr_prov_cmd_arg_10.attr.attr,
	&dev_attr_prov_cmd_arg_11.attr.attr,
	&dev_attr_prov_cmd_arg_12.attr.attr,
	&dev_attr_prov_cmd.attr.attr,
	&dev_attr_prov_cmd_status.attr.attr,
	&dev_attr_prov_cmd_return_arg_1.attr.attr,
	&dev_attr_prov_resp.attr.attr,
	&dev_attr_prov_resp_arg_1.attr.attr,
	&dev_attr_prov_resp_status.attr.attr,
	&dev_attr_prov_cmd_start.attr,
	&dev_attr_bootx_forced_recovery_mode_set.attr,
	&dev_attr_bootx_recovery_mode_clear.attr,
	&dev_attr_bootx_bank_switch.attr,
	&dev_attr_bootx_bl1_data_ver.attr.attr,
	&dev_attr_bootx_a72_completed_ok.attr,
	&dev_attr_slog_reset_log.attr,
	&dev_attr_bootx_need_reset.attr,
	&dev_attr_bootx_otp_state.attr,
	&dev_attr_bootx_scsbl2_crt_usage_restrictions.attr,
	&dev_attr_bootx_sms_crt_usage_restrictions.attr,
	&dev_attr_bootx_a72bl1_crt_usage_restrictions.attr,
	&dev_attr_bootblock_crt_usage_restrictions.attr,
	&dev_attr_coreboot_crt_usage_restrictions.attr,
	&dev_attr_bootx_failed_count_a.attr,
	&dev_attr_bootx_failed_count_b.attr,
	&dev_attr_bootx_crl_rollback_counter.attr,
	&dev_attr_bootx_bl1_rollback_counter.attr,
	&dev_attr_bootx_bl2_rollback_counter.attr,
	&dev_attr_bootx_sms_rollback_counter.attr,
	&dev_attr_bootx_a72_rollback_counter.attr,
	&dev_attr_bootblock_rollback_counter.attr,
	&dev_attr_coreboot_rollback_counter.attr,
	&dev_attr_genealogy_is_jtag_locked.attr,
	&dev_attr_genealogy_is_duk_blank.attr,
	&dev_attr_genealogy_is_root_ca_blank.attr,
	&dev_attr_genealogy_is_comm_ca_blank.attr,
	&dev_attr_genealogy_is_duk_locked.attr,
	&dev_attr_genealogy_is_root_ca_locked.attr,
	&dev_attr_genealogy_is_other_soc_fused.attr,
	&dev_attr_fmp_wrapped_key.attr,
	&dev_attr_fmp_enabled.attr,
	&dev_attr_fmp_is_otp_key_force_bit_fused.attr,
	&dev_attr_fmp_fuse_otp_key_force_bit.attr,
	&dev_attr_fmp_fuse_key.attr,
	&dev_attr_fmp_is_otp_key_locked_fused.attr,
	&dev_attr_fmp_fuse_otp_key_lock.attr,
	&dev_attr_bootx_unlock_token_clear.attr,
	&dev_attr_factory_gated.attr,
	NULL,
};

static const struct attribute_group mbox_sysfs_attr_group = {
	.attrs = mbox_sysfs_attrs,
};

static int trav_mbox_scs_remove(struct platform_device *pdev)
{
	struct trav_mbox *mbox = platform_get_drvdata(pdev);

	if (!mbox)
		return -EINVAL;

	if (mbox->sysfs_inited)
		sysfs_remove_group(&pdev->dev.kobj, &mbox_sysfs_attr_group);

	dma_release_from_dev_coherent(&pdev->dev, get_order(bounce_size), p_bounce);
	of_reserved_mem_device_release(&pdev->dev);

	if (mtd_info) {
		mtd_device_unregister(mtd_info);
		kfree(mtd_info);
	}

	return 0;
}

static int scs_flash_init_mtd(const char *name)
{
	struct mtd_info *mtd;

	/* Allocate some memory */
	mtd = kmalloc(sizeof(struct mtd_info), GFP_KERNEL);
	if (!mtd)
		return -ENOMEM;

	memset(mtd, 0, sizeof(*mtd));

	/* Setup the MTD structure */
	mtd->name = name;
	mtd->type = MTD_RAM;
	mtd->flags = MTD_CAP_RAM;
	mtd->size = SCS_FLASH_TOTAL_SIZE;
	mtd->writesize = 1;
	mtd->writebufsize = writebuf_size;
	mtd->erasesize = SCS_FLASH_ERASE_SIZE;

	mtd->owner = THIS_MODULE;
	mtd->_erase = scs_flash_erase;
	mtd->_read = scs_flash_read;
	mtd->_write = scs_flash_write;

	if (mtd_device_register(mtd, scs_partitions, ARRAY_SIZE(scs_partitions))) {
		kfree(mtd_info);
		return -EIO;
	}

	mtd_info = mtd;

	return 0;
}

static int trav_mbox_scs_probe(struct platform_device *pdev)
{
	struct trav_mbox *mbox;
	struct mbox * mbox_regs;
	struct resource *regs;
	dma_addr_t dma_handle;
	void *vaddr;

	int err;

	mbox = devm_kzalloc(&pdev->dev, sizeof(*mbox), GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	mbox->mbox_base = devm_ioremap_resource(&pdev->dev, regs);
	if (IS_ERR(mbox->mbox_base))
		return PTR_ERR(mbox->mbox_base);

	mbox->irq[MBOX_INT_PRIO_LOW] = platform_get_irq(pdev, 0);
	mbox->irq[MBOX_INT_PRIO_HIGH] = platform_get_irq(pdev, 1);

	mbox->dev = &pdev->dev;
	spin_lock_init(&mbox->lock);
	mutex_init(&mbox->mbox_mtx);
	mutex_init(&mbox->flash_mtx);

	init_completion(&mbox->tx_completion[MBOX_INT_PRIO_LOW]);
	init_completion(&mbox->tx_completion[MBOX_INT_PRIO_HIGH]);
	init_completion(&mbox->rx_completion[MBOX_INT_PRIO_LOW]);
	init_completion(&mbox->rx_completion[MBOX_INT_PRIO_HIGH]);

	platform_set_drvdata(pdev, mbox);

	mbox_regs = (struct mbox *)(mbox->mbox_base);

	mbox->int_rx = (struct mbox_int *)(mbox->mbox_base + A72_MAILBOX_TO_A72_INT - A72_MAILBOX);
	mbox->int_tx = (struct mbox_int *)(mbox->mbox_base + A72_MAILBOX_TO_SCS_INT - A72_MAILBOX);

	mbox->issr_tx_lo = &(mbox_regs->issr_lo1);
	mbox->issr_rx_lo = &(mbox_regs->issr_lo0);

	mbox->issr_tx_hi = &(mbox_regs->issr_hi1);
	mbox->issr_rx_hi = &(mbox_regs->issr_hi0);

        /* Add device's sysfs */
	err = sysfs_create_group(&pdev->dev.kobj, &mbox_sysfs_attr_group);
	if (err)
		dev_err(mbox->dev, "error creating sysfs entries\n");
	else
		mbox->sysfs_inited = 1;

	err = request_irq(mbox->irq[MBOX_INT_PRIO_LOW], mailbox_scs_isr_lo, 0,
			  dev_name(&pdev->dev), mbox);
	if (err < 0) {
                dev_err(&pdev->dev, "failed requesting irq, irq = %d\n",
                                 mbox->irq[MBOX_INT_PRIO_LOW]);
		goto out_sysfs_remove;
        }

        err = request_irq(mbox->irq[MBOX_INT_PRIO_HIGH], mailbox_scs_isr_hi, 0,
                          dev_name(&pdev->dev), mbox);
        if (err < 0) {
                dev_err(&pdev->dev, "failed requesting irq, irq = %d\n",
                                 mbox->irq[MBOX_INT_PRIO_HIGH]);
                free_irq(mbox->irq[MBOX_INT_PRIO_LOW], mbox);
		goto out_sysfs_remove;
        }

	scs_service_init(mbox);

	if (scs_file_service_inited != 1) {
		err = -ENODEV;
		goto out_irq_free;
	}

	err = of_reserved_mem_device_init(&pdev->dev);
	if (err)
		goto out_irq_free;

	err = dma_alloc_from_dev_coherent(&pdev->dev, bounce_size, &dma_handle, &vaddr);
	if (err != 1) {
		err = -err;
		goto out_of_release;
	}
	bounce = (uint32_t) dma_handle;
	p_bounce = vaddr;
	pr_info("bounce phys_addr: 0x%0lx\n", (unsigned long)bounce);

	err = scs_flash_init_mtd("scs_flash");
	if (err)
		goto out_dma_release;

	scs_mbox = mbox;

	return 0;

out_dma_release:
	dma_release_from_dev_coherent(&pdev->dev, get_order(bounce_size), p_bounce);
out_of_release:
	of_reserved_mem_device_release(&pdev->dev);
out_irq_free:
	free_irq(mbox->irq[MBOX_INT_PRIO_LOW], mbox);
	free_irq(mbox->irq[MBOX_INT_PRIO_HIGH], mbox);
out_sysfs_remove:
	if (mbox->sysfs_inited)
		sysfs_remove_group(&pdev->dev.kobj, &mbox_sysfs_attr_group);

	return err;
}

static const struct of_device_id trav_mbox_scs_match[] = {
        { .compatible = "turbo,trav-mailbox-scs" },
        { /* Sentinel */ }
};

MODULE_DEVICE_TABLE(of, trav_mbox_scs_match);

static struct platform_driver trav_mbox_scs_driver = {
	.probe  = trav_mbox_scs_probe,
	.remove = trav_mbox_scs_remove,
	.driver = {
		.name   = DRIVER_NAME,
		.of_match_table = trav_mbox_scs_match,
	},
};

module_platform_driver(trav_mbox_scs_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Proxy MTD driver for SCS qspi flash");
