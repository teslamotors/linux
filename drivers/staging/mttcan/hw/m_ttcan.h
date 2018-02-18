/*
 * Copyright (c) 2015-2017, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __M_TTCAN_DEF
#define __M_TTCAN_DEF

#include "m_ttcan_regdef.h"
#include "m_ttcan_linux.h"

/*      Message RAM config  */
/*	+-------------------+
 *	|   11 bit filter   |
 *	+-------------------+
 *	|   29 bit filter   |
 *	+-------------------+
 *	|   RX FIFO 0       |
 *	+-------------------+
 *	|   RX FIFO 1       |
 *	+-------------------+
 *	|   RX BUFFERS      |
 *	+-------------------+
 *	|   TX EVENT FIFO   |
 *	+-------------------+
 *	|   TX BUFFERS      |
 *	+-------------------+
 *	|   MEM TRIGG ELMTS |
 *	+-------------------+
 */

#define MAX_RXB_ELEM_SIZE	72
#define MAX_TXB_ELEM_SIZE	72
#define TX_EVENT_FIFO_ELEM_SIZE	8
#define SIDF_ELEM_SIZE		4
#define XIDF_ELEM_SIZE		8
#define TXB_ELEM_HEADER_SIZE	8
#define RXB_ELEM_HEADER_SIZE	8
#define TRIG_ELEM_SIZE		8


#define CAN_STD_ID 11
#define CAN_EXT_ID 29

#define CAN_STD_ID_MASK 0x000007FFU
#define CAN_EXT_ID_MASK 0x1FFFFFFFU
#define CAN_ERR_MASK 0x1FFFFFFFU

#define CAN_FMT 0x80000000U	/*1= EXT/ 0 = STD */
#define CAN_RTR 0x40000000U	/* RTR */
#define CAN_ERR 0x20000000U	/* ERR/Data  message frame */

#define CAN_BRS_MASK 0xFE
#define CAN_ESI_MASK 0xFD
#define CAN_FD_MASK  0xFB
#define CAN_DIR_MASK 0xF7

#define CAN_BRS_FLAG 0x01
#define CAN_ESI_FLAG 0x02
#define CAN_FD_FLAG  0x04
#define CAN_DIR_RX   0x08

#define MTTCAN_RAM_SIZE	4096
#define CAN_WORD_IN_BYTES 4

/* ISO 11898-1 */
#define CAN_MAX_DLC 8
#define CAN_MAX_DLEN 8

/* ISO 11898-7 */
#define CANFD_MAX_DLC 15
#define CANFD_MAX_DLEN 64

#define MAX_DATA_LEN 64
#define MAX_RX_ENTRIES 64
#define MAX_LEC 8

#define NUM_CAN_CONTROLLERS 2

/* Global Filter Confugration */
#define MTTCAN_GFC_ANFS_SHIFT	0x04
#define GFC_ANFS_RXFIFO_0	0x00
#define GFC_ANFS_RXFIFO_1	(0x01 << MTTCAN_GFC_ANFS_SHIFT)
#define GFC_ANFS_REJECT		(0x03 << MTTCAN_GFC_ANFS_SHIFT)

#define MTTCAN_GFC_ANFE_SHIFT	0x02
#define GFC_ANFE_RXFIFO_0	0x00
#define GFC_ANFE_RXFIFO_1	(0x01 << MTTCAN_GFC_ANFE_SHIFT)
#define GFC_ANFE_REJECT		(0x03 << MTTCAN_GFC_ANFE_SHIFT)

#define GFC_RRFS_REJECT		0x2
#define GFC_RRFE_REJECT		0x1

/* Last Error Code */
enum ttcan_lec_type {
	LEC_NO_ERROR = 0,
	LEC_STUFF_ERROR = 1,
	LEC_FORM_ERROR = 2,
	LEC_ACK_ERROR = 3,
	LEC_BIT1_ERROR = 4,
	LEC_BIT0_ERROR = 5,
	LEC_CRC_ERROR = 6,
	LEC_NO_CHANGE = 7,
};

