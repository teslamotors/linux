/*
 * Copyright (c) 2015-2017, NVIDIA CORPORATION. All rights reserved.
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

/**********************************************/
/********TTCAN Configurations functions *******/
/**********************************************/

#include "m_ttcan.h"
#include <asm/io.h>
#include <linux/delay.h>

#define MTTCAN_INIT_TIMEOUT 1000

void ttcan_print_version(struct ttcan_controller *ttcan)
{
	u32 crel, endn;

	crel = ttcan_read32(ttcan, ADR_MTTCAN_CREL);
	endn = ttcan_read32(ttcan, ADR_MTTCAN_ENDN);

	pr_info("Release %d.%d.%d from %2.2x.%2.2x.201%1.1x\n",
		(crel & MTT_CREL_REL_MASK) >> MTT_CREL_REL_SHIFT,
		(crel & MTT_CREL_STEP_MASK) >> MTT_CREL_STEP_SHIFT,
		(crel & MTT_CREL_SUBS_MASK) >> MTT_CREL_SUBS_SHIFT,
		(crel & MTT_CREL_DAY_MASK) >> MTT_CREL_DAY_SHIFT,
		(crel & MTT_CREL_MON_MASK) >> MTT_CREL_MON_SHIFT,
		(crel & MTT_CREL_YEAR_MASK) >> MTT_CREL_YEAR_SHIFT);
	pr_debug("CAN register access %s Endian Reg 0x%x\n",
		 (endn == 0x87654321) ? "PASS" : "FAIL", endn);
}

int ttcan_write32_check(struct ttcan_controller *ttcan,
			int reg, u32 val, u32 mask)
{
	u32 ret_val;

	ttcan_write32(ttcan, reg, val);

	ret_val = ttcan_read32(ttcan, reg);

	if ((ret_val & mask) == (val & mask))
		return 0;
	else
		pr_err("%s:addr: 0x%x write 0x%x read 0x%x mask 0x%x\n",
		       __func__, reg, val, ret_val, mask);
	return -EIO;
}

inline void ttcan_set_ok(struct ttcan_controller *ttcan)
{
	u32 val;

	val = ttcan_xread32(ttcan, ADDR_M_TTCAN_CNTRL_REG);
	val |= M_TTCAN_CNTRL_REG_COK;
	ttcan_xwrite32(ttcan, ADDR_M_TTCAN_CNTRL_REG, val);
}

int ttcan_set_init(struct ttcan_controller *ttcan)
{
	u32 cccr_reg;
	int timeout = MTTCAN_INIT_TIMEOUT;

	cccr_reg = ttcan_read32(ttcan, ADR_MTTCAN_CCCR);

	if ((cccr_reg & MTT_CCCR_INIT_MASK) == 0) {
		/* Controller not yet initialized */
		cccr_reg |= 1;

		ttcan_write32(ttcan, ADR_MTTCAN_CCCR, cccr_reg);

		do {
			cccr_reg = ttcan_read32(ttcan, ADR_MTTCAN_CCCR);
			udelay(1);
			timeout--;
		} while ((cccr_reg & MTT_CCCR_INIT_MASK) == 0 && timeout);

		if (!timeout) {
			pr_err("Controller %s Timeout\n", __func__);
			return -ETIMEDOUT;
		}
	}
	return 0;
}

int ttcan_reset_init(struct ttcan_controller *ttcan)
{
	u32 cccr_reg;
	int timeout = MTTCAN_INIT_TIMEOUT;

	cccr_reg = ttcan_read32(ttcan, ADR_MTTCAN_CCCR);

	if (cccr_reg & MTT_CCCR_INIT_MASK) {
		/* Controller was initialized */
		cccr_reg &= ~1;

		ttcan_write32(ttcan, ADR_MTTCAN_CCCR, cccr_reg);

		do {
			cccr_reg = ttcan_read32(ttcan, ADR_MTTCAN_CCCR);
			udelay(1);
			timeout--;
		} while ((cccr_reg & MTT_CCCR_INIT_MASK) && timeout);

		if (!timeout) {
			pr_err("Controller %s Timeout\n", __func__);
			return -ETIMEDOUT;
		}
	}
	return 0;
}

int ttcan_set_config_change_enable(struct ttcan_controller *ttcan)
{
	u32 cccr_reg;
	int timeout = MTTCAN_INIT_TIMEOUT;

	/* initialize the core */
	if (ttcan_set_init(ttcan))
		return -ETIMEDOUT;

	/* set configuration change enable bit */
	cccr_reg = ttcan_read32(ttcan, ADR_MTTCAN_CCCR);
	cccr_reg |= MTT_CCCR_CCE_MASK;
	ttcan_write32(ttcan, ADR_MTTCAN_CCCR, cccr_reg);

	do {
		cccr_reg = ttcan_read32(ttcan, ADR_MTTCAN_CCCR);
		udelay(1);
		timeout--;
	} while (((cccr_reg & MTT_CCCR_CCE_MASK) == 0) && timeout);

	if (!timeout) {
		pr_err("Controller %s Timeout\n", __func__);
		return -ETIMEDOUT;
	}

	return 0;
}

