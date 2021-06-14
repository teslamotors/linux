#ifndef DC_LINK_OVERRIDES
#define DC_LINK_OVERRIDES

#include "dc_dp_types.h"

const static enum dc_voltage_swing static_vs[] = {
	VOLTAGE_SWING_LEVEL0,
	VOLTAGE_SWING_LEVEL1,
	VOLTAGE_SWING_LEVEL2,
	VOLTAGE_SWING_LEVEL3
};

const static enum dc_pre_emphasis static_pe[] = {
	PRE_EMPHASIS_DISABLED,
	PRE_EMPHASIS_LEVEL1,
	PRE_EMPHASIS_LEVEL2,
	PRE_EMPHASIS_LEVEL3
};

const static enum dc_post_cursor2 static_pc[] = {
	POST_CURSOR2_DISABLED,
	POST_CURSOR2_LEVEL1,
	POST_CURSOR2_LEVEL2,
	POST_CURSOR2_LEVEL3
};

#define STATIC_VS(x) &static_vs[x]
#define STATIC_PE(x) &static_pe[x]
#define STATIC_PC(x) &static_pc[x]


#ifndef CONFIG_DRM_AMD_DC_LINK_TRAIN_OVERRIDE
const static struct dc_link_training_overrides dc_overrides[MAX_PIPES] = { 0 };
#else
/* Tesla RevA Phy Setting Overrides
 * These overridden lane settings were empirically choosen for the
 * Tesla Rev A MCU + Hirose connector to Display. The exact settings
 * needed to be adjusted due to cable noise and extended length that would
 * cause consistent flickering even after successfully lane training.
 * See Tesla internal : ECET-10499 for more information.
 */
const static struct dc_link_training_overrides dc_overrides[MAX_PIPES] = {
	{ 0 }, /* Display Port 1 */
	{ 0 }, /* HDMI-A-1 */
	{
		.voltage_swing = STATIC_VS(VOLTAGE_SWING_LEVEL2),
		.pre_emphasis = STATIC_PE(PRE_EMPHASIS_LEVEL1),
		.post_cursor2 = NULL
	},     /* Display Port 2*/
	{ 0 }, /* DDI4 */
	{ 0 }, /* DDI5 */
	{ 0 }, /* DDI6 */
};
#endif

#endif