/*Size of data in an element */
enum ttcan_data_field_size {
	BYTE8 = 0,
	BYTE12 = 1,
	BYTE16 = 2,
	BYTE20 = 3,
	BYTE24 = 4,
	BYTE32 = 5,
	BYTE48 = 6,
	BYTE64 = 7
};

enum ttcan_timestamp_source {
	TS_DISABLE = 0,
	TS_INTERNAL = 1,
	TS_EXTERNAL = 2,
	TS_DISABLE2 = 3
};

enum ttcan_rx_type {
	BUFFER = 1,
	FIFO_0 = 2,
	FIFO_1 = 4,
	TX_EVT = 8
};


enum ttcan_mram_item {
	MRAM_SIDF = 0,
	MRAM_XIDF,
	MRAM_RXF0,
	MRAM_RXF1,
	MRAM_RXB,
	MRAM_TXE,
	MRAM_TXB,
	MRAM_TMC,
	MRAM_ELEMS
};

enum ttcan_tx_conf {
	TX_CONF_TXB = 0,
	TX_CONF_TXQ,
	TX_CONF_QMODE,
	TX_CONF_BSIZE,
	TX_CONF_MAX
};

enum ttcan_rx_conf {
	RX_CONF_RXB = 0,
	RX_CONF_RXF0,
	RX_CONF_RXF1,
	RX_CONF_MAX
};


struct ttcan_mram_elem {
	u16 off;
	u16 num;
};
/* bit 0 - 28 : CAN identifier
 * bit 29 : type of frame (0 = data, 1 = error)
 * bit 30 : RTR
 * bit 31 : frame format type (0 = std, 1 = ext)
 */

/* bit 0 : 1 = BRS; 0 = Normal
 * bit 1 : 1 = Error Passive; 0 = Error Active
 * bit 2 : 1 = CAN FD; 0 = Normal
 * bit 4 : 1 = RX; 0 = Tx Direction
 */


struct ttcanfd_frame {
	u32 can_id;		/* FMT/RTR/ERR/ID */
	u8 d_len;		/* data length */
	u8 flags;		/* FD flags */
	u8 resv0;
	u8 resv1;
	u8 data[MAX_RX_ENTRIES] __aligned(8);
};

struct ttcan_element_size {
	u16 rx_fifo0;
	u16 rx_fifo1;
	u16 rx_buffer;
	u16 tx_buffer;
	u16 tx_fifo;
};

struct ttcan_bittiming {
	u32 bitrate;		/* Bit-rate in bits/second */
	u32 sampling_point;	/* Sampling point in one-tenth of a percent */
	u32 tq;		/* Time quanta in nenoseconds */
	u32 prop_seg;		/* Propogation segment in TQs */
	u32 phase_seg1;	/* Phase buffer segment 1 in TQ */
	u32 phase_seg2;	/* Phase buffer segment 2 in TQs */
	u32 sjw;		/* (re) synchronization jump width in TQs */
	u32 brp;		/* bit rate prescalar */
	u32 tdc;		/* transceiver delay comp. (1 is enable) */
	u32 tdc_offset;	/* transceiver delay comp. offset */
	u32 tdc_filter_window; /* transceiver delay comp. filter window */
};

struct can_bittiming_const_fd {
	u8 name[16];		/* Name of the CAN controller hardware */
	u32 tseg1_min;	/* Time segement 1 = prop_seg + phase_seg1 */
	u32 tseg1_max;
	u32 tseg2_min;	/* Time segement 2 = phase_seg2 */
	u32 tseg2_max;
	u32 sjw_max;		/* Synchronisation jump width */
	u32 brp_min;		/* Bit-rate prescaler */
	u32 brp_max;
	u32 brp_inc;
};

struct ttcan_bittiming_fd {
	struct ttcan_bittiming nominal;	/* Arb phase bit timing */
	struct ttcan_bittiming data;	/* Data phase bit timing */
	u32 fd_flags;		/* bit 0: FD; bit 1: BRS */
};

struct ttcan_txbuff_config {
	u32 fifo_q_num;
	u32 ded_buff_num;
	u32 evt_q_num;
	enum ttcan_data_field_size dfs;
	u32 flags;		/* bit 0: 0=Fifo, 1=Queue */
};