int ttcan_set_power(struct ttcan_controller *ttcan, int value)
{

	u32 cccr_reg;
	int timeout = MTTCAN_INIT_TIMEOUT;

	cccr_reg = ttcan_read32(ttcan, ADR_MTTCAN_CCCR);
	cccr_reg &= ~(MTT_CCCR_CSR_MASK);
	cccr_reg |= ((~value) << MTT_CCCR_CSR_SHIFT) & MTT_CCCR_CSR_MASK;
	ttcan_write32(ttcan, ADR_MTTCAN_CCCR, cccr_reg);

	do {
		cccr_reg = ttcan_read32(ttcan, ADR_MTTCAN_CCCR);
		udelay(1);
		timeout--;
	} while (((cccr_reg & MTT_CCCR_CSA_MASK) >> MTT_CCCR_CSA_SHIFT)
		 == value && timeout);

	if (!timeout) {
		pr_err("Controller %s Timeout\n", __func__);
		return -ETIMEDOUT;
	}

	return 0;
}

void ttcan_reset_config_change_enable(struct ttcan_controller *ttcan)
{

	/* reset the core */
	if (ttcan_reset_init(ttcan))
		return;

	/*CCCR.CCE is automatically reset when CCCR.INIT is reset */
}

void ttcan_disable_auto_retransmission(struct ttcan_controller *ttcan,
		bool enable)
{
	u32 cccr_reg;

	/* set DAR bit */
	cccr_reg = ttcan_read32(ttcan, ADR_MTTCAN_CCCR);
	if (enable)
		cccr_reg |= MTT_CCCR_DAR_MASK;
	else
		cccr_reg &= ~MTT_CCCR_DAR_MASK;
	ttcan_write32_check(ttcan, ADR_MTTCAN_CCCR, cccr_reg, MTTCAN_CCCR_MSK);
}

int ttcan_set_loopback(struct ttcan_controller *ttcan)
{
	u32 test_reg;
	u32 cccr_reg;
	int timeout = MTTCAN_INIT_TIMEOUT;

	/* set TEST.LBCK (external loopback) bit */
	cccr_reg = ttcan_read32(ttcan, ADR_MTTCAN_CCCR);
	test_reg = ttcan_read32(ttcan, ADR_MTTCAN_TEST);

	if (ttcan_protected(cccr_reg))
		return -EPERM;

	cccr_reg |= MTT_CCCR_TEST_MASK;
	test_reg |= MTT_TEST_LBCK_MASK;

	ttcan_write32(ttcan, ADR_MTTCAN_CCCR, cccr_reg);
	do {
		cccr_reg = ttcan_read32(ttcan, ADR_MTTCAN_CCCR);
		udelay(1);
		timeout--;
	} while ((cccr_reg & MTT_CCCR_TEST_MASK) == 0 && timeout);

	if (!timeout) {
		pr_err("%s: Timeout cccr = 0x%x\n", __func__, cccr_reg);
		return -ETIMEDOUT;
	}

	return ttcan_write32_check(ttcan, ADR_MTTCAN_TEST,
				   test_reg, MTTCAN_TEST_MSK);

}

int ttcan_set_bus_monitoring_mode(struct ttcan_controller *ttcan, bool enable)
{
	u32 cccr_reg;

	/* set MON bit(bus monitor mode */
	cccr_reg = ttcan_read32(ttcan, ADR_MTTCAN_CCCR);
	if (ttcan_protected(cccr_reg))
		return -EPERM;
	if (enable)
		cccr_reg |= MTT_CCCR_MON_MASK;
	else
		cccr_reg &= ~MTT_CCCR_MON_MASK;
	return ttcan_write32_check(ttcan, ADR_MTTCAN_CCCR,
				   cccr_reg, MTTCAN_CCCR_MSK);
}

int ttcan_set_normal_mode(struct ttcan_controller *ttcan)
{
	u32 cccr_reg;
	u32 test_reg;
	int timeout = MTTCAN_INIT_TIMEOUT;

	/* Clear loopback and monitor mode */
	cccr_reg = ttcan_read32(ttcan, ADR_MTTCAN_CCCR);
	test_reg = ttcan_read32(ttcan, ADR_MTTCAN_TEST);

	if (ttcan_protected(cccr_reg))
		return -EPERM;

	if (test_reg & MTT_TEST_LBCK_MASK) {
		test_reg &= ~(MTT_TEST_LBCK_MASK);
		if ((cccr_reg & MTT_CCCR_TEST_MASK) == 0) {
			cccr_reg |= MTT_CCCR_TEST_MASK;
			ttcan_write32(ttcan, ADR_MTTCAN_CCCR, cccr_reg);
			do {
				cccr_reg = ttcan_read32(ttcan, ADR_MTTCAN_CCCR);
				udelay(1);
				timeout--;
			} while ((cccr_reg & MTT_CCCR_TEST_MASK) == 0
				 && timeout);

			if (!timeout) {
				pr_err("%s: Timeout cccr = 0x%x\n", __func__,
				       cccr_reg);
				return -ETIMEDOUT;
			}
		}
		ttcan_write32_check(ttcan, ADR_MTTCAN_TEST, test_reg,
				    MTTCAN_TEST_MSK);
	}
	cccr_reg &= ~(MTT_CCCR_MON_MASK);
	cccr_reg &= ~(MTT_CCCR_TEST_MASK);
	return ttcan_write32_check(ttcan, ADR_MTTCAN_CCCR, cccr_reg,
				   MTTCAN_CCCR_MSK);
}

