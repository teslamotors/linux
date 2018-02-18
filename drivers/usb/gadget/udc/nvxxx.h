/*
* nvxxx.h - Nvidia device mode implementation
*
* Copyright (c) 2013-2016, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/ioctl.h>
#ifdef CONFIG_TEGRA_GADGET_BOOST_CPU_FREQ
#include <linux/pm_qos.h>
#endif

#define NON_STD_CHARGER_DET_TIME_MS 1000
#define USB_ANDROID_SUSPEND_CURRENT_MA 2

/* VENDOR ID */
#define XUDC_VENDOR_ID		0x10de

/* DEVICE ID */
#define XUDC_DEVICE_ID_T210	0x0fad
#define XUDC_DEVICE_ID_T186	0x10e2

#define XUDC_IS_T210(t)							\
	(t->soc ? (t->soc->device_id == XUDC_DEVICE_ID_T210) :		\
	(t->pdev.pci ? (t->pdev.pci->device == XUDC_DEVICE_ID_T210) :	\
	false))

#define XUDC_IS_T186(t)							\
	(t->soc ? (t->soc->device_id == XUDC_DEVICE_ID_T186) :		\
	(t->pdev.pci ? (t->pdev.pci->device == XUDC_DEVICE_ID_T186) :	\
	false))

/*
 * Register definitions
 */

/* Offsets */
#define SPARAM		0x00000000
#define SPARAM_ERSTMAX(x)		(((x) & 0x1f) << 16)

#define DB		0x00000004
#define DB_TARGET(x)	(((x) & 0xff) << 8)
#define DB_STREAMID(x)	(((x) & 0xffff) << 16)

#define ERSTSZ		0x00000008
#define ERSTSZ_ERST0SZ(x)	((x) & 0xffff)
#define ERSTSZ_ERST1SZ(x)	(((x) & 0xffff) << 16)

#define ERST0BALO	0x00000010
#define ERST0BALO_ADDRLO(x)	((x) & 0xfffffff0)

#define ERST0BAHI	0x00000014

#define ERST1BALO	0x00000018
#define ERST1BALO_ADDRLO(x)	((x) & 0xfffffff0)

#define ERST1BAHI	0x0000001c

#define ERDPLO		0x00000020
#define ERDPLO_EHB		(1 << 3)
#define ERDPLO_ADDRLO(x)	((x) & 0xfffffff0)

#define ERDPHI		0x00000024

#define EREPLO		0x00000028
#define EREPLO_ECS	1
#define EREPLO_SEGI	(1 << 1)
#define EREPLO_ADDRLO(x)	((x) & 0xfffffff0)

#define EREPHI		0x0000002C

#define CTRL		0x00000030
#define CTRL_RUN	1
#define CTRL_LSE	(1 << 1)
#define CTRL_IE		(1 << 4)
#define CTRL_SMI_EVT	(1 << 5)
#define CTRL_SMI_DSE	(1 << 6)
#define CTRL_EWE	(1 << 7)
#define CTRL_DEVADDR(x)	(((x) & 0x7f) << 24)
#define CTRL_ENABLE	(1 << 31)

#define ST		0x00000034
#define ST_RC	1
#define ST_IP	(1 << 4)
#define ST_DSE	(1 << 5)

#define RT_IMOD	0x00000038
#define RT_IMOD_IMODI(x)	((x) & 0xffff)
#define RT_IMOD_IMODC(x)	(((x) & 0xffff) << 16)

#define PORTSC	0x0000003C
#define PORTSC_CCS	1
#define PORTSC_PED	(1 << 1)
#define PORTSC_LANE_POLARITY_OVRD	(1 << 2)
#define PORTSC_LANE_POLARITY_OVRD_VALUE	(1 << 3)
#define PORTSC_PR	(1 << 4)
#define PORTSC_PLS(x)	(((x) & 0xf) << 5)
#define PORTSC_LANE_POLARITY_VALUE (1 << 9)
#define PORTSC_PS(x)	(((x) & 0xf) << 10)
#define PORTSC_PS_MASK	0xf
#define PORTSC_PS_SHIFT	10
#define PORTSC_LWS	(1 << 16)
#define PORTSC_CSC	(1 << 17)
#define PORTSC_WRC	(1 << 19)
#define PORTSC_PRC	(1 << 21)
#define PORTSC_PLC	(1 << 22)
#define PORTSC_CEC	(1 << 23)
#define PORTSC_WPR	(1 << 30)

#define PORTSC_CHANGE_MASK (PORTSC_CSC | PORTSC_WRC | PORTSC_PRC | \
			     PORTSC_PLC | PORTSC_CEC)

#define PORTSC_PLS_MASK	(0xf << 5)
#define XDEV_U0		(0x0 << 5)
#define XDEV_U2		(0x2 << 5)
#define XDEV_U3		(0x3 << 5)
#define XDEV_DISABLED	(0x4 << 5)
#define XDEV_INACTIVE	(0x6 << 5)
#define XDEV_RESUME	(0xf << 5)

#define ECPLO		0x00000040
#define ECPLO_ADDRLO(x)	((x) & 0xffffffc0)

#define ECPHI		0x00000044