struct ttcan_rxbuff_config {
	u32 rxq0_dsize;
	u32 rxq1_dsize;
	u32 rxb_dsize;
	u64 rxq0_bmsk;
	u64 rxq1_bmsk;
	u64 rxb_bmsk;
};

struct ttcan_filter_config {
	u32 std_fltr_size;
	u32 xtd_fltr_size;
};

struct ttcan_rx_msg_list {
	struct ttcanfd_frame msg;
	struct list_head recv_list;
};

struct ttcan_txevt_msg_list {
	struct mttcan_tx_evt_element txevt;
	struct list_head txevt_list;
};

struct ttcan_controller {
	spinlock_t lock;
	struct ttcan_element_size e_size;
	struct ttcan_bittiming_fd bt_config;
	struct ttcan_txbuff_config tx_config;
	struct ttcan_rxbuff_config rx_config;
	struct ttcan_filter_config fltr_config;
	struct ttcan_mram_elem mram_cfg[MRAM_ELEMS];
	struct list_head rx_q0;
	struct list_head rx_q1;
	struct list_head rx_b;
	struct list_head tx_evt;
	void __iomem *base;	/* controller regs space should be remapped. */
	void __iomem *xbase;    /* extra registers are mapped */
	void __iomem *mram_vbase;
	size_t mram_base;
	unsigned long tx_object;
	unsigned long tx_obj_cancelled;
	u8 tx_buf_dlc[32];
	u64 ts_counter;
	u32 id;
	u32 proto_state;
	u32 intr_enable_reg;
	u32 intr_tt_enable_reg;
	u32 ts_prescalar;
	u32 tt_mem_elements;
	int rxq0_mem;
	int rxq1_mem;
	int rxb_mem;
	int evt_mem;
	u16 list_status;	/* bit 0: 1=Full; */
	u16 resv0;
};

static inline u8 ttcan_dlc2len(u8 dlc)
{
	return can_dlc2len(dlc);
}

static inline u8 ttcan_len2dlc(u8 len)
{
	if (len > 64)
		return 0xF;
	return can_len2dlc(len);
}

static inline enum ttcan_data_field_size
get_dfs(u32 bytes)
{
	switch (bytes) {
	case 8:
		return BYTE8;
	case 12:
		return BYTE12;
	case 16:
		return BYTE16;
	case 20:
		return BYTE20;
	case 24:
		return BYTE24;
	case 32:
		return BYTE32;
	case 48:
		return BYTE48;
	case 64:
		return BYTE64;
	default:
		return 0;
	}
}
static inline int data_in_element(
	enum ttcan_data_field_size dfs)
{
	switch (dfs) {
	case BYTE8:
		return 8;
	case BYTE12:
		return 12;
	case BYTE16:
		return 16;
	case BYTE20:
		return 20;
	case BYTE24:
		return 24;
	case BYTE32:
		return 32;
	case BYTE48:
		return 48;
	case BYTE64:
		return 64;
	default:
		return 0;
	}
}
static inline u32 ttcan_xread32(struct ttcan_controller *ttcan, int reg)
{
	return (u32) readl(ttcan->xbase + reg);
}

static inline u32 ttcan_read32(struct ttcan_controller *ttcan, int reg)
{
	return (u32) readl(ttcan->base + reg);
}

static inline void ttcan_xwrite32(struct ttcan_controller *ttcan,
	int reg, u32 val)
{
	writel(val, ttcan->xbase + reg);
}

static inline void ttcan_write32(struct ttcan_controller *ttcan,
	int reg, u32 val)
{
	writel(val, ttcan->base + reg);
}

static inline int ttcan_protected(u32 cccr_reg)
{
	if ((cccr_reg & 0x3) != 0x3) {
		pr_err("%s: protected\n", __func__);
		return -EPERM;
	}
	return 0;
}

void ttcan_print_version(struct ttcan_controller *ttcan);
int ttcan_write32_check(struct ttcan_controller *ttcan,
			int offset, u32 val, u32 mask);