inline u32 ttcan_read_ecr(struct ttcan_controller *ttcan)
{
	return ttcan_read32(ttcan, ADR_MTTCAN_ECR);
}

int ttcan_set_bitrate(struct ttcan_controller *ttcan)
{
	unsigned int temp_reg;
	int ret = 0;
	u32 cccr_reg;
	u32 nbtp_reg;
	u32 dbtp_reg;
	u32 tdcr_reg;

	nbtp_reg = ((ttcan->bt_config.nominal.phase_seg2 - 1) <<
		    MTT_NBTP_NTSEG2_SHIFT) & MTT_NBTP_NTSEG2_MASK;
	nbtp_reg |= ((ttcan->bt_config.nominal.phase_seg1 +
		      ttcan->bt_config.nominal.prop_seg - 1)
		     << MTT_NBTP_NTSEG1_SHIFT) & MTT_NBTP_NTSEG1_MASK;

	nbtp_reg |= (ttcan->bt_config.nominal.sjw - 1) <<
	    MTT_NBTP_NSJW_SHIFT & MTT_NBTP_NSJW_MASK;
	nbtp_reg |= (ttcan->bt_config.nominal.brp - 1) <<
	    MTT_NBTP_NBRP_SHIFT & MTT_NBTP_NBRP_MASK;

	pr_debug("%s NBTP(0x%x) value (0x%x)\n", __func__, ADR_MTTCAN_NBTP,
		nbtp_reg);
	ret = ttcan_write32_check(ttcan, ADR_MTTCAN_NBTP, nbtp_reg,
		MTTCAN_NBTP_MSK);
	if (ret) {
		pr_err("%s: Normal bitrate configuration failed\n", __func__);
		return ret;
	}

	if (ttcan->bt_config.fd_flags & CAN_FD_FLAG) {
		dbtp_reg = ((ttcan->bt_config.data.phase_seg2 - 1) <<
			    MTT_DBTP_DTSEG2_SHIFT) & MTT_DBTP_DTSEG2_MASK;
		dbtp_reg |= ((ttcan->bt_config.data.phase_seg1 +
			      ttcan->bt_config.data.prop_seg - 1) <<
			     MTT_DBTP_DTSEG1_SHIFT) & MTT_DBTP_DTSEG1_MASK;
		dbtp_reg |= ((ttcan->bt_config.data.sjw - 1) <<
			     MTT_DBTP_DSJW_SHIFT) & MTT_DBTP_DSJW_MASK;
		dbtp_reg |= ((ttcan->bt_config.data.brp - 1) <<
			     MTT_DBTP_DBRP_SHIFT) & MTT_DBTP_DBRP_MASK;
		dbtp_reg |= (ttcan->bt_config.data.tdc << MTT_DBTP_TDC_SHIFT) &
		    MTT_DBTP_TDC_MASK;

		tdcr_reg = (ttcan->bt_config.data.tdc_offset <<
			MTT_TDCR_TDCO_SHIFT) & MTT_TDCR_TDCO_MASK;

		pr_debug("%s DBTP(0x%x) value (0x%x)\n", __func__,
			ADR_MTTCAN_DBTP, dbtp_reg);
		ret = ttcan_write32_check(ttcan, ADR_MTTCAN_DBTP,
					dbtp_reg, MTTCAN_DBTP_MSK);
		if (ret) {
			pr_err("%s: Fast bitrate configuration failed\n",
			       __func__);
			return ret;
		}

		ret = ttcan_write32_check(ttcan, ADR_MTTCAN_TDCR,
					tdcr_reg, MTTCAN_TDCR_MSK);
		if (ret) {
			pr_err("%s: Fast bitrate configuration failed\n",
			       __func__);
			return ret;
		}

		temp_reg = cccr_reg = ttcan_read32(ttcan, ADR_MTTCAN_CCCR);
		if (ttcan->bt_config.fd_flags & CAN_FD_FLAG)
			cccr_reg |= MTT_CCCR_FDOE_MASK;
		else
			cccr_reg &= ~(MTT_CCCR_FDOE_MASK);

		if (ttcan->bt_config.fd_flags & CAN_BRS_FLAG)
			cccr_reg |= MTT_CCCR_BRSE_MASK;
		else
			cccr_reg &= ~(MTT_CCCR_BRSE_MASK);

		if (temp_reg != cccr_reg) {
			ret = ttcan_write32_check(ttcan, ADR_MTTCAN_CCCR,
				cccr_reg, MTTCAN_CCCR_MSK);
			if (ret) {
				pr_err("%s: Error in enabling FD\n", __func__);
				return ret;
			}
		}

	}
	return ret;
}

int ttcan_tx_req_pending(struct ttcan_controller *ttcan)
{
	u32 txbrp_reg = ttcan_read32(ttcan, ADR_MTTCAN_TXBRP);

	if (txbrp_reg)
		return 1;
	return 0;
}

int ttcan_tx_buff_req_pending(struct ttcan_controller *ttcan, u8 index)
{
	u32 txbrp_reg;
	u32 mask = 1 << index;

	txbrp_reg = ttcan_read32(ttcan, ADR_MTTCAN_TXBRP);

	if (txbrp_reg & mask)
		return 1;
	else
		return 0;
}

