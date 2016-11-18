/*
* Support for Intel Camera Imaging ISP subsystem.
* Copyright (c) 2010 - 2016, Intel Corporation.
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

#ifndef __IA_CSS_PROGRAM_GROUP_DATA_H
#define __IA_CSS_PROGRAM_GROUP_DATA_H

#include "ia_css_psys_data_storage_class.h"

/*! \file */

/** @file ia_css_program_group_data.h
 *
 * Define the data objects that are passed to the process groups
 * i.e. frames and matrices with their sub-structures
 *
 * The data objects are separate from the process group terminal,
 * although they are stored by value rather than by reference and
 * make the process group terminal dependendent on its definition
 *
 * This frame definition overloads the current CSS frame definition
 * they are the same object, just a slightly different implementation
 */

#include <vied_nci_psys_system_global.h>	/* vied_vaddress_t */

#include <type_support.h>
#include "ia_css_program_group_data_defs.h"	/* ia_css_frame_format_type */

#include "ia_css_terminal_defs.h"

/*
 * Frame buffer state used for sequencing
 * (see FAS 5.5.3)
 *
 * The buffer can be in DDR or a handle to a stream
 */
typedef enum ia_css_buffer_state {
	IA_CSS_BUFFER_NULL = 0,
	IA_CSS_BUFFER_UNDEFINED,
	IA_CSS_BUFFER_EMPTY,
	IA_CSS_BUFFER_NONEMPTY,
	IA_CSS_BUFFER_FULL,
	IA_CSS_N_BUFFER_STATES
} ia_css_buffer_state_t;

#define IA_CSS_BUFFER_STATE_IN_BITS	32

/*
 * Pointer state used to signal MMU invalidation
 */
typedef enum ia_css_pointer_state {
	IA_CSS_POINTER_INVALID = 0,
	IA_CSS_POINTER_VALID,
	IA_CSS_N_POINTER_STATES
} ia_css_pointer_state_t;

#define IA_CSS_POINTER_STATE_IN_BITS	32

/*
 * Access direction needed to select the access port
 */
typedef enum ia_css_access_type {
	IA_CSS_ACCESS_LOCKED = 0,
	IA_CSS_ACCESS_READ,
	IA_CSS_ACCESS_WRITE,
	IA_CSS_ACCESS_MODIFY,
	IA_CSS_N_ACCESS_TYPES
} ia_css_access_type_t;

#define IA_CSS_ACCESS_TYPE_IN_BITS	32

/*
 * Access attribute needed to select the access port
 *	- public : snooped
 *	- private: non-snooped
 * Naming is a bit awkward, lack of inspiration
 */
typedef enum ia_css_access_scope {
	IA_CSS_ACCESS_PRIVATE = 0,
	IA_CSS_ACCESS_PUBLIC,
	IA_CSS_N_ACCESS_SCOPES
} ia_css_access_scopes_t;

#define IA_CSS_ACCESS_SCOPES_IN_BITS	32

#define IA_CSS_N_FRAME_PLANES			6

#define IA_CSS_FRAME_FORMAT_BITMAP_BITS		64
typedef uint64_t				ia_css_frame_format_bitmap_t;

typedef struct ia_css_param_frame_descriptor_s ia_css_param_frame_descriptor_t;
typedef struct ia_css_param_frame_s		ia_css_param_frame_t;

typedef struct ia_css_frame_descriptor_s	ia_css_frame_descriptor_t;
typedef struct ia_css_frame_s				ia_css_frame_t;
typedef struct ia_css_fragment_descriptor_s	ia_css_fragment_descriptor_t;

typedef struct ia_css_stream_s			ia_css_stream_t;


#define	N_UINT64_IN_STREAM_STRUCT			1

#define	IA_CSS_STREAM_STRUCT_BITS \
	(N_UINT64_IN_STREAM_STRUCT * 64)

struct ia_css_stream_s {
	uint64_t dummy;
};

struct ia_css_param_frame_descriptor_s {
	uint16_t	size;		/**< Size of the descriptor */
	uint32_t	buffer_count;	/**< Number of parameter buffers */
};

struct ia_css_param_frame_s {
	/*< Base virtual addresses to parameters in subsystem virtual
	 * memory space
	 */
	vied_vaddress_t *data;
};

#define N_UINT32_IN_FRAME_DESC_STRUCT \
	(1 + IA_CSS_N_FRAME_PLANES + (IA_CSS_N_DATA_DIMENSION - 1))
#define N_UINT16_IN_FRAME_DESC_STRUCT (1 + IA_CSS_N_DATA_DIMENSION)
#define N_UINT8_IN_FRAME_DESC_STRUCT 3
#define N_PADDING_UINT8_IN_FRAME_DESC_STRUCT 3