void ttcan_set_ok(struct ttcan_controller *ttcan);
int ttcan_set_init(struct ttcan_controller *ttcan);
int ttcan_reset_init(struct ttcan_controller *ttcan);
int ttcan_set_power(struct ttcan_controller *ttcan, int value);
int ttcan_set_config_change_enable(struct ttcan_controller *ttcan);
void ttcan_reset_config_change_enable(struct ttcan_controller *ttcan);
int ttcan_set_baudrate(struct ttcan_controller *ttcan, int fdflags);

int ttcan_read_txevt_ram(struct ttcan_controller *ttcan,
	u32 read_addr, struct mttcan_tx_evt_element *txevt);
int ttcan_read_rx_msg_ram(struct ttcan_controller *ttcan,
			  u32 addr_in_msg_ram,
			  struct ttcanfd_frame *ttcanfd);
int ttcan_write_tx_msg_ram(struct ttcan_controller *ttcan,
			   u32 addr_in_msg_ram,
			   struct ttcanfd_frame *ttcanfd,
			   u8 index);

unsigned int ttcan_read_txevt_fifo(struct ttcan_controller *ttcan);

unsigned int ttcan_read_rx_fifo0(struct ttcan_controller *ttcan);
unsigned int ttcan_read_rx_fifo1(struct ttcan_controller *ttcan);
unsigned int ttcan_read_hp_mesgs(struct ttcan_controller *ttcan,
					struct ttcanfd_frame *ttcanfd);

void ttcan_set_rx_buffers_elements(struct ttcan_controller *ttcan);

int ttcan_set_tx_buffer_addr(struct ttcan_controller *ttcan);
int ttcan_tx_fifo_full(struct ttcan_controller *ttcan);
bool ttcan_tx_buffers_full(struct ttcan_controller *ttcan);

int ttcan_tx_fifo_queue_msg(struct ttcan_controller *ttcan,
			    struct ttcanfd_frame *ttcanfd);
int ttcan_tx_fifo_get_free_element(struct ttcan_controller *ttcan);

int ttcan_tx_buf_req_pending(struct ttcan_controller *ttcan, u8 index);
void ttcan_tx_ded_msg_write(struct ttcan_controller *ttcan,
			    struct ttcanfd_frame *ttcanfd,
			    u8 index);
void ttcan_tx_trigger_msg_transmit(struct ttcan_controller *ttcan, u8 index);
int ttcan_tx_msg_buffer_write(struct ttcan_controller *ttcan,
				struct ttcanfd_frame *ttcanfd);

void ttcan_prog_std_id_fltrs(struct ttcan_controller *ttcan, void *std_shadow);
void ttcan_set_std_id_filter(struct ttcan_controller *ttcan, void *std_shadow,
				int filter_index, u8 sft, u8 sfec, u32 sfid1,
				u32 sfid2);
u32 ttcan_get_std_id_filter(struct ttcan_controller *ttcan, int idx);

void ttcan_prog_xtd_id_fltrs(struct ttcan_controller *ttcan, void *xtd_shadow);
void ttcan_set_xtd_id_filter(struct ttcan_controller *ttcan, void *xtd_shadow,
				int filter_index, u8 eft, u8 efec, u32 efid1,
				u32 efid2);
u64 ttcan_get_xtd_id_filter(struct ttcan_controller *ttcan, int idx);

void ttcan_set_std_id_filter_addr(struct ttcan_controller *ttcan);
void ttcan_set_xtd_id_filter_addr(struct ttcan_controller *ttcan);
int ttcan_set_gfc(struct ttcan_controller *ttcan, u32 regval);
u32 ttcan_get_gfc(struct ttcan_controller *ttcan);

int ttcan_set_xidam(struct ttcan_controller *ttcan, u32 regval);
u32 ttcan_get_xidam(struct ttcan_controller *ttcan);

void ttcan_set_time_stamp_conf(struct ttcan_controller *ttcan,
				u16 timer_prescalar,
				enum ttcan_timestamp_source time_type);
void ttcan_set_txevt_fifo_conf(struct ttcan_controller *ttcan);
/* Mesg RAM partition */
int ttcan_mesg_ram_config(struct ttcan_controller *ttcan,
		u32 *arr, u32 *tx_conf , u32 *rx_conf);