bool ttcan_tx_buffers_full(struct ttcan_controller *ttcan)
{
	u32 txbrp_reg;
	u32 txfqs_full = 1;
	u32 mask = (1 << ttcan->tx_config.ded_buff_num) - 1;

	/* If FIFO/queue is enabled, check if full bit is set */
	if (ttcan->tx_config.fifo_q_num)
		txfqs_full = (ttcan_read32(ttcan, ADR_MTTCAN_TXFQS) &
				MTT_TXFQS_TFQF_MASK) >> MTT_TXFQS_TFQF_SHIFT;

	/* Check for pending Tx requests in msg buffer */
	txbrp_reg = ttcan_read32(ttcan, ADR_MTTCAN_TXBRP) & mask;

	if ((txbrp_reg == mask) && txfqs_full)
		return true;
	else
		return false;
}

void ttcan_tx_ded_msg_write(struct ttcan_controller *ttcan,
			    struct ttcanfd_frame *ttcanfd,
			    u8 index)
{
	u32 ram_addr = ttcan->mram_cfg[MRAM_TXB].off +
	    (index * ttcan->e_size.tx_buffer);
	ttcan_write_tx_msg_ram(ttcan, ram_addr, ttcanfd, index);
	ttcan->tx_buf_dlc[index] = ttcanfd->d_len;
}

void ttcan_tx_trigger_msg_transmit(struct ttcan_controller *ttcan, u8 index)
{
	ttcan_write32(ttcan, ADR_MTTCAN_TXBAR, (1 << index));
}

int ttcan_tx_msg_buffer_write(struct ttcan_controller *ttcan,
			      struct ttcanfd_frame *ttcanfd)
{
	int msg_no;
	u32 txbrp_free = ~ttcan_read32(ttcan, ADR_MTTCAN_TXBRP);

	/* mask out buffers that are previously reserved by SW */
	txbrp_free &= ~ttcan->tx_object;

	/* mask for buffers reserved for Tx message buffers */
	txbrp_free &= (1 << ttcan->tx_config.ded_buff_num) - 1;

	msg_no = ffs(txbrp_free) - 1;
	if (msg_no < 0)
		return -ENOMEM;

	/* Write to CAN controller message RAM */
	ttcan_tx_ded_msg_write(ttcan, ttcanfd, msg_no);

	return msg_no;
}

int ttcan_set_tx_buffer_addr(struct ttcan_controller *ttcan)
{
	int ret = 0;
	u32 txbc_reg;
	u32 txesc_reg;
	u32 tx_intr_en;
	u32 rel_start_addr = ttcan->mram_cfg[MRAM_TXB].off >> 2;
	enum ttcan_data_field_size dfs = ttcan->tx_config.dfs;


	txbc_reg = (rel_start_addr << MTT_TXBC_TBSA_SHIFT) & MTT_TXBC_TBSA_MASK;
	txbc_reg |= (ttcan->tx_config.ded_buff_num << MTT_TXBC_NDTB_SHIFT) &
			MTT_TXBC_NDTB_MASK;
	txbc_reg |= (ttcan->tx_config.fifo_q_num << MTT_TXBC_TFQS_SHIFT) &
			MTT_TXBC_TFQS_MASK;

	if (ttcan->tx_config.flags & 0x1)
		txbc_reg |= MTT_TXBC_TFQM_MASK;	/* Queue mode */
	else
		txbc_reg &= ~(MTT_TXBC_TFQM_MASK);	/* FIFO mode */

	ret = ttcan_write32_check(ttcan, ADR_MTTCAN_TXBC, txbc_reg,
		MTTCAN_TXBC_MSK);
	if (ret) {
		pr_err("%s: Error in setting ADR_MTTCAN_TXBC\n", __func__);
		return ret;
	}

	txesc_reg = (dfs << MTT_TXESC_TBDS_SHIFT) & MTT_TXESC_TBDS_MASK;

	ret = ttcan_write32_check(ttcan, ADR_MTTCAN_TXESC, txesc_reg,
		MTTCAN_TXESC_MSK);
	if (ret) {
		pr_err("%s: Error in setting ADR_MTTCAN_TXESC\n", __func__);
		return ret;
	}

	tx_intr_en = (1 << (ttcan->tx_config.ded_buff_num
			+ ttcan->tx_config.fifo_q_num)) - 1;
	/* Enable TC interrupt for tx buffers + queue */
	ttcan_write32(ttcan, ADR_MTTCAN_TXBTIE, tx_intr_en);
	/* Enable TCF interrupt for tx buffers */
	ttcan_write32(ttcan, ADR_MTTCAN_TXBCIE, tx_intr_en);

	return ret;
}

/* Queue Message in Tx Queue
 * Return
 *	-ve in case of error
 *      idx written buffer index
 */
int ttcan_tx_fifo_queue_msg(struct ttcan_controller *ttcan,
			    struct ttcanfd_frame *ttcanfd)
{
	u32 txfqs_reg;
	u32 put_idx;

	txfqs_reg = ttcan_read32(ttcan, ADR_MTTCAN_TXFQS);

	/* Test for Tx FIFO/Queue full */
	if (txfqs_reg & MTT_TXFQS_TFQF_MASK)
		return -ENOMEM;

