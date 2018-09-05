/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0) */
/*
 * Copyright (C) 2018 Intel Corporation
 */

#ifndef __CRLMODULE_SENSOR_DS_H_
#define __CRLMODULE_SENSOR_DS_H_

#include "crlmodule.h"

#define CRL_SUBDEVS				3

/* Index for subdevs in any structure with multiple SDs */
#define CRL_SD_PA_INDEX				0
#define CRL_SD_BINNER_INDEX			1
#define CRL_SD_SCALER_INDEX			2

#define CRL_REG_LEN_08BIT			1
#define CRL_REG_LEN_16BIT			2
#define CRL_REG_LEN_24BIT			3
#define CRL_REG_LEN_32BIT			4

#define CRL_REG_READ_AND_UPDATE			(1 << 3)
#define CRL_REG_LEN_READ_MASK			0x07
#define CRL_REG_LEN_DELAY			0x10

#define CRL_FLIP_DEFAULT_NONE			0
#define CRL_FLIP_HFLIP				1
#define CRL_FLIP_VFLIP				2
#define CRL_FLIP_HFLIP_VFLIP			3

#define CRL_FLIP_HFLIP_MASK			0xfe
#define CRL_FLIP_VFLIP_MASK			0xfd

#define CRL_PIXEL_ORDER_GRBG			0
#define CRL_PIXEL_ORDER_RGGB			1
#define CRL_PIXEL_ORDER_BGGR			2
#define CRL_PIXEL_ORDER_GBRG			3
#define CRL_PIXEL_ORDER_IGNORE                  255

/* Flag to notify configuration selction imact from Ctrls */
#define CRL_IMPACTS_NO_IMPACT			0
#define CRL_IMPACTS_PLL_SELECTION		(1 << 1)
#define CRL_IMPACTS_MODE_SELECTION		(1 << 2)

/*
 * In crl_dynamic_entity::entity_type is denoted by bits 6 and 7
 * 0 -> crl_dynamic_entity:entity_value is a constant
 * 1 -> crl_dynamic_entity:entity_value is a referene to variable
 * 2 -> crl_dynamic_entity:entity_value is a ctrl value
 * 3 -> crl_dynamic_entity:entity_value is a 8 bit register address
 */
enum crl_dynamic_entity_type {
	CRL_DYNAMIC_VAL_OPERAND_TYPE_CONST = 0,
	CRL_DYNAMIC_VAL_OPERAND_TYPE_VAR_REF,
	CRL_DYNAMIC_VAL_OPERAND_TYPE_CTRL_VAL,
	CRL_DYNAMIC_VAL_OPERAND_TYPE_REG_VAL, /* Only 8bit registers */
};

/*
 * For some combo device which has some devices inside itself with different
 * i2c address, adding flag to specify whether current device needs i2c
 * address override.
 * For back-compatibility, making flag equals 0. So existing sensor configure
 * doesn't need to be modified.
 */
#define CRL_I2C_ADDRESS_NO_OVERRIDE		0

struct crl_sensor;
struct i2c_client;

enum crl_subdev_type {
	CRL_SUBDEV_TYPE_SCALER,
	CRL_SUBDEV_TYPE_BINNER,
	CRL_SUBDEV_TYPE_PIXEL_ARRAY,
};

enum crl_ctrl_op_type {
	CRL_CTRL_SET_OP,
	CRL_CTRL_GET_OP,
};

enum crl_ctrl_update_context {
	SENSOR_IDLE, /* Powered on. But not streamind */
	SENSOR_STREAMING, /* Sensor streaming */
	SENSOR_POWERED_ON, /* streaming or idle */
};

enum crl_operators {
	CRL_BITWISE_AND = 0,
	CRL_BITWISE_OR,
	CRL_BITWISE_LSHIFT,
	CRL_BITWISE_RSHIFT,
	CRL_BITWISE_XOR,
	CRL_BITWISE_COMPLEMENT,
	CRL_ADD,
	CRL_SUBTRACT,
	CRL_MULTIPLY,
	CRL_DIV,
	CRL_ASSIGNMENT,
};

/* Replicated from videodev2.h */
enum crl_ctrl_type {
	CRL_CTRL_TYPE_INTEGER = 1,
	CRL_CTRL_TYPE_BOOLEAN,
	CRL_CTRL_TYPE_MENU_INT,
	CRL_CTRL_TYPE_MENU_ITEMS,
	CRL_CTRL_TYPE_BUTTON,
	CRL_CTRL_TYPE_INTEGER64,
	CRL_CTRL_TYPE_CTRL_CLASS,
	CRL_CTRL_TYPE_CUSTOM,
};

enum crl_addr_len {
	CRL_ADDR_16BIT = 0,
	CRL_ADDR_8BIT,
	CRL_ADDR_7BIT,
};

