/* drivers/gpu/arm/.../platform/gpu_notifier.h
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali-T Series platform-dependent codes
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file gpu_notifier.h
 */

#ifndef _GPU_NOTIFIER_H_
#define _GPU_NOTIFIER_H_

int gpu_notifier_init(struct kbase_device *kbdev);
void gpu_notifier_term(void);

#endif /* _GPU_NOTIFIER_H_ */