	/* Test if Tx index is previously reserved in SW */
	put_idx = (txfqs_reg & MTT_TXFQS_TFQPI_MASK) >> MTT_TXFQS_TFQPI_SHIFT;
	if (ttcan->tx_object & (1 << put_idx))
		return -ENOMEM;

	/* Write to CAN controller message RAM */
	ttcan_tx_ded_msg_write(ttcan, ttcanfd, put_idx);

	return put_idx;

}

/* Check tx fifo status
*  return 1 if fifo full
*/
int ttcan_tx_fifo_full(struct ttcan_controller *ttcan)
{
	u32 txfqs_reg;
	txfqs_reg = ttcan_read32(ttcan, ADR_MTTCAN_TXFQS);
	return (txfqs_reg & MTT_TXFQS_TFQF_MASK) >> MTT_TXFQS_TFQF_SHIFT;
}

static int process_rx_mesg(struct ttcan_controller *ttcan, u32 addr)
{
	struct ttcanfd_frame ttcanfd;
	ttcan_read_rx_msg_ram(ttcan, addr, &ttcanfd);
	return add_msg_controller_list(ttcan, &ttcanfd, &ttcan->rx_b, BUFFER);
}

int ttcan_read_rx_buffer(struct ttcan_controller *ttcan)
{
	u32 ndat1, ndat2;
	u32 read_addr;
	int msgs_read = 0;

	ndat1 = ttcan_read32(ttcan, ADR_MTTCAN_NDAT1);
	ndat2 = ttcan_read32(ttcan, ADR_MTTCAN_NDAT2);

	while (ndat1 != 0 || ndat2 != 0) {
		u32 bit_set1 = ffs(ndat1) - 1;
		u32 bit_set2 = ffs(ndat2) - 1;
		if (ndat1) {
			read_addr = ttcan->mram_cfg[MRAM_RXB].off + (bit_set1 *
				ttcan->e_size.rx_buffer);
			if (process_rx_mesg(ttcan, read_addr))
				return msgs_read;
			ttcan_write32(ttcan, ADR_MTTCAN_NDAT1,
				(1 << (bit_set1)));
			msgs_read++;
		}

		if (ndat2) {
			read_addr = ttcan->mram_cfg[MRAM_RXB].off + (bit_set2 *
				ttcan->e_size.rx_buffer);
			if (process_rx_mesg(ttcan, read_addr))
				return msgs_read;
			ttcan_write32(ttcan, ADR_MTTCAN_NDAT2,
				(1 << (bit_set2)));
			msgs_read++;
		}
		ndat1 &= ~(1 << (bit_set1));
		ndat2 &= ~(1 << (bit_set2));
	}

	return msgs_read;
}

/* Tx Evt Fifo */

unsigned int ttcan_read_txevt_fifo(struct ttcan_controller *ttcan)
{
	struct mttcan_tx_evt_element txevt;
	u32 txefs;
	u32 read_addr;
	int q_read = 0;
	int msgs_read = 0;

	txefs = ttcan_read32(ttcan, ADR_MTTCAN_TXEFS);

	if (!(txefs & MTT_TXEFS_EFFL_MASK)) {
		pr_info("%s: Tx Event FIFO empty\n", __func__);
		return 0;
	}
	q_read = ttcan->tx_config.evt_q_num;
	while ((txefs & MTT_TXEFS_EFFL_MASK) && q_read--) {

		u32 get_idx =
		    (txefs & MTT_TXEFS_EFGI_MASK) >> MTT_TXEFS_EFGI_SHIFT;
		read_addr =
		    ttcan->mram_cfg[MRAM_TXE].off +
		    (get_idx * TX_EVENT_FIFO_ELEM_SIZE);

		pr_debug("%s:txevt: read_addr %x EFGI %x\n", __func__,
			 read_addr, get_idx);

		ttcan_read_txevt_ram(ttcan, read_addr, &txevt);
		if (add_event_controller_list(ttcan, &txevt,
					      &ttcan->tx_evt) < 0) {
			pr_err("%s: failed to add to list\n", __func__);
			return msgs_read;
		}
		ttcan_write32(ttcan, ADR_MTTCAN_TXEFA, get_idx);
		txefs = ttcan_read32(ttcan, ADR_MTTCAN_TXEFS);
		msgs_read++;
	}
	return msgs_read;
}

/* Rx FIFO section */

