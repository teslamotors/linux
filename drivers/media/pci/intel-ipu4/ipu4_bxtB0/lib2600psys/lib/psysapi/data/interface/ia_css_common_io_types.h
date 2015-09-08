/**
* Support for Intel Camera Imaging ISP subsystem.
* Copyright (c) 2010 - 2015, Intel Corporation.
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

#ifndef __IA_CSS_COMMON_IO_TYPES
#define __IA_CSS_COMMON_IO_TYPES

#include <type_support.h>

#ifndef MAX_IO_DMA_CHANNELS
#define MAX_IO_DMA_CHANNELS 3
#endif

/*
 * Pre-defined frame format
 *
 * Those formats have inbuild support of traffic
 * and access functions
 *
 * Note that the formats are for terminals, so there
 * is no distinction between input and output formats
 *	- Custom formats with ot without descriptor
 *	- 4CC formats such as YUV variants
 *	- MIPI (line) formats as produced by CSI receivers
 *	- MIPI (sensor) formats such as Bayer or RGBC
 *	- CSS internal formats (private types)
 *  - CSS parameters (type 1 - 6)
 */
#define IA_CSS_FRAME_FORMAT_TYPE_BITS					32
typedef enum ia_css_frame_format_type {
	IA_CSS_DATA_CUSTOM_NO_DESCRIPTOR = 0,
	IA_CSS_DATA_CUSTOM,

	IA_CSS_DATA_FORMAT_NV11,		/**<  12 bit YUV 411, Y, UV 2-plane  (8 bit per element) */
	IA_CSS_DATA_FORMAT_YUV420,		/**< bpp bit YUV 420, Y, U, V 3-plane (bpp/1.5 bpe) */
	IA_CSS_DATA_FORMAT_YV12,		/**<  12 bit YUV 420, Y, V, U 3-plane (8 bit per element) */
	IA_CSS_DATA_FORMAT_NV12,		/**<  12 bit YUV 420, Y, UV 2-plane (8 bit per element) */
	IA_CSS_DATA_FORMAT_NV12_16,		/**<  16 bit YUV 420, Y, UV 2-plane (8 bit per element) */
	IA_CSS_DATA_FORMAT_NV12_TILEY,		/**<  12 bit YUV 420, Intel proprietary tiled format, TileY */
	IA_CSS_DATA_FORMAT_NV21,		/**<  12 bit YUV 420, Y, VU 2-plane  (8 bit per element) */
	IA_CSS_DATA_FORMAT_YUV422,		/**< bpp bit YUV 422, Y, U, V 3-plane (bpp/2 bpe) */
	IA_CSS_DATA_FORMAT_YV16,		/**<  16 bit YUV 422, Y, V, U 3-plane  (8 bit per element) */
	IA_CSS_DATA_FORMAT_NV16,		/**<  16 bit YUV 422, Y, UV 2-plane  (8 bit per element) */
	IA_CSS_DATA_FORMAT_NV61,		/**<  16 bit YUV 422, Y, VU 2-plane  (8 bit per element) */
	IA_CSS_DATA_FORMAT_UYVY,		/**<  16 bit YUV 422, UYVY 1-plane interleaved  (8 bit per element) */
	IA_CSS_DATA_FORMAT_YUYV,		/**<  16 bit YUV 422, YUYV 1-plane interleaved  (8 bit per element) */
	IA_CSS_DATA_FORMAT_YUV444,		/**< bpp bit YUV 444, Y, U, V 3-plane (bpp/3 bpe) */
	IA_CSS_DATA_FORMAT_Y800,		/**< 8 bit monochrome plane */

	IA_CSS_DATA_FORMAT_RGB565,		/**< 5-6-5 bit packed (1-plane) RGB (16bpp, ~5 bpe) */
	IA_CSS_DATA_FORMAT_RGB888,		/**< 24 bit RGB, 3 planes  (8 bit per element) */
	IA_CSS_DATA_FORMAT_RGBA888,		/**< 32 bit RGB-Alpha, 1 plane  (8 bit per element) */

	IA_CSS_DATA_FORMAT_BAYER_GRBG,		/**< bpp bit raw, [[Gr, R];[B, Gb]] 1-plane (bpp == bpe) */
	IA_CSS_DATA_FORMAT_BAYER_RGGB,		/**< bpp bit raw, [[R, Gr];[Gb, B]] 1-plane (bpp == bpe) */
	IA_CSS_DATA_FORMAT_BAYER_BGGR,		/**< bpp bit raw, [[B, Gb];[Gr, R]] 1-plane (bpp == bpe) */
	IA_CSS_DATA_FORMAT_BAYER_GBRG,		/**< bpp bit raw, [[Gb, B];[R, Gr]] 1-plane (bpp == bpe) */

	IA_CSS_DATA_FORMAT_YUV420_LINE,		/**< bpp bit (NV12) YUV 420, Y, UV 2-plane derived 3-line, 2-Y, 1-UV (bpp/1.5 bpe) */
	IA_CSS_DATA_FORMAT_RAW,			/**< Deprecated RAW, 1 plane */
	IA_CSS_DATA_FORMAT_RAW_PACKED,		/**< Deprecated RAW, 1 plane, packed */
	IA_CSS_DATA_FORMAT_QPLANE6,		/**< Internal, for advanced ISP */
	IA_CSS_DATA_FORMAT_BINARY_8,		/**< 1D byte stream, used for jpeg 1-plane */
	IA_CSS_DATA_FORMAT_MIPI,			/**< Deprecated MIPI frame, 1D byte stream 1 plane */
	IA_CSS_DATA_FORMAT_MIPI_YUV420_8,	/**<  12 bit [[YY];[UYVY]]  1-plane interleaved 2-line (8 bit per element) */
	IA_CSS_DATA_FORMAT_MIPI_YUV420_10,	/**<  15 bit [[YY];[UYVY]]  1-plane interleaved 2-line (10 bit per element) */
	IA_CSS_DATA_FORMAT_MIPI_LEGACY_YUV420_8,/**<  12 bit [[UY];[VY]]  1-plane interleaved 2-line (8 bit per element) */

	IA_CSS_DATA_GENERIC_PARAMETER,		/**< Type 1-5 parameter, not fragmentable */
	IA_CSS_DATA_DVS_PARAMETER,		/**< Video stabilisation Type 6 parameter, fragmentable */
	IA_CSS_DATA_DVS_COORDINATES,		/**< Video stabilisation Type 6 parameter, coordinates */
	IA_CSS_DATA_DPC_PARAMETER,		/**< Dead Pixel correction Type 6 parameter, fragmentable */
	IA_CSS_DATA_LSC_PARAMETER,		/**< Lens Shading Correction Type 6 parameter, fragmentable */
	IA_CSS_DATA_S3A_STATISTICS_HI,		/**< 3A statistics output HI. */
	IA_CSS_DATA_S3A_STATISTICS_LO,		/**< 3A statistics output LO. */
	IA_CSS_DATA_S3A_HISTOGRAM,		/**< histogram output */

	IA_CSS_DATA_FORMAT_BAYER_LINE_INTERLEAVED, 	/**< Gr R B Gb Gr R B Gb  in PIXELS  (also called isys interleaved) */
	IA_CSS_DATA_FORMAT_BAYER_VECTORIZED,		/**< Gr R B Gb Gr R B Gb  in VECTORS (VCC IMAGE, ISP NWAY depentdent) */
	IA_CSS_DATA_FORMAT_BAYER_GRBG_VECTORIZED,	/**< Gr R Gr R ... | B Gb B Gb ..  in VECTORS (ISP NWAY depentdent) */

	IA_CSS_DATA_FORMAT_YUV420_VECTORIZED,   /**<  16 bit YUV 420, Y even plane, Y uneven plane, UV plane vector interleaved */
	IA_CSS_DATA_FORMAT_YYUVYY_VECTORIZED,   /**<  16 bit YUV 420, YYUVYY vector interleaved */

	IA_CSS_N_FRAME_FORMAT_TYPES
} ia_css_frame_format_type_t;