#define MFINDEX	0x00000048
#define MFINDEX_UFRAME(x)	((x) & 0x7)
#define MFINDEX_FRAME(x)	(((x) & 0x7ff) < 3)
#define MFINDEX_FRAME_SHIFT	3

#define PORTPM		0x0000004C
#define PORTPM_L1S(x)	((x) & 0x3)
#define PORTPM_RWE		(1 << 3)
#define PORTPM_RWE_SHIFT	3
#define PORTPM_HIRD(x)	(((x) & 0xf) << 4)
#define PORTPM_U2TIMEOUT(x)	(((x) & 0xff) << 8)
#define PORTPM_U1TIMEOUT(x)	(((x) & 0xff) << 16)
#define PORTPM_FLA	(1 << 24)
#define PORTPM_VBA	(1 << 25)
#define PORTPM_WOC	(1 << 26)
#define PORTPM_WOD	(1 << 27)
#define PORTPM_U1E	(1 << 28)
#define PORTPM_U1E_SHIFT	28
#define PORTPM_U2E	(1 << 29)
#define PORTPM_U2E_SHIFT	29
#define PORTPM_FRWE	(1 << 30)
#define PORTPM_PNG_CYA	(1 << 31)

#define EP_HALT	0x00000050

#define EP_PAUSE	0x00000054

#define EP_RELOAD	0x00000058

#define EP_STCHG	0x0000005C

#define FLOWCNTRL	0x00000060
#define FLOWCNTRL_IN_THRESH(x)	((x) & 0x7f)
#define FLOWCNTRL_IN_EN	(1 << 7)
#define FLOWCNTRL_OUT_THRESH(x)	(((x) & 0x7f) << 8)
#define FLOWCNTRL_OUT_EN	(1 << 15)
#define FLOWCNTRL_IDLE_MITS(x)	(((x) & 0xff) << 16)

#define DEVNOTIF_LO	0x00000064
#define DEVNOTIF_LO_TRIG	1
#define DEVNOTIF_LO_TYPE(x)	(((x) & 0xf) << 4)
#define DEVNOTIF_LO_DATA(x)	(((x) & 0xffffff) << 8)

#define DEVNOTIF_HI	0x00000068

#define PORTHALT	0x0000006c
#define PORTHALT_HALT_LTSSM		1
#define PORTHALT_HALT_REJECT		(1 << 1)
#define PORTHALT_STCHG_STATE(x)	(((x) & 0xf) << 16)
#define PORTHALT_STCHG_REQ	(1 << 20)
#define PORTHALT_STCHG_INTR_EN	(1 << 24)
#define PORTHALT_PME_EN	(1 << 25)
#define PORTHALT_SMI_EN	(1 << 26)

#define PORT_TM	0x00000070
#define PORT_TM_CTRL(x)	((x) & 0xf)

#define EP_THREAD_ACTIVE	0x00000074
#define EP_STOPPED	0x00000078

#define HSFSPI_TESTMODE_CTRL        0x0000013C

#define BLCG			0x00000840
#define BLCG_DFPCI		(1 << 0)
#define BLCG_UFPCI		(1 << 1)
#define BLCG_FE			(1 << 2)
#define BLCG_CORE_BI		(1 << 3)
#define BLCG_PICLK_BI		(1 << 4)
#define BLCG_HSFS_PI		(1 << 5)
#define BLCG_NVWRAP_480M	(1 << 6)
#define BLCG_NVWRAP_48M		(1 << 7)
#define BLCG_COREPLL_PWRDN	(1 << 8)
#define BLCG_IOPLL_0_PWRDN	(1 << 9)
#define BLCG_IOPLL_1_PWRDN	(1 << 10)
#define BLCG_IOPLL_2_PWRDN	(1 << 11)
#define BLCG_IOPLL_3_PWRDN	(1 << 12)
#define BLCG_SS_PI		(1 << 13)
#define BLCG_SS_PI_500M		(1 << 14)
#define BLCG_ALL \
	(BLCG_DFPCI | BLCG_UFPCI | BLCG_FE | BLCG_CORE_BI | BLCG_PICLK_BI | \
	 BLCG_HSFS_PI | BLCG_NVWRAP_480M | BLCG_NVWRAP_48M | BLCG_COREPLL_PWRDN | \
	 BLCG_IOPLL_0_PWRDN | BLCG_IOPLL_1_PWRDN | BLCG_IOPLL_2_PWRDN | \
	 BLCG_IOPLL_3_PWRDN | BLCG_SS_PI | BLCG_SS_PI_500M)

#define CFG_DEV_FE                  0x0000085C
#define CFG_DEV_FE_PORTREGSEL(x)	((x) & 0x3)
#define CFG_DEV_FE_INFINITE_SS_RETRY	(1 << 29)

#define CFG_DEV_SSPI_XFER           0x00000858
#define CFG_DEV_SSPI_XFER_ACKTIMEOUT_SHIFT 0
#define CFG_DEV_SSPI_XFER_ACKTIMEOUT_MASK  0xffffffff

#define NV_BIT(bit_name) (1 << bit_name)