unsigned int ttcan_read_rx_fifo0(struct ttcan_controller *ttcan)
{
	u32 rxf0s_reg;
	struct ttcanfd_frame ttcanfd;
	u32 read_addr;
	int q_read = 0;
	unsigned int msgs_read = 0;

	rxf0s_reg = ttcan_read32(ttcan, ADR_MTTCAN_RXF0S);

	if (!(rxf0s_reg & MTT_RXF0S_F0FL_MASK)) {
		return msgs_read;
	}

	/* Read at max queue size in one attempt */
	q_read = ttcan->mram_cfg[MRAM_RXF0].num;

	while ((rxf0s_reg & MTT_RXF0S_F0FL_MASK) && q_read--) {
		u32 get_idx = (rxf0s_reg & MTT_RXF0S_F0GI_MASK) >>
		    MTT_RXF0S_F0GI_SHIFT;
		if (ttcan->rx_config.rxq0_bmsk & (1 << get_idx)) {
			/* All ready process on High priority */
			ttcan_write32(ttcan, ADR_MTTCAN_RXF0A, get_idx);
			ttcan->rx_config.rxq0_bmsk &= ~(1U << get_idx);
			rxf0s_reg = ttcan_read32(ttcan, ADR_MTTCAN_RXF0S);
			continue;
		}

		read_addr = ttcan->mram_cfg[MRAM_RXF0].off +
		    (get_idx * ttcan->e_size.rx_fifo0);

		pr_debug("%s:fifo0: read_addr %x FOGI %x\n", __func__,
			 read_addr, get_idx);

		ttcan_read_rx_msg_ram(ttcan, read_addr, &ttcanfd);
		if (add_msg_controller_list(ttcan, &ttcanfd,
					    &ttcan->rx_q0, FIFO_0) < 0) {
			pr_err("%s: failed to add to list\n", __func__);
			return msgs_read;
		}
		ttcan_write32(ttcan, ADR_MTTCAN_RXF0A, get_idx);
		rxf0s_reg = ttcan_read32(ttcan, ADR_MTTCAN_RXF0S);
		msgs_read++;
	}
	return msgs_read;
}

unsigned int ttcan_read_rx_fifo1(struct ttcan_controller *ttcan)
{
	u32 rxf1s_reg;
	struct ttcanfd_frame ttcanfd;
	u32 read_addr;
	int q_read = 0;
	int msgs_read = 0;

	rxf1s_reg = ttcan_read32(ttcan, ADR_MTTCAN_RXF1S);

	if (!(rxf1s_reg & MTT_RXF1S_F1FL_MASK)) {
		return msgs_read;
	}

	/* Read at max queue size in one attempt */
	q_read = ttcan->mram_cfg[MRAM_RXF1].num;

	while ((rxf1s_reg & MTT_RXF1S_F1FL_MASK) && q_read--) {
		u32 get_idx = (rxf1s_reg & MTT_RXF1S_F1GI_MASK) >>
		    MTT_RXF1S_F1GI_SHIFT;
		if (ttcan->rx_config.rxq1_bmsk & (1 << get_idx)) {
			/* All ready process on High priority */
			ttcan_write32(ttcan, ADR_MTTCAN_RXF1A, get_idx);
			ttcan->rx_config.rxq1_bmsk &= ~(1U << get_idx);
			rxf1s_reg = ttcan_read32(ttcan, ADR_MTTCAN_RXF1S);
			continue;
		}
		read_addr = ttcan->mram_cfg[MRAM_RXF1].off +
		    (get_idx * ttcan->e_size.rx_fifo1);

		pr_debug("%s:fifo1: read_addr %x FOGI %x\n", __func__,
			 read_addr, get_idx);

		ttcan_read_rx_msg_ram(ttcan, read_addr, &ttcanfd);
		if (add_msg_controller_list(ttcan, &ttcanfd,
					    &ttcan->rx_q1, FIFO_1) < 0) {
			pr_err("%s: failed to add to list\n", __func__);
			return msgs_read;
		}
		ttcan_write32(ttcan, ADR_MTTCAN_RXF1A, get_idx);
		rxf1s_reg = ttcan_read32(ttcan, ADR_MTTCAN_RXF1S);
		msgs_read++;
	}
	return msgs_read;
}

/* Returns message read else return 0 */
unsigned int ttcan_read_rx_fifo(struct ttcan_controller *ttcan)
{
	int msgs_read = 0;

	msgs_read = ttcan_read_rx_fifo0(ttcan);

	if (ttcan->mram_cfg[MRAM_RXF1].num)
		msgs_read += ttcan_read_rx_fifo1(ttcan);

	return msgs_read;
}

unsigned int ttcan_read_hp_mesgs(struct ttcan_controller *ttcan,
				 struct ttcanfd_frame *ttcanfd)
{
	u32 hpms;
	u32 fltr_idx;
	u32 buf_idx;
	u32 msi;
	u32 read_addr;

	hpms = ttcan_read32(ttcan, ADR_MTTCAN_HPMS);
	fltr_idx = (hpms & MTT_HPMS_FIDX_MASK) >> MTT_HPMS_FIDX_SHIFT;
	buf_idx = (hpms & MTT_HPMS_BIDX_MASK) >> MTT_HPMS_BIDX_SHIFT;
	msi = (hpms & MTT_HPMS_MSI_MASK) >> MTT_HPMS_MSI_SHIFT;

	if (hpms & MTT_HPMS_FLST_MASK) {
		/* Extended Filter list */
		pr_debug("Xtd Filter:%d Matched\n", fltr_idx);
		pr_debug("0x%llx\n", ttcan_get_xtd_id_filter(ttcan, fltr_idx));
	} else {
		/* Standard Filter list */
		pr_debug("Std Filter:%d Matched\n", fltr_idx);
		pr_debug("0x%x\n", ttcan_get_std_id_filter(ttcan, fltr_idx));
	}

