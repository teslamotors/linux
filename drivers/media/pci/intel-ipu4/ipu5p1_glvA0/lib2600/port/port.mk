# INTEL CONFIDENTIAL
#
# Copyright (C) 2013 - 2015 Intel Corporation.
# All Rights Reserved.
#
# The source code contained or described herein and all documents
# related to the source code ("Material") are owned by Intel Corporation
# or licensors. Title to the Material remains with Intel
# Corporation or its licensors. The Material contains trade
# secrets and proprietary and confidential information of Intel or its
# licensors. The Material is protected by worldwide copyright
# and trade secret laws and treaty provisions. No part of the Material may
# be used, copied, reproduced, modified, published, uploaded, posted,
# transmitted, distributed, or disclosed in any way without Intel's prior
# express written permission.
#
# No License under any patent, copyright, trade secret or other intellectual
# property right is granted to or conferred upon you by disclosure or
# delivery of the Materials, either expressly, by implication, inducement,
# estoppel or otherwise. Any license under such intellectual property rights
# must be express and approved by Intel in writing.
#
# MODULE is PORT

PORT_DIR=$${MODULES_DIR}/port

PORT_INTERFACE=$(PORT_DIR)/interface
PORT_SOURCES1=$(PORT_DIR)/src

PORT_HOST_FILES += $(PORT_SOURCES1)/send_port.c
PORT_HOST_FILES += $(PORT_SOURCES1)/recv_port.c
PORT_HOST_FILES += $(PORT_SOURCES1)/queue.c

PORT_HOST_CPPFLAGS += -I$(PORT_INTERFACE)

PORT_FW_FILES += $(PORT_SOURCES1)/send_port.c
PORT_FW_FILES += $(PORT_SOURCES1)/recv_port.c

PORT_FW_CPPFLAGS += -I$(PORT_INTERFACE)