enum crl_operands {
	CRL_CONSTANT = 0,
	CRL_VARIABLE,
	CRL_CONTROL,
};

/* References to the CRL driver member variables */
enum crl_member_data_reference_ids {
	CRL_VAR_REF_OUTPUT_WIDTH = 1,
	CRL_VAR_REF_OUTPUT_HEIGHT,
	CRL_VAR_REF_PA_CROP_WIDTH,
	CRL_VAR_REF_PA_CROP_HEIGHT,
	CRL_VAR_REF_FRAME_TIMING_WIDTH,
	CRL_VAR_REF_FRAME_TIMING_HEIGHT,
	CRL_VAR_REF_BINNER_WIDTH,
	CRL_VAR_REF_BINNER_HEIGHT,
	CRL_VAR_REF_H_BINN_FACTOR,
	CRL_VAR_REF_V_BINN_FACTOR,
	CRL_VAR_REF_SCALE_FACTOR,
	CRL_VAR_REF_BITSPERPIXEL,
	CRL_VAR_REF_PIXELRATE_PA,
	CRL_VAR_REF_PIXELRATE_CSI,
	CRL_VAR_REF_PIXELRATE_LINK_FREQ,
};

enum crl_frame_desc_type {
	CRL_MBUS_FRAME_DESC_TYPE_PLATFORM,
	CRL_MBUS_FRAME_DESC_TYPE_PARALLEL,
	CRL_MBUS_FRAME_DESC_TYPE_CCP2,
	CRL_MBUS_FRAME_DESC_TYPE_CSI2,
};

enum crl_pwr_ent_type {
	CRL_POWER_ETY_GPIO_FROM_PDATA = 1,
	CRL_POWER_ETY_GPIO_CUSTOM,
	CRL_POWER_ETY_REGULATOR_FRAMEWORK,
	CRL_POWER_ETY_CLK_FRAMEWORK,
};

struct crl_dynamic_entity {
	enum crl_dynamic_entity_type entity_type;
	u32 entity_val;
};

struct crl_arithmetic_ops {
	enum crl_operators op;
	struct crl_dynamic_entity operand;
};

struct crl_dynamic_calculated_entity {
	u8 ops_items;
	struct crl_arithmetic_ops *ops;
};

struct crl_register_write_rep {
	u16 address;
	u8 len;
	u32 val;
	u16 dev_i2c_addr;
	u32 mask;
};

struct crl_register_read_rep {
	u16 address;
	u8 len;
	u32 mask;
	u16 dev_i2c_addr;
};

/*
 * crl_dynamic_register_access is used mainly in the ctrl context.
 * This is intended to provide some generic arithmetic operations on the values
 * to be written to a control's register or on the values read from a register.
 * These arithmetic operations are controlled using struct crl_arithmetic_ops.
 *
 * One important information is that this structure behave differently for the
 * set controls and volatile get controls.
 *
 * For the set control operation, the usage of the members are straight forward.
 * The set control can result into multiple register write operations. Hence
 * there can be more than one crl_dynamic_register_access entries associated
 * with a control which results into separate register writes.
 *
 * But for the volatile get control operation, where a control is used
 * to query read only information from the sensor, there could be only one
 * crl_dynamic_register_access entry. Because the result of a get control is
 * a single value. crl_dynamic_register_access.address, len and mask values are
 * not used in volatile get control context. Instead all the needed information
 * must be encoded into member -> ops (struct crl_arithmetic_ops)
 */
struct crl_dynamic_register_access {
	u16 address;
	u8 len;
	u32 mask;
	u8 ops_items;
	struct crl_arithmetic_ops *ops;
	u16 dev_i2c_addr;
};

struct crl_sensor_detect_config {
	struct crl_register_read_rep reg; /* Register to read */
	unsigned int width; /* width of the value in chars*/
};

struct crl_sensor_subdev_config {
	enum crl_subdev_type subdev_type;
	char name[32];
};

enum crl_ctrl_flag {
	CRL_CTRL_FLAG_UPDATE = 1,
	CRL_CTRL_FLAG_READ_ONLY = 2,
	CRL_CTRL_FLAG_WRITE_ONLY = 4,
};

/*
 * The ctrl id value pair which should be compared when selecting a
 * configuration. This gives flexibility to provide any data through set ctrl
 * and provide selection mechanism for a particular configuration
 */
struct crl_ctrl_data_pair {
	u32 ctrl_id;
	u32 data;
};

enum crl_dep_ctrl_action_type {
	CRL_DEP_CTRL_ACTION_TYPE_SELF = 0,
	CRL_DEP_CTRL_ACTION_TYPE_DEP_CTRL,
};

