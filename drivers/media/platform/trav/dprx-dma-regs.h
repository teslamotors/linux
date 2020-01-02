#ifndef _DPRX_DMA_SFR_H_
#define _DPRX_DMA_SFR_H_

#define	DPRX_DMA_SFR_UP					0x100
#define	DPRX_DMA_SFR_UP_VID				0x104
#define	DPRX_DMA_SFR_UP_AUD				0x108
#define	DPRX_DMA_SFR_UP_SDP				0x10C
#define DPRX_DMA_SFR_UPDATE(_n)				(0x1 << (_n))

#define	DPRX_DMA_SFR_CHECK_SIZE_VID			0x110
#define	DPRX_DMA_SFR_CHECK_SIZE_AUD			0x114
#define	DPRX_DMA_SFR_CHECK_SIZE_SDP			0x118
#define	DPRX_DMA_SFR_LINEEND_VID			0x11C
#define DPRX_DMA_SFR_DATACHECK_SIZE(_n)			(0x1 << (_n))

#define DPRX_DMA_SOFT_RESET_ALL				0x200
#define DPRX_DMA_SOFT_RESET_ALL_DMA			(0x1 << 3)
#define DPRX_DMA_SOFT_RESET_ALL_VID			(0x1 << 2)
#define DPRX_DMA_SOFT_RESET_ALL_AUD			(0x1 << 1)
#define DPRX_DMA_SOFT_RESET_ALL_SDP			(0x1 << 0)

#define DPRX_DMA_SOFT_RESET_VID				0x204
#define DPRX_DMA_SOFT_RESET_VID_WB			(0x1 << 9)
#define DPRX_DMA_SOFT_RESET_VID_NIC400			(0x1 << 8)
#define DPRX_DMA_SOFT_RESET_VID_MST(_n)			(0x1 << (_n))

#define DPRX_DMA_SOFT_RESET_AUD				0x208
#define DPRX_DMA_SOFT_RESET_AUD_WB			(0x1 << 9)
#define DPRX_DMA_SOFT_RESET_AUD_NIC400			(0x1 << 8)
#define DPRX_DMA_SOFT_RESET_AUD_MST(_n)			(0x1 << (_n))

#define DPRX_DMA_SOFT_RESET_SDP				0x20C
#define DPRX_DMA_SOFT_RESET_SDP_WB			(0x1 << 9)
#define DPRX_DMA_SOFT_RESET_SDP_NIC400			(0x1 << 8)
#define DPRX_DMA_SOFT_RESET_SDP_MST(_n)			(0x1 << (_n))

#define	DPRX_DMA_INTGEN_SEL				0x1000

/* Type of interrupt register */
#define BUFFER_OVERFLOW					0
#define FRAME_START					1
#define FRAME_END					2
#define FRAME_ERR					3
#define CHK_SOFTRST					4
#define SOFTRESET_DONE					5
#define FRAME_DONE					6

#define	DPRX_DMA_INT_SRC_VID(_n)			(0x1100 + ((_n) * 4))

#define	DPRX_DMA_INT_MSK_VID(_n)			(0x1130 + ((_n) * 4))

#define	DPRX_DMA_INT_CLR_VID(_n)			(0x1160 + ((_n) * 4))

#define	DPRX_DMA_INT_VID_STATUS(_n)			(0x11A0 + ((_n) * 4))

#define	DPRX_DMA_INT_SRC_AUD(_n)			(0x1200 + ((_n) * 4))

#define	DPRX_DMA_INT_MSK_AUD(_n)			(0x1230 + ((_n) * 4))

#define	DPRX_DMA_INT_CLR_AUD(_n)			(0x1260 + ((_n) * 4))

#define	DPRX_DMA_INT_AUD_STATUS(_n)			(0x12A0 + ((_n) * 4))

#define	DPRX_DMA_INT_SRC_SDP(_n)			(0x1300 + ((_n) * 4))

#define	DPRX_DMA_INT_MSK_SDP(_n)			(0x1330 + ((_n) * 4))

#define	DPRX_DMA_INT_CLR_SDP(_n)			(0x1360 + ((_n) * 4))

#define	DPRX_DMA_INT_SDP_STATUS(_n)			(0x13A0 + ((_n) * 4))

/* VID block */
#define	DPRX_DMA_VID_FMT_STRM(_n)			(0x2000 + ((_n) * 4))
#define DPRX_DMA_VID_FMT_STRM_RGB			(0x3 << 0)
#define DPRX_DMA_VID_FMT_STRM_RAW			(0x4 << 0)

#define	DPRX_DMA_VID_BITS_STRM(_n)			(0x2020 + ((_n) * 4))
#define DPRX_DMA_VID_BITS_WIDTH_8			0
#define DPRX_DMA_VID_BITS_WIDTH_10			1
#define DPRX_DMA_VID_BITS_WIDTH_12			2
#define DPRX_DMA_VID_BITS_WIDTH_14			3
#define DPRX_DMA_VID_BITS_WIDTH_16			4
#define DPRX_DMA_VID_BITS_WIDTH_MASK			~(0x7 << 0)