#define XHCI_SETF_VAR(field, var, fieldval) \
	(var = (((var) & ~(field ## _MASK)) | \
			(((fieldval) << field ## _SHIFT) & (field ## _MASK))))

#define XHCI_GETF(field, val) \
		(((val) & (field ## _MASK)) >> (field ## _SHIFT))

#define USB_REQ_SET_ISOCH_DEALY     49

#define PRIME_NOT_RCVD_WAR
	/*--------------------------------------------*/
	/*---------global variables-------------------*/
	/*--------------------------------------------*/

enum EP_STATE_E {
	EP_STATE_DISABLED = 0,
	EP_STATE_RUNNING,
};

enum EP_TYPE_E {
	EP_TYPE_INVALID = 0,
	EP_TYPE_ISOCH_OUT,
	EP_TYPE_BULK_OUT,
	EP_TYPE_INTR_OUT,
	EP_TYPE_CNTRL,
	EP_TYPE_ISOCH_IN,
	EP_TYPE_BULK_IN,
	EP_TYPE_INTR_IN
};

/*Frome Table 128 of XHCI spec. */
enum TRB_TYPE_E {
	TRB_TYPE_RSVD = 0,
	TRB_TYPE_XFER_NORMAL,
	TRB_TYPE_XFER_SETUP_STAGE,
	TRB_TYPE_XFER_DATA_STAGE,
	TRB_TYPE_XFER_STATUS_STAGE,
	TRB_TYPE_XFER_DATA_ISOCH,   /* 5*/
	TRB_TYPE_LINK,
	TRB_TYPE_EVT_TRANSFER = 32,
	TRB_TYPE_EVT_PORT_STATUS_CHANGE = 34,
	TRB_TYPE_XFER_STREAM = 48,
	TRB_TYPE_EVT_SETUP_PKT = 63,
};

/*Table 127*/
enum TRB_CMPL_CODES_E {
	CMPL_CODE_INVALID       = 0,
	CMPL_CODE_SUCCESS,
	CMPL_CODE_DATA_BUFFER_ERR,
	CMPL_CODE_BABBLE_DETECTED_ERR,
	CMPL_CODE_USB_TRANS_ERR,
	CMPL_CODE_TRB_ERR,  /*5*/
	CMPL_CODE_TRB_STALL,
	CMPL_CODE_INVALID_STREAM_TYPE_ERR = 10,
	CMPL_CODE_SHORT_PKT = 13,
	CMPL_CODE_RING_UNDERRUN,
	CMPL_CODE_RING_OVERRUN, /*15*/
	CMPL_CODE_EVENT_RING_FULL_ERR = 21,
	CMPL_CODE_STOPPED = 26,
	CMPL_CODE_ISOCH_BUFFER_OVERRUN = 31,
	/*192-224 vendor defined error*/
	CMPL_CODE_STREAM_NUMP_ERROR = 219,
	CMPL_CODE_PRIME_PIPE_RECEIVED,
	CMPL_CODE_HOST_REJECTED,
	CMPL_CODE_CTRL_DIR_ERR,
	CMPL_CODE_CTRL_SEQNUM_ERR,
};


/* Endpoint Context */
struct ep_cx_s {
#define EP_CX_EP_STATE_MASK         0x00000007
#define EP_CX_EP_STATE_SHIFT                 0
#define EP_CX_MULT_MASK             0x00000300
#define EP_CX_MULT_SHIFT                     8
#define EP_CX_MAX_PSTREAMS_MASK     0x00007C00
#define EP_CX_MAX_PSTREAMS_SHIFT            10
#define EP_LINEAR_STRM_ARR_MASK     0x00008000
#define EP_LINEAR_STRM_ARR_SHIFT            15
#define EP_CX_INTERVAL_MASK         0x00FF0000
#define EP_CX_INTERVAL_SHIFT                16
	__le32 ep_dw0;

#define EP_CX_CERR_MASK             0x00000006
#define EP_CX_CERR_SHIFT                     1
#define EP_CX_EP_TYPE_MASK          0x00000038
#define EP_CX_EP_TYPE_SHIFT                  3
#define EP_CX_HOST_INIT_DISABLE_MASK 0x00000080
#define EP_CX_HOST_INIT_DISABLE_SHIFT        7
#define EP_CX_MAX_BURST_SIZE_MASK   0x0000FF00
#define EP_CX_MAX_BURST_SIZE_SHIFT           8
#define EP_CX_MAX_PACKET_SIZE_MASK  0xFFFF0000
#define EP_CX_MAX_PACKET_SIZE_SHIFT         16
	__le32 ep_dw1;

#define EP_CX_DEQ_CYC_STATE_MASK    0x00000001
#define EP_CX_DEQ_CYC_STATE_SHIFT            0
#define EP_CX_TR_DQPT_LO_MASK       0xFFFFFFF0
#define EP_CX_TR_DQPT_LO_SHIFT               4
	__le32 ep_dw2;

#define EP_CX_TR_DQPT_HI_MASK       0xFFFFFFFF
#define EP_CX_TR_DQPT_HI_SHIFT               0
	__le32 ep_dw3;

#define EP_CX_AVE_TRB_LEN_MASK      0x0000FFFF
#define EP_CX_AVE_TRB_LEN_SHIFT              0
#define EP_CX_MAX_ESIT_PAYLOAD_MASK 0xFFFF0000
#define EP_CX_MAX_ESIT_PAYLOAD_SHIFT        16
	__le32 ep_dw4;

#define EP_CX_EDTLA_MASK            0x00FFFFFF
#define EP_CX_EDTLA_SHIFT                    0
#define EP_CX_PARTIALTD_MASK        0x02000000
#define EP_CX_PARTIALTD_SHIFT               25
#define EP_CX_SPLITXSTATE_MASK      0x04000000
#define EP_CX_SPLITXSTATE_SHIFT             26
#define EP_CX_SEQ_NUM_MASK          0xFF000000
#define EP_CX_SEQ_NUM_SHIFT                 24
	__le32 ep_dw5;

#define EP_CX_CPROG_MASK       0x000000ff
#define EP_CX_CPROG_SHIFT               0
#define EP_CX_SBYTE_MASK       0x00007f00
#define EP_CX_SBYTE_SHIFT               8
#define EP_CX_TP_MASK          0x00018000
#define EP_CX_TP_SHIFT                 15
#define EP_CX_REC_MASK         0x00020000
#define EP_CX_REC_SHIFT                17
#define EP_CX_CERRCNT_MASK     0x000c0000
#define EP_CX_CERRCNT_SHIFT            18
#define EP_CX_CED_MASK         0x00100000
#define EP_CX_CED_SHIFT                20
#define EP_CX_HSP_MASK         0x00200000
#define EP_CX_HSP_SHIFT                21
#define EP_CX_RTY_MASK         0x00400000
#define EP_CX_RTY_SHIFT                22
#define EP_CX_STD_MASK              0x00800000
#define EP_CX_STD_SHIFT                     23
#define EP_CX_EP_STATUS_MASK        0xff000000
#define EP_CX_EP_STATUS_SHIFT               24
	__le32 ep_dw6;

#define EP_CX_DATA_OFFSET_MASK      0x0000ffff
#define EP_CX_DATA_OFFSET_SHIFT              0
#define EP_CX_LPA_MASK              0x00200000
#define EP_CX_LPA_SHIFT                     21
#define EP_CX_NUMTRBS_MASK          0x07c00000
#define EP_CX_NUMTRBS_SHIFT                 22
#define EP_CX_NUMP_MASK             0xf8000000
#define EP_CX_NUMP_SHIFT                    27
	__le32 ep_dw7;

	__le32 ep_dw8;
	__le32 ep_dw9;

#define EP_CX_CPING_MASK            0x000000ff
#define EP_CX_CPING_SHIFT                    0
#define EP_CX_SPING_MASK            0x0000ff00
#define EP_CX_SPING_SHIFT                   8
#define EP_CX_TC_MASK               0x00030000
#define EP_CX_TC_SHIFT                      16
#define EP_CX_NS_MASK               0x00040000
#define EP_CX_NS_SHIFT                      18
#define EP_CX_RO_MASK               0x00080000
#define EP_CX_RO_SHIFT                      19
#define EP_CX_TLM_MASK              0x00100000
#define EP_CX_TLM_SHIFT                     20
#define EP_CX_DLM_MASK              0x00200000
#define EP_CX_DLM_SHIFT                     21
#define EP_CX_SRR_MASK              0xff000000
#define EP_CX_SRR_SHIFT                     24
	__le32 ep_dw10;

#define EP_CX_DEVADDR_MASK          0x0000007f
#define EP_CX_DEVADDR_SHIFT                  0
#define EP_CX_HUBADDR_MASK          0x0000ff00
#define EP_CX_HUBADDR_SHIFT                  8
#define EP_CX_RPORTNUM_MASK         0x00ff0000
#define EP_CX_RPORTNUM_SHIFT                16
#define EP_CX_SLOTID_MASK           0xff000000
#define EP_CX_SLOTID_SHIFT                  24
	__le32 ep_dw11;

#define EP_CX_RSTRING_MASK          0x000fffff
#define EP_CX_RSTRING_SHIFT                  0
#define EP_CX_SPEED_MASK            0x00f00000
#define EP_CX_SPEED_SHIFT                   20
#define EP_CX_LPU_MASK              0x01000000
#define EP_CX_LPU_SHIFT                     24
#define EP_CX_MTT_MASK              0x02000000
#define EP_CX_MTT_SHIFT                     25
#define EP_CX_HUB_MASK              0x04000000
#define EP_CX_HUB_SHIFT                     26
#define EP_CX_DCI_MASK              0xf8000000
#define EP_CX_DCI_SHIFT                     27
	__le32 ep_dw12;

#define EP_CX_TTHSLOTID_MASK        0x000000ff
#define EP_CX_TTHSLOTID_SHIFT                0
#define EP_CX_TTPORTNUM_MASK        0x0000ff00
#define EP_CX_TTPORTNUM_SHIFT                8
#define EP_CX_SSF_MASK              0x000f0000
#define EP_CX_SSF_SHIFT                     16
#define EP_CX_SPS_MASK              0x00300000
#define EP_CX_SPS_SHIFT                     20
#define EP_CX_INTRTGT_MASK          0xffc00000
#define EP_CX_INTRTGT_SHIFT                 22
	__le32 ep_dw13;

#define EP_CX_FRZ_MASK         0x00000001
#define EP_CX_FRZ_SHIFT                 0
#define EP_CX_END_MASK         0x00000002
#define EP_CX_END_SHIFT                 1
#define EP_CX_ELM_MASK         0x00000004
#define EP_CX_ELM_SHIFT                 2
#define EP_CX_MRK_MASK         0x00000008
#define EP_CX_MRK_SHIFT                 3
#define EP_CX_LINKLO_MASK      0xfffffff0
#define EP_CX_LINKLO_SHIFT              4
	__le32 ep_dw14;

	__le32 ep_dw15;
};


/* Transfer TRBs*/
struct transfer_trb_s {
	__le32   data_buf_ptr_lo;
	__le32   data_buf_ptr_hi;

#define TRB_TRANSFER_LEN_MASK       0x0001FFFF
#define TRB_TRANSFER_LEN_SHIFT               0
#define TRB_TD_SIZE_MASK            0x003E0000
#define TRB_TD_SIZE_SHIFT                   17
#define TRB_INTR_TARGET_MASK        0xFFC00000
#define TRB_INTR_TARGET_SHIFT               22

	__le32   trb_dword2;

#define TRB_CYCLE_BIT_MASK          0x00000001
#define TRB_CYCLE_BIT_SHIFT                  0
#define TRB_LINK_TOGGLE_CYCLE_MASK  0x00000002
#define TRB_LINK_TOGGLE_CYCLE_SHIFT          1
#define TRB_INTR_ON_SHORT_PKT_MASK  0x00000004
#define TRB_INTR_ON_SHORT_PKT_SHIFT          2
#define TRB_NO_SNOOP_MASK           0x00000008
#define TRB_NO_SNOOP_SHIFT                   3
#define TRB_CHAIN_BIT_MASK          0x00000010
#define TRB_CHAIN_BIT_SHIFT                  4
#define TRB_INTR_ON_COMPLETION_MASK 0x00000020
#define TRB_INTR_ON_COMPLETION_SHIFT         5
#define TRB_IMMEDIATE_DATA_MASK     0x00000040
#define TRB_IMMEDIATE_DATA_SHIFT             6
#define ISOC_TRB_TRAN_BURST_CT_MASK 0x00000180
#define ISOC_TRB_TRAN_BURST_CT_SHIFT         7
#define TRB_TYPE_MASK               0x0000FC00
#define TRB_TYPE_SHIFT                      10
#define DATA_STAGE_TRB_DIR_MASK     0x00010000
#define DATA_STAGE_TRB_DIR_SHIFT            16
#define ISOC_TRB_TLBPC_MASK         0x000F0000
#define ISOC_TRB_TLBPC_SHIFT                16
#define ISOC_TRB_FRAME_ID_MASK      0x7FF00000
#define ISOC_TRB_FRAME_ID_SHIFT             20
#define STREAM_TRB_STREAM_ID_MASK   0xFFFF0000
#define STREAM_TRB_STREAM_ID_SHIFT	    16
#define ISOC_TRB_SIA_MASK           0x80000000
#define ISOC_TRB_SIA_SHIFT                  31

	__le32 trb_dword3;
};


/*Event TRBs*/
struct event_trb_s {
	u32 trb_pointer_lo;
	u32 trb_pointer_hi;

#define EVE_TRB_TRAN_LEN_MASK       0x00FFFFFF
#define EVE_TRB_TRAN_LEN_SHIFT               0
#define EVE_TRB_SEQ_NUM_MASK        0x0000FFFF
#define EVE_TRB_SEQ_NUM_SHIFT                0
#define EVE_TRB_COMPL_CODE_MASK     0xFF000000
#define EVE_TRB_COMPL_CODE_SHIFT            24
	u32 eve_trb_dword2;

#define EVE_TRB_CYCLE_BIT_MASK      0x00000001
#define EVE_TRB_CYCLE_BIT_SHIFT              0
#define EVE_TRB_EVENT_DATA_MASK     0x00000004
#define EVE_TRB_EVENT_DATA_SHIFT             2
#define EVE_TRB_TYPE_MASK           0x0000FC00
#define EVE_TRB_TYPE_SHIFT                  10
#define EVE_TRB_ENDPOINT_ID_MASK    0x001F0000
#define EVE_TRB_ENDPOINT_ID_SHIFT           16
	u32 eve_trb_dword3;
};

struct nv_udc_request {
	struct usb_request usb_req;
	struct list_head queue;
	bool mapped;
	u64 buff_len_left;
	u32 trbs_needed;
	struct transfer_trb_s *first_trb;
	struct transfer_trb_s *last_trb;
	bool all_trbs_queued;
	bool short_pkt;
	bool need_zlp;	/* only used by ctrl ep */
};

struct nv_setup_packet {
	struct usb_ctrlrequest usbctrlreq;
	u16 seqnum;
};

struct nv_buffer_info_s {
	void *vaddr;
	dma_addr_t dma;
	u32 len;
};

struct nv_udc_ep {
	struct usb_ep usb_ep;
#ifdef PRIME_NOT_RCVD_WAR
	struct delayed_work work;
#endif
	struct nv_buffer_info_s tran_ring_info;
	union {
		struct transfer_trb_s *tran_ring_ptr;
		struct transfer_trb_s *first_trb;
	};
	union {
		struct transfer_trb_s *link_trb;
		struct transfer_trb_s *last_trb;
	};
	struct transfer_trb_s *enq_pt;
	struct transfer_trb_s *deq_pt;
	u8 pcs;
#ifdef PRIME_NOT_RCVD_WAR
	u8 stream_rejected_retry_count;
#endif
	char name[10];
	u8 DCI;
	struct list_head queue;
	const struct usb_endpoint_descriptor *desc;
	const struct usb_ss_ep_comp_descriptor *comp_desc;
	bool tran_ring_full;
	struct nv_udc_s *nvudc;
};

struct sel_value_s {
	u16 u2_pel_value;
	u16 u2_sel_value;
	u8 u1_pel_value;
	u8 u1_sel_value;
};

struct mmio_reg_s {
	u32 device_address;
	u32 portsc;
	u32 portpm;
	u32 ereplo;
	u32 erephi;
	u32 ctrl;
};

struct xudc_board_data {
	u32 ss_portmap;
	u32 lane_owner;
	u32 otg_portmap;
};

struct nv_udc_s {
	struct usb_gadget gadget;
	struct usb_gadget_driver *driver;
	union {
		struct pci_dev *pci;
		struct platform_device *plat;
	} pdev;
	struct device *dev; /* a shortcut to pdev.[pci/plat]->dev */
	struct tegra_xudc_soc_data *soc;

	struct usb_phy *phy;
	struct nv_udc_ep udc_ep[32];
	u32 irq;
	u32 padctl_irq;
	struct nv_buffer_info_s ep_cx;
	struct ep_cx_s *p_epcx;
	resource_size_t	mmio_phys_len;
	resource_size_t	mmio_phys_base;
	void __iomem *mmio_reg_base;

	struct nv_buffer_info_s event_ring0;
	struct nv_buffer_info_s event_ring1;
	struct event_trb_s *evt_dq_pt;
	u8 CCS;
	bool registered;
	bool enabled;
	bool binded;
	bool pullup;
	bool is_elpg;
	bool is_suspended;
	bool extcon_event_processing;
	bool elpg_is_processing;
	u32 act_bulk_ep;
	u32 num_enabled_eps;
	u32 g_isoc_eps;
	struct nv_udc_request *status_req;
	struct sel_value_s sel_value;
	u16 statusbuf;

#define WAIT_FOR_SETUP      0
#define SETUP_PKT_PROCESS_IN_PROGRESS 1
#define DATA_STAGE_XFER     2
#define DATA_STAGE_RECV     3
#define STATUS_STAGE_XFER   4
#define STATUS_STAGE_RECV   5
	u8 setup_status;
	u8 device_state;
	u8 resume_state;
	u16 ctrl_seq_num;
	u32 ss_port;
	u32 hs_port;
	bool is_ss_port_active;
	spinlock_t lock;
	struct mutex elpg_lock;
	struct event_trb_s *evt_seg0_last_trb;
	struct event_trb_s *evt_seg1_last_trb;
	u32 dbg_cnt1;
	u32 dbg_cnt2;
	u32 dbg_cnt3;
	u32 dbg_cnt4;
#define CTRL_REQ_QUEUE_DEPTH  5
	struct nv_setup_packet ctrl_req_queue[CTRL_REQ_QUEUE_DEPTH];
	u8    ctrl_req_enq_idx;
	void (*setup_fn_call_back) (struct nv_udc_s *);
	u16   dev_addr;
	u32 num_evts_processed;
	u32 iso_delay;
	u32 stream_rejected;
	u32 set_tm;
	struct mmio_reg_s mmio_reg;

	/* mmio regions */
	void __iomem *base;
	void __iomem *ipfs;
	void __iomem *fpci;
	void __iomem *padctl;

	/* clocks */
	struct clk *pll_u_480M;
	struct clk *pll_e;
	struct clk *dev_clk;
	struct clk *ss_clk;

	/* regulators */
	struct regulator_bulk_data *supplies;
	struct xudc_board_data bdata;

	/* extcon */
	bool vbus_detected;
	struct extcon_dev *vbus_extcon_dev;
	struct notifier_block vbus_extcon_nb;

	/* charger detection */
	struct tegra_usb_cd *ucd;
	enum tegra_usb_connect_type connect_type;
	struct work_struct ucd_work;
	struct work_struct current_work;
	struct delayed_work non_std_charger_work;
	struct delayed_work port_reset_war_work;
	struct delayed_work plc_reset_war_work;
	bool wait_for_sec_prc;
	bool wait_for_csc;
	u32 current_ma;

	struct tegra_prod_list *prod_list;
	void __iomem *base_list[4];
#ifdef CONFIG_TEGRA_GADGET_BOOST_CPU_FREQ
	struct mutex boost_cpufreq_lock;
	struct pm_qos_request boost_cpufreq_req;
	struct work_struct boost_cpufreq_work;
	struct delayed_work restore_cpufreq_work;
	unsigned long cpufreq_last_boosted;
	bool cpufreq_boosted;
	bool restore_cpufreq_scheduled;
#endif

	struct phy *usb3_phy;
	struct phy *usb2_phy;
};

void free_data_struct(struct nv_udc_s *nvudc);
u32 reset_data_struct(struct nv_udc_s *nvudc);
void nvudc_handle_event(struct nv_udc_s *nvudc, struct event_trb_s *event);
/*
 * macro to poll STCHG reg for expected value.  If we exceed polling limit,
 * a console message is printed.  _fmt must be enclosed * in ""
 * as it is string identifier into dev_dbg().
 */
#define STATECHG_REG_TIMEOUT_IN_USECS 100
#define poll_stchg(dev, _fmt, _val)					\
	do {								\
		u32	_i, _reg;					\
		for (_i = 0; _i < (STATECHG_REG_TIMEOUT_IN_USECS*20); _i++) {\
			_reg = ioread32(nvudc->mmio_reg_base + EP_STCHG);\
			if ((_reg & _val) == _val)			\
				break;					\
			ndelay(50);					\
		}							\
		if (_i == (STATECHG_REG_TIMEOUT_IN_USECS*20)) {		\
			msg_dbg(dev, "STCHG Timeout: %s\n", _fmt);	\
			msg_dbg(dev, "STCHG=0x%.8x, EpMask=0x%.8x\n",	\
					_reg, _val);			\
		}							\
	} while (0)


/*
 * macro to poll RELOAD reg for expected value.  If we exceed polling limit,
 * a console message is printed.  _fmt must be enclosed
 * in "" as it is string identifier into dev_dbg().
 */
#define RELOAD_REG_TIMEOUT_IN_USECS 100
#define poll_reload(dev, _fmt, _val)					\
	do {								\
		u32	_i, _reg;					\
		for (_i = 0; _i < (RELOAD_REG_TIMEOUT_IN_USECS*20); _i++) {\
			_reg = ioread32(nvudc->mmio_reg_base + EP_RELOAD);\
			if ((_reg & _val) == 0)				\
				break;					\
			ndelay(50);					\
		}							\
		if (_i == (RELOAD_REG_TIMEOUT_IN_USECS*20)) {		\
			msg_dbg(dev, "RELOAD Timeout: "_fmt"\n");	\
			msg_dbg(dev, "RELOAD=0x%.8x, EpMask=0x%.8x\n",	\
					_reg, _val);			\
		} \
	} while (0)

/*
 * macro to poll EP_STOPPED reg for specific endpoint.
 * If we exceed polling limit, a console message is printed.
 * _fmt must be enclosed * in ""
 * as it is string identifier into dev_dbg().
 */
#define EP_STOPPED_REG_TIMEOUT_IN_USECS 100
#define poll_ep_stopped(dev, _fmt, _val)				\
	do {								\
		u32	_i, _reg;					\
		for (_i = 0; _i < (EP_STOPPED_REG_TIMEOUT_IN_USECS*20); _i++) {\
			_reg = ioread32(nvudc->mmio_reg_base + EP_STOPPED);\
			if (_reg & _val)			\
				break;					\
			ndelay(50);					\
		}							\
		if (_i == (EP_STOPPED_REG_TIMEOUT_IN_USECS*20)) {	\
			msg_dbg(dev, "EP_STOPPED Timeout: "_fmt"\n");	\
		}							\
	} while (0)

/*
* macro to poll EP_THREAD_ACTIVE reg for specific endpoint.
* If we exceed polling limit, a console message is printed.
* _fmt must be enclosed * in ""
* as it is string identifier into dev_dbg().
*/
#define EP_ACTIVE_TIMEOUT_IN_USECS 100
#define poll_ep_thread_active(dev, _fmt, _val)                         \
	do {                                                            \
		u32     _i, _reg;                                       \
		for (_i = 0; _i < (EP_ACTIVE_TIMEOUT_IN_USECS*20); _i++) {\
			_reg = ioread32(nvudc->mmio_reg_base            \
				+ EP_THREAD_ACTIVE);                    \
			if ((_reg & _val) == 0)                         \
				break;                                  \
			ndelay(50);                                     \
		}                                                       \
		if (_i == (EP_ACTIVE_TIMEOUT_IN_USECS*20)) {    \
			msg_dbg(dev, "EP_ACTIVE Timeout: "_fmt"\n");    \
		}                                                       \
	} while (0)

extern int debug_level;
#define LEVEL_DEBUG	4
#define LEVEL_INFO	3
#define LEVEL_WARNING	2
#define LEVEL_ERROR	1

#define msg_dbg(dev, fmt, args...) { \
	if (debug_level >= LEVEL_DEBUG) \
		dev_dbg(dev, "%s():%d: " fmt, __func__ , __LINE__, ## args); \
	}
#define msg_info(dev, fmt, args...) { \
	if (debug_level >= LEVEL_INFO) \
		dev_dbg(dev, "%s():%d: " fmt, __func__ , __LINE__, ## args); \
	}
#define msg_err(dev, fmt, args...) { \
	if (debug_level >= LEVEL_ERROR) \
		dev_err(dev, fmt, ## args); \
	}
#define msg_warn(dev, fmt, args...) { \
	if (debug_level >= LEVEL_WARNING) \
		dev_warn(dev, fmt, ## args); \
	}

#define msg_entry(dev) msg_dbg(dev, "enter\n")
#define msg_exit(dev) msg_dbg(dev, "exit\n")

/* xhci dev registers*/
#define TERMINATION_1				(0x7e4)
#define TERMINATION_2				(0x7e8)

/* fpci mmio registers */
#define XUSB_DEV_CFG_0				(0x0)
#define XUSB_DEV_CFG_1				(0x4)
#define  IO_SPACE_ENABLED			(1 << 0)
#define  MEMORY_SPACE_ENABLED			(1 << 1)
#define  BUS_MASTER_ENABLED			(1 << 2)

#define XUSB_DEV_CFG_4				(0x10)
#define  BASE_ADDRESS(x)			(((x) & 0xFFFF) << 16)
#define XUSB_DEV_CFG_5				(0x14)

#define SSPX_CORE_PADCTL4			(0x750)
#define RXDAT_VLD_TIMEOUT_U3_MASK	(0xfffff)
#define RXDAT_VLD_TIMEOUT_U3		(0x5dc0)

#define SSPX_CORE_CNT0				(0x610)
#define PING_TBRST_MASK				(0xff)
#define PING_TBRST(x)				((x) & 0xff)

#define SSPX_CORE_CNT30				(0x688)
#define LMPITP_TIMER_MASK			(0xfffff)
#define LMPITP_TIMER(x)				((x) & 0xfffff)

#define SSPX_CORE_CNT32				(0x690)
#define POLL_TBRST_MAX_MASK			(0xff)
#define POLL_TBRST_MAX(x)			((x) & 0xff)

/* ipfs mmio registers */
#define XUSB_DEV_CONFIGURATION			(0x180)
#define  EN_FPCI				(1 << 0)

#define XUSB_DEV_INTR_MASK			(0x188)
#define  IP_INT_MASK				(1 << 16)

/* padctl mmio registers */
#define USB2_PAD_MUX				(0x4)
#define USB2_OTG_PAD(x, v)			(((v) & 0x3) << ((x) * 2))
#define  SNPS					(0)
#define  XUSB					(1)
#define  UART					(2)

#define USB2_PORT_CAP				(0x8)
#define PORT_CAP(x, v)				(((v) & 0x3) << ((x) * 4))
#define  PORT_CAP_DISABLED			(0)
#define  PORT_CAP_HOST				(1)
#define  PORT_CAP_DEV				(2)
#define  PORT_CAP_OTG				(3)
#define PORT_INTERNAL(x)			(1 << ((x) * 4 + 2))
#define PORT_REVERSE_ID(x)			(1 << ((x) * 4 + 3))

#define ELPG_PROGRAM				(0x24)

#define IOPHY_USB3_PAD0_CTL_2			(0x58)
#define  CDR_CNTL(x)				(((x) & 0xFF) << 24)
#define  RX_EQ(x)				(((x) & 0xFFFF) << 8)
#define  RX_WANDER(x)				(((x) & 0xf) << 4)
#define  RX_TERM_CNTL(x)			(((x) & 0x3) << 2)
#define  TX_TERM_CNTL(x)			((x) & 0x3)

#define IOPHY_USB3_PAD0_CTL_4			(0x68)
#define  DFE_CNTL(x)				((x) & 0xFFFFFFFF)

#define IOPHY_MISC_PAD_P0_CTL_2		(0x78)
#define  SPARE_IN(x)				(((x) & 0x3) << 28)

#define USB2_OTG_PAD0_CTL_0			(0xa0)
#define  HS_CURR_LEVEL(x)			(((x) & 0x3f) << 0)
#define  HS_SLEW(x)				(((x) & 0x3f) << 6)
#define  FS_SLEW(x)				(((x) & 0x3) << 12)
#define  LS_RSLEW(x)				(((x) & 0x3) << 14)
#define  LS_FSLEW(x)				(((x) & 0x3) << 16)
#define  PAD_PD				(1 << 19)
#define  PAD_PD2				(1 << 20)
#define  PAD_PD_ZI				(1 << 21)
#define  LSBIAS_SEL				(1 << 23)

#define USB2_OTG_PAD0_CTL_1			(0xac)
#define  PAD_PD_DR				(1 << 2)
#define  TERM_RANGE_ADJ(x)			(((x) & 0xf) << 3)
#define  HS_IREF_CAP(x)			(((x) & 0x3) << 9)

#define USB3_PAD_MUX				(0x134)
#define  FORCE_PCIE_PAD_IDDQ_DISABLE_MASK0	(1 << 1)
#define  FORCE_PCIE_PAD_IDDQ_DISABLE_MASK1	(1 << 2)
#define  PCIE_PAD_LANE0			(((x) & 0x3) << 16)
#define  PCIE_PAD_LANE1			(((x) & 0x3) << 18)

#define UPHY_USB3_PAD_ECTL_1(p)		(0xa60 + (p * 0x40))
#define TX_TERM_CTRL(x)			(((x) & 0x3) << 16)

#define UPHY_USB3_PAD_ECTL_2(p)		(0xa64 + (p * 0x40))
#define RX_CTLE(x)				(((x) & 0xffff) << 0)

#define UPHY_USB3_PAD_ECTL_3(p)		(0xa68 + (p * 0x40))
#define RX_DFE(x)				(((x) & 0xffffffff) << 0)

#define UPHY_USB3_PAD_ECTL_4(p)		(0xa6c + (p * 0x40))
#define RX_CDR_CTRL(x)				(((x) & 0xffff) << 16)

#define UPHY_USB3_PAD_ECTL_6(p)		(0xa74 + (p * 0x40))
#define RX_EQ_CTRL_H(x)			(((x) & 0xffffffff) << 0)

#define XUSB_VBUS				(0xc60)

