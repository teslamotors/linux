/**
* INTEL CONFIDENTIAL
*
* Copyright (C) 2013 - 2016 Intel Corporation.
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

#ifndef __SEND_PORT_H__
#define __SEND_PORT_H__


/*
 * A send port can be used to send tokens into a queue.
 * The interface can be used on any type of processor (host, SP, ...)
 */

struct send_port;
struct sys_queue;
struct port_env;

/*
 * Open a send port on a queue. After the port is opened, tokens can be sent
 */
void
send_port_open(struct send_port *p, const struct sys_queue *q, const struct port_env *env);

/*
 * Determine how many tokens can be sent
 */
unsigned int
send_port_available(const struct send_port *p);

/*
 * Send a token via a send port. The function returns the number of tokens that have been sent:
 * 1: the token was accepted
 * 0: the token was not accepted (full queue)
 * The size of a token is determined at initialization.
 */
unsigned int
send_port_transfer(const struct send_port *p, const void *data);


#endif /*__SEND_PORT_H__*/