#define IA_CSS_MAX_N_FRAME_PLANES			6

struct ia_css_resolution_descriptor_s {
	uint16_t			width;					/**< Logical dimensions */
	uint16_t			height;					/**< Logical dimensions */
};

/*
 * Should comply to enum ia_css_dimension_t in ./fw_abi_common_types/ia_css_terminal_defs.h
 */
#define COL_DIMENSION 0
#define ROW_DIMENSION 1
#define N_DATA_DIMENSION 2
#define N_MAX_NUM_SLICES_PER_FRAGMENT 32
#define N_MAX_NUM_SECTIONS_PER_SLICE 4
#define N_COMMAND_COUNT 4

/*
 * Terminal type
 */
struct ia_css_terminal_descriptor_s {
	ia_css_frame_format_type_t	format;			/**< Indicates if this is a generic type or inbuild with variable size descriptor */
	uint32_t			plane_count;				/**< Number of data planes (pointers) */
	uint32_t			plane_offsets[IA_CSS_MAX_N_FRAME_PLANES];	/**< Plane offsets */
	uint32_t			stride;					/**< Physical size aspects */
	uint16_t			width;					/**< Logical dimensions */
	uint16_t			height;					/**< Logical dimensions */
	uint8_t				bpp;					/**< Bits per pixel */
	uint8_t				bpe;					/**< Bits per element */
	uint32_t			base_address;				/**< DDR base address */
	uint32_t			fragment_index[N_DATA_DIMENSION];	/**< Fragment start offset in pixels for each dimension */
};