	switch (msi) {
	default:
		pr_info("High Priority Interrupt recieved, no mesg\n");
		return 0;
	case 1:
		pr_info("High Priority FIFO Mesg lost\n");
		return 0;
	case 2:
		read_addr = ttcan->mram_cfg[MRAM_RXF0].off +
		    (buf_idx * ttcan->e_size.rx_fifo0);
		ttcan_read_rx_msg_ram(ttcan, read_addr, ttcanfd);
		ttcan->rx_config.rxq0_bmsk |= 1 << buf_idx;
		break;
	case 3:
		read_addr = ttcan->mram_cfg[MRAM_RXF1].off +
		    buf_idx * ttcan->e_size.rx_fifo1;
		ttcan_read_rx_msg_ram(ttcan, read_addr, ttcanfd);
		ttcan->rx_config.rxq1_bmsk |= 1 << buf_idx;
	}
	return 1;
}
/* Rx Buff Section */
void ttcan_set_rx_buffers_elements(struct ttcan_controller *ttcan)
{
	u32 rxbc_reg = 0;
	u32 rxf0c_reg = 0;
	u32 rxf1c_reg = 0;
	u32 rxesc_reg = 0;
	u32 rel_phy_addr;
	enum ttcan_data_field_size rxbuf_dfs;
	enum ttcan_data_field_size rxfifo0_dfs;
	enum ttcan_data_field_size rxfifo1_dfs;

	u32 rxq0 = ttcan->mram_cfg[MRAM_RXF0].num;
	u32 rxq1 = ttcan->mram_cfg[MRAM_RXF1].num;

	/* Set Rx Buffer Address */
	rel_phy_addr = ttcan->mram_cfg[MRAM_RXB].off >> 2;
	rxbc_reg = (rel_phy_addr << MTT_RXBC_RBSA_SHIFT) & MTT_RXBC_RBSA_MASK;

	ttcan_write32(ttcan, ADR_MTTCAN_RXBC, rxbc_reg);

	/* Set RXFIFO 0 */
	rel_phy_addr = ttcan->mram_cfg[MRAM_RXF0].off >> 2;
	rxf0c_reg = ((rxq0 / 2) << MTT_RXF0C_F0WM_SHIFT) & MTT_RXF0C_F0WM_MASK;
	rxf0c_reg |= (rxq0 << MTT_RXF0C_F0S_SHIFT) & MTT_RXF0C_F0S_MASK;
	rxf0c_reg |= (rel_phy_addr << MTT_RXF0C_F0SA_SHIFT) &
	    MTT_RXF0C_F0SA_MASK;

	ttcan_write32(ttcan, ADR_MTTCAN_RXF0C, rxf0c_reg);

	/* Set RxFIFO 1 */
	rel_phy_addr = ttcan->mram_cfg[MRAM_RXF1].off >> 2;
	rxf1c_reg = ((rxq1 / 2) << MTT_RXF1C_F1WM_SHIFT) & MTT_RXF1C_F1WM_MASK;
	rxf1c_reg |= (rxq1 << MTT_RXF1C_F1S_SHIFT) & MTT_RXF1C_F1S_MASK;
	rxf1c_reg |= (rel_phy_addr << MTT_RXF1C_F1SA_SHIFT) &
	    MTT_RXF1C_F1SA_MASK;

	ttcan_write32(ttcan, ADR_MTTCAN_RXF1C, rxf1c_reg);


	/* Set Rx element datasize */
	rxbuf_dfs = get_dfs(ttcan->rx_config.rxb_dsize);
	rxfifo0_dfs = get_dfs(ttcan->rx_config.rxq0_dsize);
	rxfifo1_dfs = get_dfs(ttcan->rx_config.rxq1_dsize);

	rxesc_reg = (rxbuf_dfs << MTT_RXESC_RBDS_SHIFT) & MTT_RXESC_RBDS_MASK;
	rxesc_reg |= (rxfifo0_dfs << MTT_RXESC_F0DS_SHIFT) &
	    MTT_RXESC_F0DS_MASK;
	rxesc_reg |= (rxfifo1_dfs << MTT_RXESC_F1DS_SHIFT) &
	    MTT_RXESC_F1DS_MASK;

	ttcan_write32(ttcan, ADR_MTTCAN_RXESC, rxesc_reg);

	ttcan->e_size.rx_buffer =
	    RXB_ELEM_HEADER_SIZE + ttcan->rx_config.rxb_dsize;
	ttcan->e_size.rx_fifo0 =
	    RXB_ELEM_HEADER_SIZE + ttcan->rx_config.rxq0_dsize;
	ttcan->e_size.rx_fifo1 =
	    RXB_ELEM_HEADER_SIZE + ttcan->rx_config.rxq1_dsize;
}

/*  Filters Section */

int ttcan_set_gfc(struct ttcan_controller *ttcan, u32 regval)
{
	int ret = 0;
	ret = ttcan_write32_check(ttcan, ADR_MTTCAN_GFC, regval,
			MTTCAN_GFC_MSK);
	if (ret)
		pr_err("%s: unable to set GFC register\n", __func__);

	return ret;
}

u32 ttcan_get_gfc(struct ttcan_controller *ttcan)
{
	return ttcan_read32(ttcan, ADR_MTTCAN_GFC);
}

int ttcan_set_xidam(struct ttcan_controller *ttcan, u32 regval)
{
	int ret = 0;
	ret = ttcan_write32_check(ttcan, ADR_MTTCAN_XIDAM, regval,
			MTTCAN_XIDAM_MSK);
	if (ret)
		pr_err("%s: unable to set XIDAM register\n", __func__);
	return ret;
}

