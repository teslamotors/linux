/**
 * INTEL CONFIDENTIAL
 *
 * Copyright (C) 2015 - 2016 Intel Corporation.
 * All Rights Reserved.
 *
 * The source code contained or described herein and all documents
 * related to the source code ("Material") are owned by Intel Corporation
 * or licensors. Title to the Material remains with Intel
 * Corporation or its licensors. The Material contains trade
 * secrets and proprietary and confidential information of Intel or its
 * licensors. The Material is protected by worldwide copyright
 * and trade secret laws and treaty provisions. No part of the Material may
 * be used, copied, reproduced, modified, published, uploaded, posted,
 * transmitted, distributed, or disclosed in any way without Intel's prior
 * express written permission.
 *
 * No License under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or
 * delivery of the Materials, either expressly, by implication, inducement,
 * estoppel or otherwise. Any license under such intellectual property rights
 * must be express and approved by Intel in writing.
 */

/*
 * TODO: IPU5_SDK Verify that this file is correct when IPU5 SDK has been released.
 */

#ifndef _IPU_DEVICE_CELL_DEVICES_H
#define _IPU_DEVICE_CELL_DEVICES_H

/* define cell instances in ISYS */

#define SPC0_CELL ipu_sp_control_tile_is_sp

enum ipu_device_isys_cell_id {
	SPC0
};
#define NUM_CELLS (SPC0 + 1)

#endif /* _IPU_DEVICE_CELL_DEVICES_H_ */