#define	IA_CSS_FRAME_DESCRIPTOR_STRUCT_BITS \
	(IA_CSS_FRAME_FORMAT_TYPE_BITS \
	+ (N_UINT32_IN_FRAME_DESC_STRUCT * 32) \
	+ (N_UINT16_IN_FRAME_DESC_STRUCT * 16) \
	+ (N_UINT8_IN_FRAME_DESC_STRUCT * 8) \
	+ (N_PADDING_UINT8_IN_FRAME_DESC_STRUCT * 8))

/*
 * Structure defining the frame (size and access) properties for
 * inbuild types only.
 *
 * The inbuild types like FourCC, MIPI and CSS private types are supported
 * by FW all other types are custom types which interpretation must be encoded
 * on the buffer itself or known by the source and sink
 */
struct ia_css_frame_descriptor_s {
	/**< Indicates if this is a generic type or inbuild with
	  * variable size descriptor
	  */
	ia_css_frame_format_type_t frame_format_type;
	/**< Number of data planes (pointers) */
	uint32_t plane_count;
	/**< Plane offsets accounting for fragments */
	uint32_t plane_offsets[IA_CSS_N_FRAME_PLANES];
	/**< Physical size aspects */
	uint32_t stride[IA_CSS_N_DATA_DIMENSION - 1];
	/**< Logical dimensions */
	uint16_t dimension[IA_CSS_N_DATA_DIMENSION];
	/**< Size of this descriptor */
	uint16_t size;
	/**< Bits per pixel */
	uint8_t bpp;
	/**< Bits per element */
	uint8_t bpe;
	/**< 1 if terminal uses compressed datatype, 0 otherwise */
	uint8_t is_compressed;
	/**< Padding for 64bit alignment */
	uint8_t padding[N_PADDING_UINT8_IN_FRAME_DESC_STRUCT];
};

#define	N_UINT32_IN_FRAME_STRUCT		1

#define	IA_CSS_FRAME_STRUCT_BITS \
	(IA_CSS_BUFFER_STATE_IN_BITS \
	+ IA_CSS_ACCESS_TYPE_IN_BITS \
	+ IA_CSS_POINTER_STATE_IN_BITS \
	+ IA_CSS_ACCESS_SCOPES_IN_BITS \
	+ VIED_VADDRESS_BITS \
	+ (N_UINT32_IN_FRAME_STRUCT * 32))

/*
 * Main frame structure holding the main store and auxilary access properties
 * the "pointer_state" and "access_scope" should be encoded on the
 * "vied_vaddress_t" type
 */
struct ia_css_frame_s {
	/**< State of the frame for purpose of sequencing */
	ia_css_buffer_state_t buffer_state;
	/**< Access direction, may change when buffer state changes */
	ia_css_access_type_t access_type;
	/**< State of the pointer for purpose of embedded MMU coherency */
	ia_css_pointer_state_t pointer_state;
	/**< Access to the pointer for purpose of host cache coherency */
	ia_css_access_scopes_t access_scope;
	/**< Base virtual address to data in subsystem virtual memory space */
	vied_vaddress_t data;
	/**< Total allocation size in bytes */
	uint32_t data_bytes;
};

#define	N_UINT16_IN_FRAGMENT_DESC_STRUCT	(3 * IA_CSS_N_DATA_DIMENSION)
#define	N_PADDING_UINT8_IN_FRAGMENT_DESC_STRUCT 4

#define	IA_CSS_FRAGMENT_DESCRIPTOR_STRUCT_BITS \
	((N_UINT16_IN_FRAME_DESC_STRUCT * 16) \
	+ (N_PADDING_UINT8_IN_FRAGMENT_DESC_STRUCT * 8))

/*
 * Structure defining the fragment (size and access) properties.
 *
 * All cropping and padding effects are described by the difference between
 * the frame size and its location and the fragment size(s) and location(s)
 */
struct ia_css_fragment_descriptor_s {
	/**< Logical dimensions of the fragment */
	uint16_t dimension[IA_CSS_N_DATA_DIMENSION];
	/**< Logical location of the fragment in the frame */
	uint16_t index[IA_CSS_N_DATA_DIMENSION];
	/**< Fractional start (phase) of the fragment in the access unit */
	uint16_t offset[IA_CSS_N_DATA_DIMENSION];
	/**< Padding for 64bit alignment */
	uint8_t padding[N_PADDING_UINT8_IN_FRAGMENT_DESC_STRUCT];
};


/*! Print the frame object to file/stream

 @param	frame[in]			frame object
 @param	fid[out]			file/stream handle

 @return < 0 on error
 */
IA_CSS_PSYS_DATA_STORAGE_CLASS_H
int ia_css_frame_print(
	const ia_css_frame_t *frame, void *fid);