u32 ttcan_get_xidam(struct ttcan_controller *ttcan)
{
	return ttcan_read32(ttcan, ADR_MTTCAN_XIDAM);
}

int ttcan_set_ttrmc(struct ttcan_controller *ttcan, u32 regval)
{
	int ret = 0;
	ret = ttcan_write32_check(ttcan, ADR_MTTCAN_TTRMC, regval,
		MTTCAN_TTRMC_MSK);
	if (ret)
		pr_err("%s: unable to set TTRMC register\n", __func__);
	return ret;
}

u32 ttcan_get_ttrmc(struct ttcan_controller *ttcan)
{
	return ttcan_read32(ttcan, ADR_MTTCAN_TTRMC);
}

void ttcan_set_std_id_filter_addr(struct ttcan_controller *ttcan)
{
	u32 sidfc_reg = 0;
	u32 rel_start_addr = ttcan->mram_cfg[MRAM_SIDF].off >> 2;
	u32 list_size = ttcan->mram_cfg[MRAM_SIDF].num;

	if (list_size > 128)
		list_size = 128;

	sidfc_reg = (rel_start_addr << MTT_SIDFC_FLSSA_SHIFT) &
	    MTT_SIDFC_FLSSA_MASK;
	sidfc_reg |= (list_size << MTT_SIDFC_LSS_SHIFT) & MTT_SIDFC_LSS_MASK;
	ttcan_write32(ttcan, ADR_MTTCAN_SIDFC, sidfc_reg);
}

void ttcan_set_xtd_id_filter_addr(struct ttcan_controller *ttcan)
{
	u32 xidfc_reg = 0;
	u32 list_size = ttcan->mram_cfg[MRAM_XIDF].num;
	u32 rel_start_addr = ttcan->mram_cfg[MRAM_XIDF].off >> 2;

	if (list_size > 64)
		list_size = 64;

	xidfc_reg = (rel_start_addr << MTT_XIDFC_FLESA_SHIFT) &
	    MTT_XIDFC_FLESA_MASK;
	xidfc_reg |= (list_size << MTT_XIDFC_LSE_SHIFT) & MTT_XIDFC_LSE_MASK;
	ttcan_write32(ttcan, ADR_MTTCAN_XIDFC, xidfc_reg);

}

void ttcan_set_time_stamp_conf(struct ttcan_controller *ttcan,
			       u16 timer_prescalar,
			       enum ttcan_timestamp_source timer_type)
{
	u32 tscc = 0;

	if (timer_prescalar > 15)
		timer_prescalar = 15;

	tscc = (timer_prescalar << MTT_TSCC_TCP_SHIFT) & MTT_TSCC_TCP_MASK;
	tscc |= (timer_type << MTT_TSCC_TSS_SHIFT) & MTT_TSCC_TSS_MASK;
	ttcan_write32(ttcan, ADR_MTTCAN_TSCC, tscc);
	ttcan->ts_prescalar = timer_prescalar + 1;
}

void ttcan_set_txevt_fifo_conf(struct ttcan_controller *ttcan)
{
	u32 txefc = 0;
	u32 rel_addr = ttcan->mram_cfg[MRAM_TXE].off >> 2;
	u32 evf_size = ttcan->mram_cfg[MRAM_TXE].num;

	txefc = ((evf_size / 2) << MTT_TXEFC_EFWM_SHIFT) &
				MTT_TXEFC_EFWM_MASK;

	txefc |= evf_size << MTT_TXEFC_EFS_SHIFT & MTT_TXEFC_EFS_MASK;
	txefc |= rel_addr << MTT_TXEFC_EFSA_SHIFT & MTT_TXEFC_EFSA_MASK;
	ttcan_write32(ttcan, ADR_MTTCAN_TXEFC, txefc);
	ttcan->tx_config.evt_q_num = evf_size;
}

void ttcan_set_xtd_mask_add(struct ttcan_controller *ttcan, int extid_mask)
{
	u32 xidam_reg = 0;

	xidam_reg = MTT_XIDAM_EIDM_MASK & 0x1FFFFFFF;
	ttcan_write32(ttcan, ADR_MTTCAN_XIDAM, xidam_reg);
}

u32 ttcan_read_tx_complete_reg(struct ttcan_controller *ttcan)
{
	return ttcan_read32(ttcan, ADR_MTTCAN_TXBTO);
}

void ttcan_set_tx_cancel_request(struct ttcan_controller *ttcan, u32 txbcr)
{
	ttcan_write32(ttcan, ADR_MTTCAN_TXBCR, txbcr);
}

u32 ttcan_read_tx_cancelled_reg(struct ttcan_controller *ttcan)
{
	return ttcan_read32(ttcan, ADR_MTTCAN_TXBCF);
}

u32 ttcan_read_psr(struct ttcan_controller *ttcan)
{
	return ttcan_read32(ttcan, ADR_MTTCAN_PSR);
}

int ttcan_controller_init(struct ttcan_controller *ttcan, u32 irq_flag,
			  u32 tt_irq_flag)
{
	if (!ttcan) {
		pr_err("TTCAN controller NULL\n");
		return -EINVAL;
	}

	ttcan->intr_enable_reg = irq_flag;
	ttcan->intr_tt_enable_reg = tt_irq_flag;
	spin_lock_init(&ttcan->lock);
	return 0;
}