#define	DPRX_DMA_VID_MODE_STRM(_n)			(0x2040 + ((_n) * 4))
#define DPRX_DMA_VID_MODE_STRM_UNPACKED			(0x0 << 0)
#define DPRX_DMA_VID_MODE_STRM_PACKED			(0x1 << 0)

#define	DPRX_DMA_VID_HEIGHT_STRM(_n)			(0x3000 + ((_n) * 4))

#define	DPRX_DMA_VID_WIDTH_STRM(_n)			(0x3020 + ((_n) * 4))

#define	DPRX_DMA_YBASE_STRM(_c, _f)			(0x3080 + ((_c) * 0x20) + ((_f) * 4))

#define	DPRX_DMA_YBASE_STRM0(_f)			(0x3080 + ((_f) * 4))
#define	DPRX_DMA_YBASE_STRM1(_f)			(0x30A0 + ((_f) * 4))
#define	DPRX_DMA_YBASE_STRM2(_f)			(0x30C0 + ((_f) * 4))
#define	DPRX_DMA_YBASE_STRM3(_f)			(0x30E0 + ((_f) * 4))
#define	DPRX_DMA_YBASE_STRM4(_f)			(0x3100 + ((_f) * 4))
#define	DPRX_DMA_YBASE_STRM5(_f)			(0x3120 + ((_f) * 4))
#define	DPRX_DMA_YBASE_STRM6(_f)			(0x3140 + ((_f) * 4))
#define	DPRX_DMA_YBASE_STRM7(_f)			(0x3160 + ((_f) * 4))

#define DPRX_DMA_ISSUE_NUM_STRM(_n)			(0x3180 + ((_n) * 4))

#define DPRX_DMA_FRAME_NUM_VID_STRM(_n)			(0x31A0 + ((_n) * 4))

#define DPRX_DMA_BASE_ADDR_VID_STRM(_n)			(0x31C0 + ((_n) * 4))

#define DPRX_DMA_DMA_EN_VID				0x3500
#define DPRX_DMA_DMA_EN_VID_CH(_n)			(1 << (_n))

/* SDP block */
#define DPRX_DMA_SDPBASE_STRM(_c, _f)			(0x4000 + ((_c) * 0x20) + ((_f) * 4))
#define DPRX_DMA_SDPBASE_STRM0(_f)			(0x4000 + ((_f) * 4))
#define DPRX_DMA_SDPBASE_STRM1(_f)			(0x4020 + ((_f) * 4))
#define DPRX_DMA_SDPBASE_STRM2(_f)			(0x4040 + ((_f) * 4))
#define DPRX_DMA_SDPBASE_STRM3(_f)			(0x4060 + ((_f) * 4))
#define DPRX_DMA_SDPBASE_STRM4(_f)			(0x4080 + ((_f) * 4))
#define DPRX_DMA_SDPBASE_STRM5(_f)			(0x40A0 + ((_f) * 4))
#define DPRX_DMA_SDPBASE_STRM6(_f)			(0x40C0 + ((_f) * 4))
#define DPRX_DMA_SDPBASE_STRM7(_f)			(0x40E0 + ((_f) * 4))

#define DPRX_DMA_NO_SDP_STRM(_n)			(0x4100 + ((_n) * 4))

#define DPRX_DMA_FRAME_NUM_SDP_STRM(_n)			(0x4120 + ((_n) * 4))

#define DPRX_DMA_BASE_ADDR_SDP_STRM(_n)			(0x4140 + ((_n) * 4))

#define DPRX_DMA_DMA_EN_SDP				0x4500
#define DPRX_DMA_DMA_EN_SDP_CH(_n)			(1 << (_n))

/* AUD block */
#define DPRX_DMA_AUDBASE_STRM(_c, _f)			(0x5000 + (_c * 0x20) + ((_f) * 4))
#define DPRX_DMA_AUDBASE_STRM0(_f)			(0x5000 + ((_f) * 4))
#define DPRX_DMA_AUDBASE_STRM1(_f)			(0x5020 + ((_f) * 4))
#define DPRX_DMA_AUDBASE_STRM2(_f)			(0x5040 + ((_f) * 4))
#define DPRX_DMA_AUDBASE_STRM3(_f)			(0x5060 + ((_f) * 4))
#define DPRX_DMA_AUDBASE_STRM4(_f)			(0x5080 + ((_f) * 4))
#define DPRX_DMA_AUDBASE_STRM5(_f)			(0x50A0 + ((_f) * 4))
#define DPRX_DMA_AUDBASE_STRM6(_f)			(0x50C0 + ((_f) * 4))
#define DPRX_DMA_AUDBASE_STRM7(_f)			(0x50E0 + ((_f) * 4))

#define DPRX_DMA_NO_AUD_STRM(_n)			(0x5100 + ((_n) * 4))

#define DPRX_DMA_FRAME_NUM_AUD_STRM(_n)			(0x5120 + ((_n) * 4))

#define DPRX_DMA_BASE_ADDR_AUD_STRM(_n)			(0x5140 + ((_n) * 4))

#define DPRX_DMA_DMA_EN_AUD				0x5500
#define DPRX_DMA_DMA_EN_AUD_CH(_n)			(1 << (_n))

#endif
