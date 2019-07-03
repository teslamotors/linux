/*
 * Copyright (c) 2016 Intel Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#ifndef __DRM_BLEND_H__
#define __DRM_BLEND_H__

#include <linux/list.h>
#include <linux/ctype.h>
#include <drm/drm_mode.h>

struct drm_device;
struct drm_atomic_state;
struct drm_plane;

/**
 * struct drm_rgba - RGBA property value type
 * @v: Internal representation of RGBA, stored in 16bpc format
 *
 * Structure to abstract away the representation of RGBA values with precision
 * up to 16 bits per color component.  This is primarily intended for use with
 * DRM properties that need to take a color value since we don't want userspace
 * to have to worry about the bit layout expected by the underlying hardware.
 *
 * We wrap the value in a structure here so that the compiler will flag any
 * accidental attempts by driver code to directly attempt bitwise operations
 * that could potentially misinterpret the value.  Drivers should instead use
 * the DRM_RGBA_{RED,GREEN,BLUE,ALPHA}BITS() macros to obtain the component
 * bits and then build values in the format their hardware expects.
 */
struct drm_rgba {
	uint64_t v;
};

/*
 * Rotation property bits. DRM_ROTATE_<degrees> rotates the image by the
 * specified amount in degrees in counter clockwise direction. DRM_REFLECT_X and
 * DRM_REFLECT_Y reflects the image along the specified axis prior to rotation
 *
 * WARNING: These defines are UABI since they're exposed in the rotation
 * property.
 */
#define DRM_ROTATE_0	BIT(0)
#define DRM_ROTATE_90	BIT(1)
#define DRM_ROTATE_180	BIT(2)
#define DRM_ROTATE_270	BIT(3)
#define DRM_ROTATE_MASK (DRM_ROTATE_0   | DRM_ROTATE_90 | \
			 DRM_ROTATE_180 | DRM_ROTATE_270)
#define DRM_REFLECT_X	BIT(4)
#define DRM_REFLECT_Y	BIT(5)
#define DRM_REFLECT_MASK (DRM_REFLECT_X | DRM_REFLECT_Y)

static inline bool drm_rotation_90_or_270(unsigned int rotation)
{
	return rotation & (DRM_MODE_ROTATE_90 | DRM_MODE_ROTATE_270);
}

int drm_plane_create_rotation_property(struct drm_plane *plane,
				       unsigned int rotation,
				       unsigned int supported_rotations);
unsigned int drm_rotation_simplify(unsigned int rotation,
				   unsigned int supported_rotations);

struct drm_property *drm_mode_create_background_color_property(struct drm_device *dev);

int drm_plane_create_zpos_property(struct drm_plane *plane,
				   unsigned int zpos,
				   unsigned int min, unsigned int max);
int drm_plane_create_zpos_immutable_property(struct drm_plane *plane,
					     unsigned int zpos);
int drm_atomic_normalize_zpos(struct drm_device *dev,
			      struct drm_atomic_state *state);

/**
 * drm_rgba - Build RGBA property values
 * @bpc: Bits per component value for @red, @green, @blue, and @alpha parameters
 * @red: Red component value
 * @green: Green component value
 * @blue: Blue component value
 * @alpha: Alpha component value
 *
 * Helper to build RGBA 16bpc color values with the bits laid out in the format
 * expected by DRM RGBA properties.
 *
 * Returns:
 * An RGBA structure encapsulating the requested RGBA value.
 */
static inline struct drm_rgba
drm_rgba(unsigned bpc,
	 uint16_t red,
	 uint16_t green,
	 uint16_t blue,
	 uint16_t alpha)
{
	int shift;
	struct drm_rgba val;

	if (WARN_ON(bpc > 16))
		bpc = 16;

	/*
	 * If we were provided with fewer than 16 bpc, shift the value we
	 * received into the most significant bits.
	 */
	shift = 16 - bpc;

	val.v = red << shift;
	val.v <<= 16;
	val.v |= green << shift;
	val.v <<= 16;
	val.v |= blue << shift;
	val.v <<= 16;
	val.v |= alpha << shift;

	return val;
}

static inline uint16_t
drm_rgba_bits(struct drm_rgba c, unsigned compshift, unsigned bits) {
	uint64_t val;

	if (WARN_ON(bits > 16))
		bits = 16;

	val = c.v & GENMASK_ULL(compshift + 15, compshift);
	return val >> (compshift + 16 - bits);
}

/*
 * Macros to access the individual color components of an RGBA property value.
 * If the requested number of bits is less than 16, only the most significant
 * bits of the color component will be returned.
 */
#define DRM_RGBA_REDBITS(c, bits)   drm_rgba_bits(c, 48, bits)
#define DRM_RGBA_GREENBITS(c, bits) drm_rgba_bits(c, 32, bits)
#define DRM_RGBA_BLUEBITS(c, bits)  drm_rgba_bits(c, 16, bits)
#define DRM_RGBA_ALPHABITS(c, bits) drm_rgba_bits(c, 0, bits)

enum drm_blend_factor {
	DRM_BLEND_FACTOR_AUTO,
	DRM_BLEND_FACTOR_ZERO,
	DRM_BLEND_FACTOR_ONE,
	DRM_BLEND_FACTOR_SRC_ALPHA,
	DRM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
	DRM_BLEND_FACTOR_CONSTANT_ALPHA,
	DRM_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA,
	DRM_BLEND_FACTOR_CONSTANT_ALPHA_TIMES_SRC_ALPHA,
	DRM_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA_TIMES_SRC_ALPHA,
};

#define DRM_BLEND_FUNC(src_factor, dst_factor)         \
	(DRM_BLEND_FACTOR_##src_factor << 16 | DRM_BLEND_FACTOR_##dst_factor)
#define DRM_BLEND_FUNC_SRC_FACTOR(val) (((val) >> 16) & 0xffff)
#define DRM_BLEND_FUNC_DST_FACTOR(val) ((val) & 0xffff)

#endif