int ttcan_controller_init(struct ttcan_controller *ttcan, u32 irq_flag,
	u32 tt_irq_flag);

u32 ttcan_read_ecr(struct ttcan_controller *ttcan);
u32 ttcan_read_tx_complete_reg(struct ttcan_controller *ttcan);
void ttcan_set_tx_cancel_request(struct ttcan_controller *ttcan, u32 txbcr);
u32 ttcan_read_tx_cancelled_reg(struct ttcan_controller *ttcan);
u32 ttcan_read_psr(struct ttcan_controller *ttcan);
int ttcan_read_rx_buffer(struct ttcan_controller *ttcan);
int ttcan_set_bitrate(struct ttcan_controller *ttcan);

void ttcan_disable_auto_retransmission(
		struct ttcan_controller *ttcan,
		bool enable);
int ttcan_set_bus_monitoring_mode(struct ttcan_controller *ttcan, bool enable);
int ttcan_set_loopback(struct ttcan_controller *ttcan);
int ttcan_set_normal_mode(struct ttcan_controller *ttcan);

/* Interrupt APIs */
void ttcan_clear_intr(struct ttcan_controller *ttcan);
void ttcan_clear_tt_intr(struct ttcan_controller *ttcan);
void ttcan_ir_write(struct ttcan_controller *ttcan, u32 value);
void ttcan_ttir_write(struct ttcan_controller *ttcan, u32 value);
u32 ttcan_read_ir(struct ttcan_controller *ttcan);
u32 ttcan_read_ttir(struct ttcan_controller *ttcan);
void ttcan_set_intrpts(struct ttcan_controller *ttcan, int enable);

/* TTCAN APIS */
void ttcan_set_trigger_mem_conf(struct ttcan_controller *ttcan);
int ttcan_set_ttrmc(struct ttcan_controller *ttcan, u32 regval);
u32 ttcan_get_ttrmc(struct ttcan_controller *ttcan);

void ttcan_set_tt_config(struct ttcan_controller *ttcan, u32 evtp,
	u32 ecc, u32 egtf, u32 awl, u32 eecs,
	u32 irto, u32 ldsdl, u32 tm, u32 gen, u32 om);
void ttcan_set_ttocf(struct ttcan_controller *ttcan, u32 value);
u32 ttcan_get_ttocf(struct ttcan_controller *ttcan);
void ttcan_set_ttmlm(struct ttcan_controller *ttcan, u32 value);
u32 ttcan_get_ttmlm(struct ttcan_controller *ttcan);
void ttcan_set_tttmc(struct ttcan_controller *ttcan, u32 value);
u32 ttcan_get_tttmc(struct ttcan_controller *ttcan);
u32 ttcan_get_cccr(struct ttcan_controller *ttcan);
void ttcan_set_txbar(struct ttcan_controller *ttcan, u32 value);
u32 ttcan_get_ttost(struct ttcan_controller *ttcan);
int ttcan_set_trigger_mem(struct ttcan_controller *ttcan, void *tmc_shadow,
	int trig_index,	u16 time_mark, u16 cycle_code, u8 tmin, u8 tmex,
	u16 trig_type, u8 filter_type, u8 mesg_num);
u64 ttcan_get_trigger_mem(struct ttcan_controller *ttcan, int idx);


void ttcan_set_ref_mesg(struct ttcan_controller *ttcan, u32 id,
	u32 rmps, u32 xtd);

int ttcan_set_matrix_limits(struct ttcan_controller *ttcan,
	u32 entt, u32 txew, u32 css, u32 ccm);

int ttcan_set_tur_config(struct ttcan_controller *ttcan, u16 denominator,
	u16 numerator, int local_timing_enable);

void ttcan_prog_trigger_mem(struct ttcan_controller *ttcan, void *tmc_shadow);

/* list APIs */
int add_msg_controller_list(struct ttcan_controller *ttcan,
	struct ttcanfd_frame *ttcanfd, struct list_head *rx_q,
	enum ttcan_rx_type rxtype);

int add_event_controller_list(struct ttcan_controller *ttcan,
				struct mttcan_tx_evt_element *txevt,
				struct list_head *evt_q);
#endif