enum crl_dep_ctrl_condition {
	CRL_DEP_CTRL_CONDITION_GREATER = 0,
	CRL_DEP_CTRL_CONDITION_LESSER,
	CRL_DEP_CTRL_CONDITION_EQUAL,
};

enum crl_dep_ctrl_action {
	CRL_DEP_CTRL_CONDITION_ADD = 0,
	CRL_DEP_CTRL_CONDITION_SUBTRACT,
	CRL_DEP_CTRL_CONDITION_MULTIPLY,
	CRL_DEP_CTRL_CONDITION_DIVIDE,
};

struct crl_dep_ctrl_cond_action {
	enum crl_dep_ctrl_condition cond;
	u32 cond_value;
	enum crl_dep_ctrl_action action;
	u32 action_value;
};

/* Dependency control provision */
struct crl_dep_ctrl_provision {
	u32 ctrl_id;
	enum crl_dep_ctrl_action_type action_type;
	unsigned int action_items;
	struct crl_dep_ctrl_cond_action *action;
};

struct crl_sensor_limits {
	unsigned int x_addr_max;
	unsigned int y_addr_max;
	unsigned int x_addr_min;
	unsigned int y_addr_min;
	unsigned int min_frame_length_lines;
	unsigned int max_frame_length_lines;
	unsigned int min_line_length_pixels;
	unsigned int max_line_length_pixels;
	u8 scaler_m_min;
	u8 scaler_m_max;
	u8 scaler_n_min;
	u8 scaler_n_max;
	u8 min_even_inc;
	u8 max_even_inc;
	u8 min_odd_inc;
	u8 max_odd_inc;
};

struct crl_ctrl_data_std {
	s64 min;
	s64 max;
	u64 step;
	s64 def;
};

struct crl_ctrl_data_menu_items {
	const char *const *menu;
	unsigned int size;
};

struct crl_ctrl_data_int_menu {
	const s64 *menu;
	s64 max;
	s64 def;
};

union crl_ctrl_data_types {
	struct crl_ctrl_data_std std_data;
	struct crl_ctrl_data_menu_items menu_items;
	struct crl_ctrl_data_int_menu int_menu;
};

/*
 * Please note a difference in the usage of "regs" member in case of a
 * volatile get control for read only purpose. Please check the
 * "struct crl_dynamic_register_access" declaration comments for more details.
 *
 * Read only controls must have "flags" CRL_CTRL_FLAG_READ_ONLY set.
 */
struct crl_ctrl_data {
	enum crl_subdev_type sd_type;
	enum crl_ctrl_op_type op_type;
	enum crl_ctrl_update_context context;
	char name[32];
	u32 ctrl_id;
	enum crl_ctrl_type type;
	union crl_ctrl_data_types data;
	unsigned long flags;
	u32 impact; /* If this control impact any config selection */
	struct ici_ext_sd_param param;
	bool enabled;
	unsigned int regs_items;
	struct crl_dynamic_register_access *regs;
	unsigned int dep_items;
	struct crl_dep_ctrl_provision *dep_ctrls;
	s64 min;
	s64 max;
	u64 step;
	s64 def;
};

struct crl_pll_configuration {
	s64 input_clk;
	s64 op_sys_clk;
	u8 bitsperpixel;
	u32 pixel_rate_csi;
	u32 pixel_rate_pa;
	u8 csi_lanes;
	unsigned int comp_items;
	struct crl_ctrl_data_pair *ctrl_data;
	unsigned int pll_regs_items;
	const struct crl_register_write_rep *pll_regs;
};

struct crl_subdev_rect_rep {
	enum crl_subdev_type subdev_type;
	struct ici_rect in_rect;
	struct ici_rect out_rect;
};

struct crl_mode_rep {
	unsigned int sd_rects_items;
	const struct crl_subdev_rect_rep *sd_rects;
	u8 binn_hor;
	u8 binn_vert;
	u8 scale_m;
	s32 width;
	s32 height;
	unsigned int comp_items;
	struct crl_ctrl_data_pair *ctrl_data;
	unsigned int mode_regs_items;
	const struct crl_register_write_rep *mode_regs;

	/*
	 * Minimum and maximum value for line length pixels and frame length
	 * lines are added for modes. This facilitates easy handling of
	 * modes which binning skipping and affects the calculation of vblank and
	 * hblank values.
	 *
	 * The blank values are limited based on the following logic
	 *
	 * If mode specific limits are available
	 * vblank = clamp(min_llp - PA_width, max_llp - PA_width)
	 * hblank = clamp(min_fll - PA_Height, max_fll - PA_Height
	 *
	 * If mode specific blanking limits are not available, then the sensor
	 * limits will be used in the same manner.
	 *
	 * If sensor mode limits are not available, then the values will be
	 * written directly to the associated control registers.
	 */
	s32 min_llp; /* minimum/maximum value for line length pixels */
	s32 max_llp;
	s32 min_fll;
	s32 max_fll; /* minimum/maximum value for frame length lines */
};