/*
 * Sub-struct for Spatial terminal descriptor
 */
struct frame_grid_param_section_desc_s {
	uint32_t			mem_offset;		/**< Offset of the parameter allocation in memory */
	uint32_t			mem_size;		/**< Memory allocation size needs of this parameter */
	uint32_t			stride;			/**< stride in bytes of each line of compute units for the specified memory space and region */
};

/*
 * Spatial terminal descriptor
 */
struct ia_css_spatial_terminal_descriptor_s {
	uint32_t	base_address;  /**< DDR base address of spatial parameter buffer */
	uint16_t	frame_grid_dimension[N_DATA_DIMENSION];		/**< Spatial param FRAME width/height, measured in compute units */
	uint16_t	fragment_grid_index[N_DATA_DIMENSION];		/**< Spatial param FRAGMENT offset of the top-left compute unit, compared to the frame */
	uint16_t	fragment_grid_dimension[N_DATA_DIMENSION];	/**< Spatial param FRAGMENT width/height, measured in compute units */
	struct frame_grid_param_section_desc_s param_planes[IA_CSS_MAX_N_FRAME_PLANES]; /**< Section/plane descriptors */
};

/*
 * Sub-struct for Sliced terminal descriptor
 */
struct sliced_param_section_desc_s {
	uint32_t			mem_offset;		/**< Offset of the parameter allocation in memory */
	uint32_t			mem_size;		/**< Memory allocation size needs of this parameter */
};

/*
 * Sliced terminal descriptor
 */
struct ia_css_sliced_terminal_descriptor_s {
	uint32_t	base_address;  				/**< DDR base address of spatial parameter buffer */
	uint16_t	slice_count;				/**< Number of slices for the parameters for this fragment */
	struct sliced_param_section_desc_s section_param[N_MAX_NUM_SLICES_PER_FRAGMENT][N_MAX_NUM_SECTIONS_PER_SLICE]; /**< Section descriptors */
};

/* DMA based terminals */
struct ia_css_common_dma_config {
	unsigned ddr_elems_per_word;
	unsigned dma_channel[MAX_IO_DMA_CHANNELS];
	unsigned here_crop_x; /* horizontal cropping in terms of elements on ISP side */
	unsigned here_crop_y; /* vertical cropping in terms of elements on ISP side */
};

/*
 * Sub-struct for Sequencer info descriptor
 */
struct kernel_fragment_sequencer_info_desc_s {
	uint16_t fragment_grid_slice_dimension[N_DATA_DIMENSION];				/**< Slice dimensions */
	uint16_t fragment_grid_slice_count[N_DATA_DIMENSION];					/**< Nof slices */
	uint16_t fragment_grid_point_decimation_factor[N_DATA_DIMENSION];			/**< Grid point decimation factor */
	int16_t fragment_grid_overlay_on_fragment_pixel_topleft_index[N_DATA_DIMENSION];	/**< Relative position of grid origin to pixel origin */
	int16_t fragment_grid_overlay_on_fragment_pixel_dimension[N_DATA_DIMENSION];		/**< Size of active fragment region */
	uint16_t command_count;									/**< If >0 it overrides the standard fragment sequencer info */
	uint16_t command_index;									/**< To be used only if command_count>0, index to the array of (ia_css_kernel_fragment_sequencer_command_desc_s) */
};

/*
 * Sub-struct for Sequencer info descriptor
 */
struct kernel_fragment_sequencer_command_desc_s {	/**< 4 commands packed together to save memory space */
	uint16_t line_count[N_COMMAND_COUNT];		/**< Contains the "(command_index%4) == index" command desc */
};

/*
 * Default limits on the number of sequencer info descriptor and command descriptor.
 */
#ifndef IA_CSS_MAX_N_SEQUENCER_INFO
#define IA_CSS_MAX_N_SEQUENCER_INFO 16
#endif
#ifndef IA_CSS_MAX_N_COMMAND_DESC
#define IA_CSS_MAX_N_COMMAND_DESC   32
#endif

/*
 * Sequencer info descriptor
 */
struct ia_css_sequencer_info_descriptor_s {
	struct kernel_fragment_sequencer_info_desc_s kernel_fragment_sequencer_info_desc[IA_CSS_MAX_N_SEQUENCER_INFO];
	struct kernel_fragment_sequencer_command_desc_s kernel_fragment_sequencer_command_desc[IA_CSS_MAX_N_COMMAND_DESC]; /* This command desc array is shared by all sequencers via command_index */
};

#endif /* __IA_CSS_COMMON_IO_TYPES */