/*! Get the data buffer handle from the frame object

@param	frame[in]			frame object

@return buffer pointer, VIED_NULL on error
*/
IA_CSS_PSYS_DATA_STORAGE_CLASS_H
const vied_vaddress_t *ia_css_frame_get_buffer_host_virtual_address(
	const ia_css_frame_t *frame);

/*! Get the data buffer handle from the frame object

 @param	frame[in]			frame object

 @return buffer pointer, VIED_NULL on error
 */
IA_CSS_PSYS_DATA_STORAGE_CLASS_H
vied_vaddress_t ia_css_frame_get_buffer(const ia_css_frame_t *frame);

/*! Set the data buffer handle on the frame object

 @param	frame[in]			frame object
 @param	buffer[in]			buffer pointer

 @return < 0 on error
 */
IA_CSS_PSYS_DATA_STORAGE_CLASS_H
int ia_css_frame_set_buffer(
	ia_css_frame_t *frame, vied_vaddress_t buffer);

/*! Set the data buffer size on the frame object

 @param	frame[in]			frame object
 @param	size[in]			number of data bytes

 @return < 0 on error
 */
IA_CSS_PSYS_DATA_STORAGE_CLASS_H
int ia_css_frame_set_data_bytes(
	ia_css_frame_t *frame, unsigned size);

/*! Get the data buffer state from the frame object

 @param	frame[in]			frame object

 @return buffer state, limit value on error
 */
IA_CSS_PSYS_DATA_STORAGE_CLASS_H
ia_css_buffer_state_t ia_css_frame_get_buffer_state(
	const ia_css_frame_t *frame);

/*! Set the data buffer state of the frame object

 @param	frame[in]			frame object
 @param	buffer_state[in]		buffer state

 @return < 0 on error
 */
IA_CSS_PSYS_DATA_STORAGE_CLASS_H
int ia_css_frame_set_buffer_state(ia_css_frame_t *frame,
	const ia_css_buffer_state_t buffer_state);

/*! Get the data pointer state from the frame object

 @param	frame[in]			frame object

 @return pointer state, limit value on error
 */
IA_CSS_PSYS_DATA_STORAGE_CLASS_H
ia_css_pointer_state_t ia_css_frame_get_pointer_state(
	const ia_css_frame_t *frame);

/*! Set the data pointer state of the frame object

 @param	frame[in]			frame object
 @param	pointer_state[in]		pointer state

 @return < 0 on error
 */
IA_CSS_PSYS_DATA_STORAGE_CLASS_H
int ia_css_frame_set_pointer_state(ia_css_frame_t *frame,
	const ia_css_pointer_state_t pointer_state);

/*! Print the frame descriptor object to file/stream

 @param	frame_descriptor[in]	frame descriptor object
 @param	fid[out]		file/stream handle

 @return < 0 on error
 */
IA_CSS_PSYS_DATA_STORAGE_CLASS_H
int ia_css_frame_descriptor_print(
	const ia_css_frame_descriptor_t *frame_descriptor, void *fid);

/*! Print the fragment descriptor object to file/stream

 @param	fragment_descriptor[in]	fragment descriptor object
 @param	fid[out]		file/stream handle

 @return < 0 on error
 */
IA_CSS_PSYS_DATA_STORAGE_CLASS_H
int ia_css_fragment_descriptor_print(
	const ia_css_fragment_descriptor_t *fragment_descriptor, void *fid);

/*! Compute the bitmap for the frame format type

 @param	frame_format_type[in]	frame format type

 @return 0 on error
 */
IA_CSS_PSYS_DATA_STORAGE_CLASS_H
ia_css_frame_format_bitmap_t ia_css_frame_format_bit_mask(
	const ia_css_frame_format_type_t frame_format_type);

/*! clear frame format bitmap

 @return cleared bitmap
 */
IA_CSS_PSYS_DATA_STORAGE_CLASS_H
ia_css_frame_format_bitmap_t ia_css_frame_format_bitmap_clear(void);


/*! Compute the size of storage required for the data descriptor object
 * on a terminal
 *@param       plane_count[in]	The number of data planes in the buffer
 */
IA_CSS_PSYS_DATA_STORAGE_CLASS_H
size_t ia_css_sizeof_frame_descriptor(
		const uint8_t plane_count);
/*! Compute the size of storage required for the kernel parameter descriptor
 * object on a terminal

 @param	section_count[in]	The number of parameter sections in the buffer

 @return 0 on error
 */
extern size_t ia_css_sizeof_kernel_param_descriptor(
	const uint16_t section_count);

#ifdef _IA_CSS_PSYS_DATA_INLINE_
#include "ia_css_program_group_data_impl.h"
#endif /* _IA_CSS_PSYS_DATA_INLINE_ */

#endif /* __IA_CSS_PROGRAM_GROUP_DATA_H */