struct crl_csi_data_fmt {
	u32 code;
	u8 pixel_order;
	u8 bits_per_pixel;
	unsigned int regs_items;
	const struct crl_register_write_rep *regs;
};

struct crl_flip_data {
	u8 flip;
	u8 pixel_order;
};

struct crl_power_seq_entity {
	enum crl_pwr_ent_type type;
	char ent_name[12];
	int ent_number;
	u16 address;
	unsigned int val;
	unsigned int undo_val; /* Undo value if any previous step failed */
	unsigned int delay; /* delay in micro seconds */
	struct regulator *regulator_priv; /* R/W */
};

struct crl_nvm_blob {
	u8 dev_addr;
	u16 start_addr;
	u16 size;
};

struct crl_nvm {
	unsigned int nvm_preop_regs_items;
	const struct crl_register_write_rep *nvm_preop_regs;

	unsigned int nvm_postop_regs_items;
	const struct crl_register_write_rep *nvm_postop_regs;

	unsigned int nvm_blobs_items;
	struct crl_nvm_blob *nvm_config;
	u32 nvm_flags;
};

/* Representation for v4l2_mbus_frame_desc_entry */
struct crl_frame_desc {
	struct crl_dynamic_entity flags;
	struct crl_dynamic_entity bpp;
	struct crl_dynamic_entity pixelcode;
	struct crl_dynamic_entity start_line;
	struct crl_dynamic_entity start_pixel;
	struct crl_dynamic_calculated_entity width;
	struct crl_dynamic_calculated_entity height;
	struct crl_dynamic_entity length;
	struct crl_dynamic_entity csi2_channel;
	struct crl_dynamic_entity csi2_data_type;
};

typedef int (*sensor_specific_init)(struct i2c_client*);
typedef int (*sensor_specific_cleanup)(struct i2c_client*);

struct crl_sensor_configuration {

	const struct crl_clock_entity *clock_entity;

	const unsigned int power_items;
	const struct crl_power_seq_entity *power_entities;
	const unsigned int power_delay; /* in micro seconds */

	const unsigned int onetime_init_regs_items;
	const struct crl_register_write_rep *onetime_init_regs;

	const unsigned int powerup_regs_items;
	const struct crl_register_write_rep *powerup_regs;

	const unsigned int poweroff_regs_items;
	const struct crl_register_write_rep *poweroff_regs;

	const unsigned int id_reg_items;
	const struct crl_sensor_detect_config *id_regs;

	const unsigned int subdev_items;
	const struct crl_sensor_subdev_config *subdevs;

	const struct crl_sensor_limits *sensor_limits;

	const unsigned int pll_config_items;
	const struct crl_pll_configuration *pll_configs;

	const unsigned int modes_items;
	const struct crl_mode_rep *modes;
	/*
	 * Fail safe mode should be the largest resolution available in the
	 * mode list. If none of the mode parameters are matched, the driver
	 * will select this mode for streaming.
	 */
	const unsigned int fail_safe_mode_index;

	const unsigned int streamon_regs_items;
	const struct crl_register_write_rep *streamon_regs;

	const unsigned int streamoff_regs_items;
	const struct crl_register_write_rep *streamoff_regs;

	const unsigned int ctrl_items;
	const struct crl_ctrl_data *ctrl_bank;

	const unsigned int csi_fmts_items;
	const struct crl_csi_data_fmt *csi_fmts;

	const unsigned int flip_items;
	const struct crl_flip_data *flip_data;

	struct crl_nvm crl_nvm_info;

	enum crl_addr_len addr_len;

	unsigned int frame_desc_entries;
	enum crl_frame_desc_type frame_desc_type;
	struct crl_frame_desc *frame_desc;
	char *msr_file_name;

	sensor_specific_init sensor_init;
	sensor_specific_cleanup sensor_cleanup;
};

struct crlmodule_sensors {
	char *pname;
	char *name;
	struct crl_sensor_configuration *ds;
};

/*
 * Function to populate the CRL data structure from the sensor configuration
 * definition file
 */
int crlmodule_populate_ds(struct crl_sensor *sensor, struct device *dev);

/*
 * Function validate the contents CRL data structure to check if all the
 * required fields are filled and are according to the limits.
 */
int crlmodule_validate_ds(struct crl_sensor *sensor);

/* Function to free all resources allocated for the CRL data structure */
void crlmodule_release_ds(struct crl_sensor *sensor);

#endif /* __CRLMODULE_SENSOR_DS_H_ */
