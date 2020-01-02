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

#include <linux/device.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/mfd/core.h>
#include <linux/arm-smccc.h>
#include <linux/mfd/sms-mbox.h>
#include <linux/time.h>

#include <asm/system_misc.h>

#include "../mtd/devices/MBOX.h"
#include "mailbox_A72_SMS_interface.h"

#define read32(v) ioread32(v)
#define write32(addr, v) iowrite32(v, addr)

#define NN_MAX          12
#define RSP_NN_MAX      11

/* number of arguments is 0 */
#define NO_ARGS		0
/* 
 * number of arguments is 6 for storing time and date during
 * get_time and set_time 
 */
#define TIME_REGS_NN	6

enum {
	MBOX_INT_PRIO_LOW,
	MBOX_INT_PRIO_HIGH,
	MBOX_INT_PRIO_MAX,
};

struct trav_mbox {
	struct device *dev;
	void __iomem *mbox_base;
	int irq[MBOX_INT_PRIO_MAX];
	spinlock_t lock;
	struct mutex mbox_mtx;

	struct mbox_issr *issr_tx_lo;
	struct mbox_issr *issr_rx_lo;
	struct mbox_issr *issr_tx_hi;
	struct mbox_issr *issr_rx_hi;
	struct mbox_int *int_rx;
	struct mbox_int *int_tx;

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
	if (pri >= MBOX_INT_PRIO_HIGH) {
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
	if (pri >= MBOX_INT_PRIO_HIGH) {
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

/* to SMS */
#define A72_MAILBOX                 (0x10090000)
#define A72_MAILBOX_TO_A72_INT      (A72_MAILBOX + 0x0008)
#define A72_MAILBOX_TO_SMS_INT      (A72_MAILBOX + 0x001C)
#define A72_MAILBOX_TO_A72_ISSR_LO  (A72_MAILBOX + 0x0080)
#define A72_MAILBOX_TO_SMS_ISSR_LO  (A72_MAILBOX + 0x00C0)
#define A72_MAILBOX_TO_A72_ISSR_HI  (A72_MAILBOX + 0x0100)
#define A72_MAILBOX_TO_SMS_ISSR_HI  (A72_MAILBOX + 0x0140)
#define A72_MAILBOX_TO_A72_SEM_LO   (A72_MAILBOX + 0x0180)
#define A72_MAILBOX_TO_SMS_SEM_LO   (A72_MAILBOX + 0x018C)
#define A72_MAILBOX_TO_A72_SEM_HI   (A72_MAILBOX + 0x0198)
#define A72_MAILBOX_TO_SMS_SEM_HI   (A72_MAILBOX + 0x01A4)

static uint32_t request_id_counter = 0;

#define MAILBOX_TX_TIMEOUT	(msecs_to_jiffies(1000))

static int mailbox_send_request_nn(struct trav_mbox *mbox, int pri, uint32_t service_id, uint32_t request_id, uint32_t argc, const uint32_t *args, uint32_t *response_status, uint32_t *return_value, int return_nn, uint32_t resp, uint32_t *resp_response_status, uint32_t *resp_return_value, int resp_return_nn)
{
	uint32_t n;
	unsigned long timeout;

	struct mbox_issr *issr_tx;

	spin_lock_irq(&mbox->lock);

	if (pri >= MBOX_INT_PRIO_HIGH)
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

	// we timed out while waiting for the SMS to acknowlege the receipt of the request
	if (0 == timeout) {
		pr_info("mailbox_send_request: timeout! request: 0x%08x.\n", request_id);
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

	return 0; // send request succeeded
}

/* pushed from SMS */
static int ap_sms_redundancy_status_pushed;
static int ap_sms_arb_state_pushed;
static int ap_sms_ok_to_start_ap_session_pushed;
static int ap_sms_other_sms_health_pushed;
static int ap_sms_other_fault_state_pushed;

/* for SMS query and push to SMS */
static int ap_ap_status;

static irqreturn_t mailbox_sms_isr(int pri, int irq, void *dev_id)
{
	struct trav_mbox *mbox = (struct trav_mbox *)dev_id;
	unsigned long flags;
	uint32_t recv_service_id;

	struct mbox_issr *issr_tx;
	struct mbox_issr *issr_rx;
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
	pr_debug("[mailbox-sms] %s, %d, pri=%d rx intgr 0x%08x intmr: 0x%08x intcr: 0x%08x intsr: 0x%08x\n", __func__, __LINE__, pri, read32(&mbox->int_rx->intgr), read32(&mbox->int_rx->intmr), read32(&mbox->int_rx->intcr), rx_intsr);

	/* ack */
	if (0 == read32(&issr_tx->issr[ISSR_INDEX_SERVICE]) && ack_mask == (rx_intsr & ack_mask)) {
		int handled = 0;
		pr_debug("[mailbox-sms] %s, %d, pri=%d\n", __func__, __LINE__, pri);

		if (NULL != mbox->response_status) {
			*mbox->response_status = read32(&issr_tx->issr[ISSR_INDEX_STATUS]);
			mbox->response_status = NULL;
			handled = 1;
		}

		if (NULL != mbox->return_value) {
			int n, j;

			/* when 13 is needed, need to use DATA0..DATA12, otherwise frm DATA1 instead */
			if (mbox->return_nn > (NN_MAX + 1))
				mbox->return_nn = (NN_MAX + 1);
			n = (mbox->return_nn == (NN_MAX + 1)) ? 0 : 1;
			for (j = 0; j < mbox->return_nn; j++, n++)
				mbox->return_value[j] = read32(&issr_tx->issr[ISSR_INDEX_DATA0 + n]);
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

		pr_debug("[mailbox-sms] %s, %d, pri=%d\n", __func__, __LINE__, pri);

		/* check if it is response at first */
		if ((response_code & MBOX_COMMAND_RESPONSE_ID) == MBOX_COMMAND_RESPONSE_ID) {
			int handled = 0;

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

			write32(&mbox->int_rx->intcr, rsp_mask);   /* rsp */

			if (!handled) {
				write32(&issr_rx->issr[ISSR_INDEX_SERVICE], 0);
				write32(&mbox->int_tx->intgr, ack_mask);
			}

		} else {
			uint32_t recv_request_id = read32(&issr_rx->issr[ISSR_INDEX_REQUEST]);
			uint32_t cmd = read32(&issr_rx->issr[ISSR_INDEX_DATA0]);
		    uint32_t index = read32(&issr_rx->issr[ISSR_INDEX_DATA1]);
			uint32_t status = 0;

			/*  handle request from SMS ? */
			pr_info("[mailbox-sms] %s, %d, pri=%d\n", __func__, __LINE__, pri);
			pr_info("request_id 0x%08x service_id 0x%08x cmd 0x%08x index 0x%08x\n", recv_request_id, recv_service_id, cmd, index);

			switch (recv_service_id) {
			case MAILBOX_SERVICE_ID_BOOT:
				switch (cmd) {
				case BOOT_SERVICE_CMD_GET_VERSION:
					/* return_value */
					write32(&issr_rx->issr[ISSR_INDEX_DATA1], 1);
					break;
				default:
					status = SMS_MAILBOX_STATUS_INVALID_COMMAND;
				}
				break;
			case MAILBOX_SERVICE_ID_AP:
				switch (cmd) {
				case AP_SERVICE_CMD_REDUNDANCY_STATUS_UPDATE:
					ap_sms_redundancy_status_pushed = index;
					break;
				case AP_SERVICE_CMD_GET_AP_STATUS:
					/* return_value */
					write32(&issr_rx->issr[ISSR_INDEX_DATA1], ap_ap_status);
					break;
				case AP_SERVICE_CMD_ARB_STATE_STATUS_UPDATE:
					ap_sms_arb_state_pushed = index;
					break;
				case AP_SERVICE_CMD_OK_TO_START_AP_SESSION_STATUS_UPDATE:
					ap_sms_ok_to_start_ap_session_pushed = index;
					break;
				case AP_SERVICE_CMD_OTHER_SMS_HEALTH_UPDATE:
					ap_sms_other_sms_health_pushed = index;
					break;
				case AP_SERVICE_CMD_OTHER_FAULT_STATE_UPDATE:
					ap_sms_other_fault_state_pushed = index;
					break;
				default:
					status = SMS_MAILBOX_STATUS_INVALID_COMMAND;
				}
				break;
			default:
				status = SMS_MAILBOX_STATUS_INVALID_REQUEST;
			}

			/* response_status */
			write32(&issr_rx->issr[ISSR_INDEX_STATUS], status);

			write32(&mbox->int_rx->intcr, rsp_mask);
			write32(&issr_rx->issr[ISSR_INDEX_SERVICE], 0);

			write32(&mbox->int_tx->intgr, ack_mask);
		}
	}

	spin_unlock_irqrestore(&mbox->lock, flags);
	return IRQ_HANDLED;
}

static irqreturn_t mailbox_sms_isr_hi(int irq, void *dev_id)
{
	return mailbox_sms_isr(MBOX_INT_PRIO_HIGH, irq, dev_id);
}

static irqreturn_t mailbox_sms_isr_lo(int irq, void *dev_id)
{
	return mailbox_sms_isr(MBOX_INT_PRIO_LOW, irq, dev_id);
}

#define MAILBOX_RX_TIMEOUT	(msecs_to_jiffies(3000))
static int mailbox_wait_for_response(struct trav_mbox *mbox, int pri, uint32_t expected_service_id, uint32_t request_id, uint32_t expected_response_code)
{
	unsigned long timeout;
	unsigned long flags;

	struct mbox_issr *issr_rx;

	timeout = wait_for_completion_timeout(&mbox->rx_completion[pri], MAILBOX_RX_TIMEOUT);

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

static void test_op_nn_with_nn(struct trav_mbox *mbox, int pri, uint32_t service_id,
		uint32_t prov_cmd, uint32_t *prov_cmd_arg, int nn,
		uint32_t *p_prov_cmd_status, uint32_t *p_prov_cmd_return_arg, int return_nn,
		uint32_t prov_resp, uint32_t *p_prov_resp_status, uint32_t *p_prov_resp_arg, int resp_return_nn)
{
	int rv, resp_rv;
	uint32_t request_args[NN_MAX + 1];
	uint32_t response_status;
	uint32_t return_value[NN_MAX + 1];
	uint32_t resp_response_status;
	uint32_t resp_return_value[RSP_NN_MAX];
	int i;

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// request SCS to for prov
	////////////////////////////////////////////////////////////////////////////////////////////////////

	if (nn > NN_MAX)
		nn = NN_MAX;
	if (return_nn > (NN_MAX + 1))
		return_nn = NN_MAX + 1;
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

		if (0 != response_status) {
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
	if (0 != rv || SMS_MAILBOX_STATUS_REQUEST_PENDING != response_status) {
		pr_info("mailbox_send_request(0x%08x) failed with response status 0x%08x and error 0x%08x\n", request_id_counter, response_status, rv);
		goto out;
	}

	// wait for the asynchronous response
	resp_rv = mailbox_wait_for_response(mbox, pri, service_id, request_id_counter, prov_resp);
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


static int test_get_nn(struct trav_mbox *mbox, int pri, uint32_t service_id, uint32_t cmd, uint32_t *val, int nn)
{
	uint32_t response_status;

	test_op_nn_with_nn(mbox, pri, service_id, cmd, NULL, 0, &response_status, val, nn, 0, NULL, NULL, 0);
	if (SMS_MAILBOX_STATUS_OK != response_status)
		return -1;

	return 0;
}

static int test_get_one_with_one(struct trav_mbox *mbox, int pri, uint32_t service_id, uint32_t cmd,
				uint32_t index, uint32_t *val)
{
	uint32_t response_status;
	uint32_t return_value;

	test_op_nn_with_nn(mbox, pri, service_id, cmd, &index, 1, &response_status, &return_value, 1, 0, NULL, NULL, 0);

	if (SMS_MAILBOX_STATUS_OK != response_status)
		return -1;

	if (val)
		*val = return_value;

	return 0;
}

static int test_service_version(struct trav_mbox *mbox, int pri, uint32_t service_id, uint32_t cmd, const char *name)
{
	uint32_t response_status;
	uint32_t return_value;

	test_op_nn_with_nn(mbox, pri, service_id, cmd, NULL, 0, &response_status, &return_value, 1, 0, NULL, NULL, 0);

	if (SMS_MAILBOX_STATUS_OK != response_status)
		return -1;

	pr_info("MAILBOX_SERVICE %s version: %u\n", name, return_value);

	return 0;
}

/* set something ? */
/* x: 0, 1, 2; resp: 0 means sync */
static int test_set_with_x(struct trav_mbox *mbox, int pri, uint32_t service_id, uint32_t cmd, int x,
				 uint32_t comp_id, uint32_t val, uint32_t resp)
{
	uint32_t response_status;
	uint32_t resp_response_status;
	uint32_t valx[2];

	// send the PMIC_SERVICE_CMD_POWER_SET request
	switch (x) {
	case 2:
		valx[0] = comp_id;
		valx[1] = val;
		break;
	case 1:
		valx[0] = val;
		break;
	default:
		x = 0;
	}

	test_op_nn_with_nn(mbox, pri, service_id, cmd, valx, x, &response_status, NULL, 0, resp, resp ? &resp_response_status : NULL, NULL, 0);

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

	// send the PMIC_SERVICE_CMD_POWER_SET request
	test_op_nn_with_nn(mbox, pri, service_id, cmd, &comp_id, 1, &response_status, NULL, 0, resp, &resp_response_status, &ret_val, 1);
	if (0 != resp_response_status)
		return -1;

	if (val)
		*val = ret_val;

	return 0;
}

/* L16 format, value = Y • 2^-12 , where Y = b[15:0] e.g. 0x1199 * 2^-12 = 1.0998V */
static uint32_t l16_uv(uint32_t val)
{
        long ret = val;

	ret *= 1000000L;

	ret >>= 12;

	return (uint32_t) ret;
}

/* L11 format, value = Y • 2^N,
 *     where N = b[15:11] is a 5-bit two’s complement integer
 *           Y = b[10:0] is an 11-bit two’s complement integer
 *     Example:
 *     For b[15:0] = 0x9807 = ‘b10011_000_0000_0111’
 *     Value = 7 • 2^–13 = 854 • 10^–6
 */
static uint32_t l11_ua(uint32_t val)
{
        long ret;
        s16 exponent;
        s32 mantissa;

	exponent = ((s16)val) >> 11;
	mantissa = ((s16)((val & 0x7ff) << 5)) >> 5;

	ret = mantissa;
	ret *= 1000000L;

        if (exponent >= 0)
                ret <<= exponent;
        else
                ret >>= -exponent;

	return (uint32_t) ret;
}

/*
 * The temperature data are 16 bits with 9 bits valid, two’s complement.
 * Bit D15 is the sign bit: 1 means negative and 0 means positive.
 * Bit D14–7 contain the temperature data, with the LSB representing 0.5°C and the MSB representing 64°C.
 * The MSB is transmitted first. The last 7 bits of the lower byte, bits D6–D0, are don’t cares.
 *     Example:
 *     For b[15:0] = 0x7D0x = ‘b0111_1101_0xxx_xxxx’, value = +125°C
 *     For b[15:0] = 0xFF8x = ‘b1111_1111_1xxx_xxxx’, value = -0.5°C
 */
static uint32_t l9_mc(uint32_t val)
{
	int32_t temp = ((val & 0xff) << 1) | ((val >> 15) & 1);

	temp = ((s32)(temp << 23)) >> 23;

	temp *= 500;

	return (uint32_t) temp;
}

#undef SMS_PRINT_POWER_VOLTAGE

#ifdef  SMS_PRINT_POWER_VOLTAGE
static int test_get_all_power_voltage(struct trav_mbox *mbox)
{
	int ret = -1;
	int i;
	uint32_t val;

	for (i = 1; i < 0x0d; i++) {
		ret = test_get_with_one(mbox, 1, MAILBOX_SERVICE_ID_PMIC, PMIC_SERVICE_CMD_POWER_GET, i, PMIC_SERVICE_RSP_POWER_GET, &val);
		if (0 != ret)
			continue;

		pr_info("POWER %d: 0x%08x\n", i, val);
	}

	for (i = 1; i < 6; i++) {
		ret = test_get_with_one(mbox, 1, MAILBOX_SERVICE_ID_PMIC, PMIC_SERVICE_CMD_VOLTAGE_GET, i, PMIC_SERVICE_RSP_VOLTAGE_GET, &val);
		if (0 != ret)
			continue;

		pr_info("VOLTAGE %d: 0x%08x aka %10d uV\n", i, val, l16_uv(val));
	}
	return 0;
}
#else
static int test_get_all_power_voltage(struct trav_mbox *mbox)
{
	return 0;
}
#endif

int sms_mbox_get_time(struct trav_mbox *mbox, uint32_t *time_regs)
{
	uint32_t response_status;

	test_op_nn_with_nn(mbox, MBOX_INT_PRIO_LOW, MAILBOX_SERVICE_ID_RTC, RTC_SERVICE_CMD_GET_TIME,
					NULL, NO_ARGS, &response_status, time_regs, TIME_REGS_NN, 0, NULL, NULL, 0);
	if (response_status != SMS_MAILBOX_STATUS_OK) {
		pr_alert("MBOX: RTC_SERVICE_CMD_GET_TIME service is not working\n");
		return -EIO;
	}
	return 0;
}

int sms_mbox_set_time(struct trav_mbox *mbox, uint32_t *time_regs)
{
	uint32_t response_status;

	test_op_nn_with_nn(mbox, MBOX_INT_PRIO_LOW, MAILBOX_SERVICE_ID_RTC, RTC_SERVICE_CMD_SET_TIME,
					time_regs, TIME_REGS_NN, &response_status, NULL, NO_ARGS, 0, NULL, NULL, 0);
	if (response_status != SMS_MAILBOX_STATUS_OK) {
		pr_alert("SMS: RTC_SERVICE_CMD_SET_TIME service is not working\n");
		return -EIO;
	}
	return 0;
}

static int test_get_pushed_init(struct trav_mbox *mbox, uint32_t service_id, uint32_t cmd, int *pushed, char *name)
{
	uint32_t val = 0;
	int ret;

	ret = test_get_one_with_one(mbox, 1, service_id, cmd, 0, &val);

	if (ret != 0)
		return ret;

	*pushed = val;

	pr_info("%s: %d\n", name, *pushed);

	return 0;
}

static void test_get_all_pushed_init(struct trav_mbox *mbox)
{
	test_get_pushed_init(mbox, MAILBOX_SERVICE_ID_AP, AP_SERVICE_CMD_GET_REDUNDANCY_STATUS,
			     &ap_sms_redundancy_status_pushed, "ap_sms_redundancy_status");

	test_get_pushed_init(mbox, MAILBOX_SERVICE_ID_AP, AP_SERVICE_CMD_GET_ARB_STATE,
			     &ap_sms_arb_state_pushed, "ap_sms_arb_state");

	test_get_pushed_init(mbox, MAILBOX_SERVICE_ID_AP, AP_SERVICE_CMD_GET_OK_TO_START_AP_SESSION,
			     &ap_sms_ok_to_start_ap_session_pushed, "ap_sms_ok_to_start_ap_session");
	
	test_get_pushed_init(mbox, MAILBOX_SERVICE_ID_AP, AP_SERVICE_CMD_GET_OTHER_SMS_HEALTH,
			     &ap_sms_other_sms_health_pushed, "ap_sms_other_sms_health");
	
	test_get_pushed_init(mbox, MAILBOX_SERVICE_ID_AP, AP_SERVICE_CMD_GET_OTHER_FAULT_STATE,
			     &ap_sms_other_fault_state_pushed, "ap_sms_other_fault_state");
}

static int sms_file_service_inited;

void sms_service_init(struct trav_mbox *mbox)
{
	int ret;

	pr_info("SMS issr_tx_hi=0x%08lx\n", (unsigned long) mbox->issr_tx_hi);
	pr_info("SMS issr_rx_hi=0x%08lx\n", (unsigned long) mbox->issr_rx_hi);
	pr_info("SMS issr_tx_lo=0x%08lx\n", (unsigned long) mbox->issr_tx_lo);
	pr_info("SMS issr_rx_lo=0x%08lx\n", (unsigned long) mbox->issr_rx_lo);

	/* Clear all pending interrupts and masks */
	write32(&mbox->int_rx->intcr, 0xffffffffU);
	write32(&mbox->int_rx->intmr, 0);
	write32(&mbox->int_tx->intcr, 0xffffffffU);
	write32(&mbox->int_tx->intmr, 0);

	ret = test_service_version(mbox, 1, MAILBOX_SERVICE_ID_PMIC, PMIC_SERVICE_CMD_GET_VERSION, "PMIC");
	if (0 != ret) {
		sms_file_service_inited = -1;
		pr_info("SMS PMIC service is not working\n");
		return;
	}

	sms_file_service_inited = 1;
	pr_info("SMS PMIC service is working now\n");

	test_get_all_power_voltage(mbox);

	test_get_all_pushed_init(mbox);
}

static struct trav_mbox *sms_mbox;

/* Mailbox to read ASV info */
/* cache it, as only need to read it once */
static uint32_t hpm_asv3;
static uint32_t mail_box_read_asv(void)
{
	int ret;
	uint32_t val = -1;

	int pri = 1;
	uint32_t service_id = MAILBOX_SERVICE_ID_PMIC;
	uint32_t cmd = PMIC_SERVICE_CMD_GET_HPM_ASV;
	uint32_t index = 3; /* ASV3*/

	if (hpm_asv3)
		return hpm_asv3;

	if (!sms_mbox)
		return 0;

	ret = test_get_one_with_one(sms_mbox, pri, service_id, cmd, index, &val);

	if (ret)
		return 0;

	hpm_asv3 = val;

	pr_info("HPM_ASV3: 0x%08x\n", val);

	return val;
}

#define HPM_ASV3_CPU_MASK  0xFF0000
#define HPM_ASV3_CPU_SHIFT 16

#define HPM_ASV3_TRIP_MASK  0xFF
#define HPM_ASV3_TRIP_SHIFT 0

int mail_box_read_asv_cpu(void)
{
	uint32_t val;

	val = mail_box_read_asv();

	return (val & HPM_ASV3_CPU_MASK) >> HPM_ASV3_CPU_SHIFT;
}

int mail_box_read_asv_trip(void)
{
	uint32_t val;

	val = mail_box_read_asv();

	return (val & HPM_ASV3_TRIP_MASK) >> HPM_ASV3_TRIP_SHIFT;
}

static uint32_t uv_l16(uint32_t val)
{
	int i = 6;

	while (i > 0 && (val & 0xfff00000)) {
		val /= 10;
		i--;
	}

	if (val & 0xfff00000)
		return 0xffff;

	val <<= 12;

	while (i > 0) {
		val /= 10;
		i--;
	}

	return val;
}

int pmic_reset(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(0x82000070, 0, 0, 0, 0, 0, 0, 0, &res);

	return (int) res.a0;
}

static void a72_handle_sms_request_enable(struct trav_mbox *mbox)
{
	int ret;

	/* notify SMS that A72 is ready for handling request */
	ret = test_get_nn(mbox, 1, MAILBOX_SERVICE_ID_BOOT,
			  BOOT_SERVICE_CMD_SET_AP_BOOTED_OS,
			  NULL, 0);

	if (0 != ret)
		pr_info("Can not notify SMS that A72 is booted to os\n");
}

/*
 * This function communicates the base address and size of the region of memory
 * that will be shared by the SMS and A72 cores to transfer CAN and general
 * purpose messages.
 */
int sms_mbox_reset_sms_comm(uint32_t region_id, uint32_t ringbuffer_base_addr,
	uint32_t ringbuffer_size)
{
	uint32_t status = 0;
	uint32_t args[3] = {
		[0] = region_id,
		[1] = ringbuffer_base_addr,
		[2] = ringbuffer_size
	};

	if (!sms_mbox)
		return -1;

	test_op_nn_with_nn(sms_mbox, MBOX_INT_PRIO_HIGH, MAILBOX_SERVICE_ID_COMM,
		COMM_SERVICE_CMD_SET_COMM_REGION, args, ARRAY_SIZE(args),
		&status, NULL, 0, 0, NULL, NULL, 0);

	return status;
}

#define DRIVER_NAME	"trav-mailbox-sms"

/* sysfs */

static uint32_t ret_v(uint32_t val)
{
	return val;
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

static int32_t tmu_reg_val_to_temp(uint16_t regValue)
{
	const uint16_t maskedRegValue = regValue & 0xFFFU; // 12-bit ADC value

	// Per Samsung, the 25C code is 0x640
	// 0.0625 degrees C per bit, so 0 code is -75C
	// +8 to round, then /16 to get degrees C with offset, -75C to get degrees C.
	return (((int32_t)maskedRegValue + 8) / 16) - 75;
}

static ssize_t print_one_temps(char *buf, ssize_t sum, uint16_t *p, int start, int n, char *str)
{
	ssize_t num;
	int i;
	ssize_t sum_orig = sum;

	num = snprintf(buf + sum, PAGE_SIZE - sum, "%s", str);
	if (num < 0)
		return num;
	sum += num;

	for (i = start; i < (start + n); i++) {
		num = snprintf(buf + sum, PAGE_SIZE - sum, "%3d ", tmu_reg_val_to_temp(p[i]));

		if (num < 0)
			return num;

		sum += num;
	}

	num = snprintf(buf + sum, PAGE_SIZE - sum, "\n");
	if (num < 0)
		return num;
	sum += num;

	return sum - sum_orig;
}

static ssize_t print_all_temps(char *buf, uint16_t *p)
{
	ssize_t sum = 0, num;
	int start = 0, i;

	int n[5] = { 8, 5, 5, 1, 3 };
	char *str[5] = { "THERMAL: CPUCL0 ", "THERMAL: CPUCL2 ", "THERMAL: GT     ", "THERMAL: TOP    ", "THERMAL: GPU    " };

	/*
	  THERMAL: CPUCL0  34  37  38  35  36  36  35  42
	  THERMAL: CPUCL2  37  38  38  38  38
	  THERMAL: GT      35  37  35  38  42
	  THERMAL: TOP     37
	  THERMAL: GPU     35  35  35
	 */

	for (i = 0; i < 5; i++) {
		num = print_one_temps(buf, sum, p, start, n[i], str[i]);
		if (num < 0)
			return num;
		sum += num;
		start += n[i];
	}

	return sum;
}

#define TMU_NN 13

static uint16_t extract_raw_temp(uint32_t *raw_temps, int id)
{
	return *(((uint16_t *)raw_temps) + id);
}

int sms_mbox_get_temps(struct trav_mbox *mbox, int16_t *temps)
{
	uint32_t raw_temps[TMU_NN];
	int i, ret;

	ret = test_get_nn(mbox, 0, MAILBOX_SERVICE_ID_TMU,
				TMU_SERVICE_CMD_GET_CURRENT_TEMPS, raw_temps,
				TMU_NN);
	if (ret == -1)
		return -EIO;

	for (i = 0; i < SMS_MBOX_N_TEMPS; i++)
		temps[i] = tmu_reg_val_to_temp(extract_raw_temp(raw_temps, i));

	return 0;
}

#define MBOX_TMU_SHOW(tmu, pri, service_id, cmd)					\
static ssize_t tmu##_show(struct device *dev, struct device_attribute *attr, char *buf)		\
{												\
	struct trav_mbox *mbox = dev_get_drvdata(dev);						\
	uint32_t val[TMU_NN];									\
	int ret;										\
												\
	ret = test_get_nn(mbox, pri, service_id, cmd, val, TMU_NN);				\
												\
	if (ret == -1)										\
		return 0;									\
												\
	return print_all_temps(buf, (uint16_t *)val);						\
}

#define MBOX_STORE(power, pri, service_id, cmd_set, comp_id, resp_set, set_x, to_str, v)	\
static ssize_t power##_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) \
{												\
	struct trav_mbox *mbox = dev_get_drvdata(dev);						\
	int ret = count;									\
	uint32_t val;										\
												\
	sscanf(buf, to_str, &val);								\
	test_set_with_x(mbox, pri, service_id, cmd_set, set_x, comp_id, v(val), resp_set);	\
												\
	return ret;										\
}

#define MBOX_ATTR_SHOW_V(power, pri, service_id, cmd, comp_id, resp, from_str, from_l16)	\
MBOX_SHOW(power, pri, service_id, cmd, comp_id, resp, from_str, from_l16)			\
static DEVICE_ATTR(power, S_IRUGO, power##_show, NULL);

#define MBOX_ATTR_SHOW(power, pri, service_id, cmd, comp_id, resp)				\
MBOX_ATTR_SHOW_V(power, pri, service_id, cmd, comp_id, resp, "%x\n", ret_v)

#define MBOX_TMU_ATTR_SHOW(tmu, pri, service_id, cmd)						\
MBOX_TMU_SHOW(tmu, pri, service_id, cmd)							\
static DEVICE_ATTR(tmu, S_IRUGO, tmu##_show, NULL);

#define MBOX_ATTR_SHOW_STORE_V(power, pri, service_id, cmd, comp_id, resp, cmd_set, resp_set, set_x, from_str, to_str, from_l16, to_l16)	\
MBOX_SHOW(power, pri, service_id, cmd, comp_id, resp, from_str, from_l16)			\
MBOX_STORE(power, pri, service_id, cmd_set, comp_id, resp_set, set_x, to_str, to_l16)		\
static DEVICE_ATTR(power, S_IRUGO | S_IWUSR, power##_show, power##_store);

#define MBOX_ATTR_SHOW_STORE(power, pri, service_id, cmd, comp_id, resp, cmd_set, resp_set, set_x)	\
MBOX_ATTR_SHOW_STORE_V(power, pri, service_id, cmd, comp_id, resp, cmd_set, resp_set, set_x, "%x\n", "%x", ret_v, ret_v)

#define MBOX_ATTR_STORE(power, pri, service_id, cmd_set, comp_id, resp_set, set_x)		\
MBOX_STORE(power, pri, service_id, cmd_set, comp_id, resp_set, set_x, "%x", ret_v)			\
static DEVICE_ATTR(power, S_IWUSR, NULL, power##_store);

MBOX_ATTR_SHOW_STORE(power_12v0_sw, 1, MAILBOX_SERVICE_ID_PMIC, PMIC_SERVICE_CMD_POWER_GET,  1,
		     PMIC_SERVICE_RSP_POWER_GET, PMIC_SERVICE_CMD_POWER_SET, PMIC_SERVICE_RSP_POWER_SET, 2);
MBOX_ATTR_SHOW_STORE(power_int,     1, MAILBOX_SERVICE_ID_PMIC, PMIC_SERVICE_CMD_POWER_GET,  2,
		     PMIC_SERVICE_RSP_POWER_GET, PMIC_SERVICE_CMD_POWER_SET, PMIC_SERVICE_RSP_POWER_SET, 2);
MBOX_ATTR_SHOW_STORE(power_cpu,     1, MAILBOX_SERVICE_ID_PMIC, PMIC_SERVICE_CMD_POWER_GET,  3,
		     PMIC_SERVICE_RSP_POWER_GET, PMIC_SERVICE_CMD_POWER_SET, PMIC_SERVICE_RSP_POWER_SET, 2);
MBOX_ATTR_SHOW_STORE(power_gpu,     1, MAILBOX_SERVICE_ID_PMIC, PMIC_SERVICE_CMD_POWER_GET,  4,
		     PMIC_SERVICE_RSP_POWER_GET, PMIC_SERVICE_CMD_POWER_SET, PMIC_SERVICE_RSP_POWER_SET, 2);
MBOX_ATTR_SHOW_STORE(power_trip0,   1, MAILBOX_SERVICE_ID_PMIC, PMIC_SERVICE_CMD_POWER_GET,  5,
		     PMIC_SERVICE_RSP_POWER_GET, PMIC_SERVICE_CMD_POWER_SET, PMIC_SERVICE_RSP_POWER_SET, 2);
/* trip1 share volt with trip0 */
MBOX_ATTR_SHOW_STORE(power_1v8_ddr, 1, MAILBOX_SERVICE_ID_PMIC, PMIC_SERVICE_CMD_POWER_GET,  7,
		     PMIC_SERVICE_RSP_POWER_GET, PMIC_SERVICE_CMD_POWER_SET, PMIC_SERVICE_RSP_POWER_SET, 2);
MBOX_ATTR_SHOW_STORE(power_1v1_ddr, 1, MAILBOX_SERVICE_ID_PMIC, PMIC_SERVICE_CMD_POWER_GET,  8,
		     PMIC_SERVICE_RSP_POWER_GET, PMIC_SERVICE_CMD_POWER_SET, PMIC_SERVICE_RSP_POWER_SET, 2);
MBOX_ATTR_SHOW_STORE(power_io_0v9,  1, MAILBOX_SERVICE_ID_PMIC, PMIC_SERVICE_CMD_POWER_GET,  9,
		     PMIC_SERVICE_RSP_POWER_GET, PMIC_SERVICE_CMD_POWER_SET, PMIC_SERVICE_RSP_POWER_SET, 2);
MBOX_ATTR_SHOW_STORE(power_io_1v8,  1, MAILBOX_SERVICE_ID_PMIC, PMIC_SERVICE_CMD_POWER_GET, 10,
		     PMIC_SERVICE_RSP_POWER_GET, PMIC_SERVICE_CMD_POWER_SET, PMIC_SERVICE_RSP_POWER_SET, 2);
MBOX_ATTR_SHOW_STORE(power_io_3v3,  1, MAILBOX_SERVICE_ID_PMIC, PMIC_SERVICE_CMD_POWER_GET, 11,
		     PMIC_SERVICE_RSP_POWER_GET, PMIC_SERVICE_CMD_POWER_SET, PMIC_SERVICE_RSP_POWER_SET, 2);
MBOX_ATTR_SHOW_STORE(power_io_1v2,  1, MAILBOX_SERVICE_ID_PMIC, PMIC_SERVICE_CMD_POWER_GET, 12,
		     PMIC_SERVICE_RSP_POWER_GET, PMIC_SERVICE_CMD_POWER_SET, PMIC_SERVICE_RSP_POWER_SET, 2);

MBOX_ATTR_SHOW_STORE_V(voltage_turbo_int,   1, MAILBOX_SERVICE_ID_PMIC, PMIC_SERVICE_CMD_VOLTAGE_GET, 1,
		PMIC_SERVICE_RSP_VOLTAGE_GET, PMIC_SERVICE_CMD_VOLTAGE_SET, PMIC_SERVICE_RSP_VOLTAGE_SET, 2,
		"%d\n", "%d", l16_uv, uv_l16);
MBOX_ATTR_SHOW_STORE_V(voltage_turbo_cpu,   1, MAILBOX_SERVICE_ID_PMIC, PMIC_SERVICE_CMD_VOLTAGE_GET, 2,
		PMIC_SERVICE_RSP_VOLTAGE_GET, PMIC_SERVICE_CMD_VOLTAGE_SET, PMIC_SERVICE_RSP_VOLTAGE_SET, 2,
		"%d\n", "%d", l16_uv, uv_l16);
MBOX_ATTR_SHOW_STORE_V(voltage_turbo_gpu,   1, MAILBOX_SERVICE_ID_PMIC, PMIC_SERVICE_CMD_VOLTAGE_GET, 3,
		PMIC_SERVICE_RSP_VOLTAGE_GET, PMIC_SERVICE_CMD_VOLTAGE_SET, PMIC_SERVICE_RSP_VOLTAGE_SET, 2,
		"%d\n", "%d", l16_uv, uv_l16);
MBOX_ATTR_SHOW_STORE_V(voltage_turbo_trip0, 1, MAILBOX_SERVICE_ID_PMIC, PMIC_SERVICE_CMD_VOLTAGE_GET, 4,
		PMIC_SERVICE_RSP_VOLTAGE_GET, PMIC_SERVICE_CMD_VOLTAGE_SET, PMIC_SERVICE_RSP_VOLTAGE_SET, 2,
		"%d\n", "%d", l16_uv, uv_l16);
/* trip1 share volt with trip0 */

MBOX_ATTR_STORE(tmu_start_trace,     0, MAILBOX_SERVICE_ID_TMU, TMU_SERVICE_CMD_START_TRACE,     0, 0, 0);
MBOX_ATTR_STORE(tmu_stop_trace,      0, MAILBOX_SERVICE_ID_TMU, TMU_SERVICE_CMD_STOP_TRACE,      0, 0, 0);
MBOX_ATTR_STORE(tmu_set_sample_rate, 0, MAILBOX_SERVICE_ID_TMU, TMU_SERVICE_CMD_SET_SAMPLE_RATE, 0, 0, 1);
MBOX_ATTR_STORE(tmu_print_trace,     0, MAILBOX_SERVICE_ID_TMU, TMU_SERVICE_CMD_PRINT_TRACE,     0, 0, 0);

MBOX_TMU_ATTR_SHOW(tmu_current_temps, 0, MAILBOX_SERVICE_ID_TMU, TMU_SERVICE_CMD_GET_CURRENT_TEMPS);
MBOX_TMU_ATTR_SHOW(tmu_trace_samples, 0, MAILBOX_SERVICE_ID_TMU, TMU_SERVICE_CMD_GET_TRACE_SAMPLES);

MBOX_ATTR_SHOW_V(measured_voltage_turbo_int, 1, MAILBOX_SERVICE_ID_PMIC,
	       PMIC_SERVICE_CMD_MEASURED_VOLTAGE_CURRENT_GET, PMIC_SERVICE_MEASURED_VOLTAGE_TURBO_INT,
	       PMIC_SERVICE_RSP_MEASURED_VOLTAGE_CURRENT_GET, "%d\n", l16_uv);
MBOX_ATTR_SHOW_V(measured_voltage_turbo_cpu, 1, MAILBOX_SERVICE_ID_PMIC,
	       PMIC_SERVICE_CMD_MEASURED_VOLTAGE_CURRENT_GET, PMIC_SERVICE_MEASURED_VOLTAGE_TURBO_CPU,
	       PMIC_SERVICE_RSP_MEASURED_VOLTAGE_CURRENT_GET, "%d\n", l16_uv);
MBOX_ATTR_SHOW_V(measured_voltage_turbo_trip0, 1, MAILBOX_SERVICE_ID_PMIC,
	       PMIC_SERVICE_CMD_MEASURED_VOLTAGE_CURRENT_GET, PMIC_SERVICE_MEASURED_VOLTAGE_TURBO_TRIP0,
	       PMIC_SERVICE_RSP_MEASURED_VOLTAGE_CURRENT_GET, "%d\n", l16_uv);
/* trip1 share volt with trip0 */

MBOX_ATTR_SHOW_V(measured_current_turbo_int, 1, MAILBOX_SERVICE_ID_PMIC,
	       PMIC_SERVICE_CMD_MEASURED_VOLTAGE_CURRENT_GET, PMIC_SERVICE_MEASURED_CURRENT_TURBO_INT,
	       PMIC_SERVICE_RSP_MEASURED_VOLTAGE_CURRENT_GET, "%d\n", l11_ua);
MBOX_ATTR_SHOW_V(measured_current_turbo_cpu, 1, MAILBOX_SERVICE_ID_PMIC,
	       PMIC_SERVICE_CMD_MEASURED_VOLTAGE_CURRENT_GET, PMIC_SERVICE_MEASURED_CURRENT_TURBO_CPU,
	       PMIC_SERVICE_RSP_MEASURED_VOLTAGE_CURRENT_GET, "%d\n", l11_ua);
MBOX_ATTR_SHOW_V(measured_current_turbo_trip0, 1, MAILBOX_SERVICE_ID_PMIC,
	       PMIC_SERVICE_CMD_MEASURED_VOLTAGE_CURRENT_GET, PMIC_SERVICE_MEASURED_CURRENT_TURBO_TRIP0,
	       PMIC_SERVICE_RSP_MEASURED_VOLTAGE_CURRENT_GET, "%d\n", l11_ua);
/* trip1 share volt with trip0 */

MBOX_ATTR_SHOW_V(temperature_sensor_id_0x48, 1, MAILBOX_SERVICE_ID_PMIC,
	       PMIC_SERVICE_CMD_TEMPERATURE_GET, PMIC_SERVICE_TEMPERATURE_SENSOR_ID_0x48,
	       PMIC_SERVICE_RSP_TEMPERATURE_GET, "%d\n", l9_mc);
MBOX_ATTR_SHOW_V(temperature_sensor_id_0x49, 1, MAILBOX_SERVICE_ID_PMIC,
	       PMIC_SERVICE_CMD_TEMPERATURE_GET, PMIC_SERVICE_TEMPERATURE_SENSOR_ID_0x49,
	       PMIC_SERVICE_RSP_TEMPERATURE_GET, "%d\n", l9_mc);
MBOX_ATTR_SHOW_V(temperature_sensor_id_0x4a, 1, MAILBOX_SERVICE_ID_PMIC,
	       PMIC_SERVICE_CMD_TEMPERATURE_GET, PMIC_SERVICE_TEMPERATURE_SENSOR_ID_0x4A,
	       PMIC_SERVICE_RSP_TEMPERATURE_GET, "%d\n", l9_mc);
MBOX_ATTR_SHOW_V(temperature_sensor_id_0x4c, 1, MAILBOX_SERVICE_ID_PMIC,
	       PMIC_SERVICE_CMD_TEMPERATURE_GET, PMIC_SERVICE_TEMPERATURE_SENSOR_ID_0x4C,
	       PMIC_SERVICE_RSP_TEMPERATURE_GET, "%d\n", l9_mc);

MBOX_ATTR_STORE(bootx_a72_completed_ok, 1, MAILBOX_SERVICE_ID_BOOT, BOOT_SERVICE_CMD_A72_COMPLETED_OK, 0, 0, 0);

/* retrieved from SMS */
MBOX_ATTR_SHOW(ap_sms_redundancy_status,      1, MAILBOX_SERVICE_ID_AP,
	       AP_SERVICE_CMD_GET_REDUNDANCY_STATUS, 0, 0);
MBOX_ATTR_SHOW(ap_sms_faulted,		      1, MAILBOX_SERVICE_ID_AP,
	       AP_SERVICE_CMD_GET_FAULTED, 0, 0);
MBOX_ATTR_SHOW(ap_sms_arb_state,	      1, MAILBOX_SERVICE_ID_AP,
	       AP_SERVICE_CMD_GET_ARB_STATE, 0, 0);
MBOX_ATTR_SHOW(ap_sms_ok_to_start_ap_session, 1, MAILBOX_SERVICE_ID_AP,
	       AP_SERVICE_CMD_GET_OK_TO_START_AP_SESSION, 0, 0);
MBOX_ATTR_SHOW(ap_sms_other_sms_health, 1, MAILBOX_SERVICE_ID_AP,
	       AP_SERVICE_CMD_GET_OTHER_SMS_HEALTH, 0, 0);
MBOX_ATTR_SHOW(ap_sms_other_fault_state, 1, MAILBOX_SERVICE_ID_AP,
	       AP_SERVICE_CMD_GET_OTHER_FAULT_STATE, 0, 0);

/* pushed from SMS */
static DEVICE_INT_ATTR(ap_sms_redundancy_status_pushed,	     0444, ap_sms_redundancy_status_pushed);
static DEVICE_INT_ATTR(ap_sms_arb_state_pushed,		     0444, ap_sms_arb_state_pushed);
static DEVICE_INT_ATTR(ap_sms_ok_to_start_ap_session_pushed, 0444, ap_sms_ok_to_start_ap_session_pushed);
static DEVICE_INT_ATTR(ap_sms_other_sms_health_pushed, 0444, ap_sms_other_sms_health_pushed);
static DEVICE_INT_ATTR(ap_sms_other_fault_state_pushed, 0444, ap_sms_other_fault_state_pushed);

/* for SMS query */
static DEVICE_INT_ATTR(ap_ap_status, 0644, ap_ap_status);

/* push to SMS */
static ssize_t ap_ap_status_push_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct trav_mbox *mbox = dev_get_drvdata(dev);
	int ret = count;
	uint32_t val;

	if (kstrtouint(buf, 16, &val))
		return ret;

	ap_ap_status = val;

	test_set_with_x(mbox, 1, MAILBOX_SERVICE_ID_AP, AP_SERVICE_CMD_AP_STATUS_UPDATE, 1, 0, ap_ap_status, 0);

	return ret;
}

static DEVICE_ATTR_WO(ap_ap_status_push);

static DEVICE_UINT_ATTR(hpm_asv3, 0444, hpm_asv3);

static struct attribute *mbox_sysfs_attrs[] = {
	&dev_attr_power_12v0_sw.attr,
	&dev_attr_power_int.attr,
	&dev_attr_power_cpu.attr,
	&dev_attr_power_gpu.attr,
	&dev_attr_power_trip0.attr,
	&dev_attr_power_1v8_ddr.attr,
	&dev_attr_power_1v1_ddr.attr,
	&dev_attr_power_io_0v9.attr,
	&dev_attr_power_io_1v8.attr,
	&dev_attr_power_io_3v3.attr,
	&dev_attr_power_io_1v2.attr,
	&dev_attr_voltage_turbo_int.attr,
	&dev_attr_voltage_turbo_cpu.attr,
	&dev_attr_voltage_turbo_gpu.attr,
	&dev_attr_voltage_turbo_trip0.attr,
	&dev_attr_tmu_start_trace.attr,
	&dev_attr_tmu_stop_trace.attr,
	&dev_attr_tmu_set_sample_rate.attr,
	&dev_attr_tmu_print_trace.attr,
	&dev_attr_tmu_current_temps.attr,
	&dev_attr_tmu_trace_samples.attr,
	&dev_attr_measured_voltage_turbo_int.attr,
	&dev_attr_measured_voltage_turbo_cpu.attr,
	&dev_attr_measured_voltage_turbo_trip0.attr,
	&dev_attr_measured_current_turbo_int.attr,
	&dev_attr_measured_current_turbo_cpu.attr,
	&dev_attr_measured_current_turbo_trip0.attr,
	&dev_attr_temperature_sensor_id_0x48.attr,
	&dev_attr_temperature_sensor_id_0x49.attr,
	&dev_attr_temperature_sensor_id_0x4a.attr,
	&dev_attr_temperature_sensor_id_0x4c.attr,
	&dev_attr_bootx_a72_completed_ok.attr,
	&dev_attr_ap_sms_redundancy_status.attr,
	&dev_attr_ap_sms_faulted.attr,
	&dev_attr_ap_sms_arb_state.attr,
	&dev_attr_ap_sms_ok_to_start_ap_session.attr,
	&dev_attr_ap_sms_other_sms_health.attr,
	&dev_attr_ap_sms_other_fault_state.attr,
	&dev_attr_ap_sms_redundancy_status_pushed.attr.attr,
	&dev_attr_ap_sms_arb_state_pushed.attr.attr,
	&dev_attr_ap_sms_ok_to_start_ap_session_pushed.attr.attr,
	&dev_attr_ap_sms_other_sms_health_pushed.attr.attr,
	&dev_attr_ap_sms_other_fault_state_pushed.attr.attr,
	&dev_attr_ap_ap_status.attr.attr,
	&dev_attr_ap_ap_status_push.attr,
	&dev_attr_hpm_asv3.attr.attr,
	NULL,
};

static const struct attribute_group mbox_sysfs_attr_group = {
        .attrs = mbox_sysfs_attrs,
};

/* sub function */
static const struct mfd_cell sms_mbox_devs[] = {
	{ .name = SMS_MBOX_REGULATOR_NAME, },
	{
		.name = SMS_MBOX_THERMAL_NAME,
		.of_compatible = "turbo,trav-mailbox-sms-therm",
	},
	{
		.name = SMS_MBOX_RTC_NAME,
		.of_compatible = "turbo,trav-mailbox-sms-rtc",
	},
};

static int trav_mbox_sms_probe(struct platform_device *pdev)
{
	struct trav_mbox *mbox;
	struct mbox * mbox_regs;
	struct resource	*regs;
	int ret = 0;

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

	init_completion(&mbox->tx_completion[MBOX_INT_PRIO_LOW]);
	init_completion(&mbox->tx_completion[MBOX_INT_PRIO_HIGH]);
	init_completion(&mbox->rx_completion[MBOX_INT_PRIO_LOW]);
	init_completion(&mbox->rx_completion[MBOX_INT_PRIO_HIGH]);

	platform_set_drvdata(pdev, mbox);

	mbox_regs = (struct mbox *)(mbox->mbox_base);

	mbox->int_rx = (struct mbox_int *)(mbox->mbox_base + A72_MAILBOX_TO_A72_INT - A72_MAILBOX);
	mbox->int_tx = (struct mbox_int *)(mbox->mbox_base + A72_MAILBOX_TO_SMS_INT - A72_MAILBOX);

	mbox->issr_tx_lo = &(mbox_regs->issr_lo1);
	mbox->issr_rx_lo = &(mbox_regs->issr_lo0);

	mbox->issr_tx_hi = &(mbox_regs->issr_hi1);
	mbox->issr_rx_hi = &(mbox_regs->issr_hi0);

        /* Add device's sysfs */
	ret = sysfs_create_group(&pdev->dev.kobj, &mbox_sysfs_attr_group);
	if (ret)
		dev_err(mbox->dev, "error creating sysfs entries\n");

	ret = request_irq(mbox->irq[MBOX_INT_PRIO_LOW], mailbox_sms_isr_lo, 0,
			  dev_name(&pdev->dev), mbox);
	if (ret < 0) {
                dev_err(&pdev->dev, "failed requesting irq, irq = %d\n",
				 mbox->irq[MBOX_INT_PRIO_LOW]);
                return -1;
	}

	ret = request_irq(mbox->irq[MBOX_INT_PRIO_HIGH], mailbox_sms_isr_hi, 0,
			  dev_name(&pdev->dev), mbox);
	if (ret < 0) {
                dev_err(&pdev->dev, "failed requesting irq, irq = %d\n",
				 mbox->irq[MBOX_INT_PRIO_HIGH]);
		free_irq(mbox->irq[MBOX_INT_PRIO_LOW], mbox);
                return -1;
	}

	sms_service_init(mbox);

	if (sms_file_service_inited != 1) {
		free_irq(mbox->irq[MBOX_INT_PRIO_LOW], mbox);
		free_irq(mbox->irq[MBOX_INT_PRIO_HIGH], mbox);
		return -1;
	}

	ret = devm_mfd_add_devices(&pdev->dev, 0, sms_mbox_devs,
				ARRAY_SIZE(sms_mbox_devs), NULL, 0, NULL);
	if (ret)
		dev_err(mbox->dev, "mbox_sms: Failed to add subdevices\n");

	sms_mbox = mbox;

	a72_handle_sms_request_enable(mbox);

	return ret;
}

static int trav_mbox_sms_remove(struct platform_device *pdev)
{
	struct trav_mbox *mbox = platform_get_drvdata(pdev);

	if (!mbox)
		return -EINVAL;

	if (mbox->irq[MBOX_INT_PRIO_LOW] >= 0)
		free_irq(mbox->irq[MBOX_INT_PRIO_LOW], mbox);

	if (mbox->irq[MBOX_INT_PRIO_HIGH] >= 0)
		free_irq(mbox->irq[MBOX_INT_PRIO_HIGH], mbox);

	sysfs_remove_group(&pdev->dev.kobj, &mbox_sysfs_attr_group);

	mfd_remove_devices(&pdev->dev);

	return 0;
}

static const struct of_device_id trav_mbox_sms_match[] = {
	{ .compatible = "turbo,trav-mailbox-sms" },
	{ /* Sentinel */ }
};

MODULE_DEVICE_TABLE(of, trav_mbox_sms_match);

static struct platform_driver trav_mbox_sms_driver = {
	.probe	= trav_mbox_sms_probe,
	.remove	= trav_mbox_sms_remove,
	.driver	= {
		.name	= DRIVER_NAME,
		.of_match_table	= trav_mbox_sms_match,
	},
};

module_platform_driver(trav_mbox_sms_driver);

int sms_mbox_get_voltage(struct trav_mbox *mbox, int comp_id)
{
	uint32_t val;
	int ret;

	ret = test_get_with_one(mbox, 1, MAILBOX_SERVICE_ID_PMIC, PMIC_SERVICE_CMD_VOLTAGE_GET,
				comp_id, PMIC_SERVICE_RSP_VOLTAGE_GET, &val);

	if (ret)
		pr_debug("VOLT %d get: failed with %08x\n", comp_id, ret);
	else
		pr_debug("VOLT %d get: %08x aka %d Done\n", comp_id, val, l16_uv(val));

	return ret ? ret : l16_uv(val);
}

int sms_mbox_set_voltage(struct trav_mbox *mbox, int comp_id, int uV)
{
	uint32_t val;
	int ret;

	val = uv_l16(uV);

	pr_debug("VOLT %d try to set: %08x aka %d\n", comp_id, val, uV);

	ret = test_set_with_x(mbox, 1, MAILBOX_SERVICE_ID_PMIC, PMIC_SERVICE_CMD_VOLTAGE_SET, 2,
			      comp_id, val, PMIC_SERVICE_RSP_VOLTAGE_SET);

	if (ret)
		pr_debug("VOLT %d set: failed with %08x\n", comp_id, ret);
	else
		pr_debug("VOLT %d set: %08x aka %d Done\n", comp_id, val, uV);

	return ret;
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("TRAV mailbox sms driver module");
