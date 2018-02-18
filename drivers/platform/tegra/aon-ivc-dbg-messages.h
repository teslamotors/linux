/*
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef _AON_IVC_DBG_MESSAGES_H_
#define _AON_IVC_DBG_MESSAGES_H_

#include <linux/types.h>

/* All the enums and the fields inside the structs described in this header
 * file supports only uX type, where X can be 8,16,32.
 */

/* This enum represents the types of power management requests assocaited
 * with AON.
 */
enum aon_pm_request {
	/* Query/set the AON power state */
	AON_PM_TARGET_POWER_STATE = 0,
	/* Query/set the threshold of the specific power state on AON */
	AON_PM_THRESHOLD = 1,
	/* Query/set the wake timeout of AON */
	AON_PM_WAKE_TIMEOUT = 2,
	/* Force AON to sleep */
	AON_PM_FORCE_SLEEP = 3,
	/* Extract the number of times each power state of AON in entered */
	AON_PM_STATE_COUNT = 4,
	/* Extract the amount of time spent in each power state of AON */
	AON_PM_STATE_TIME = 5,
	/* Query/set whether VDD_RTC should go to retention in deep dormant */
	AON_PM_VDD_RTC_RETENTION = 9,
	/* Query the number of times SPE entered SC8 */
	AON_PM_SC8_COUNT = 10,
};

/* This enum indicates the power states of the target.
 * Excluding active, we have 6 power states.
 */
enum aon_pm_states {
	/* shallow power states */
	AON_IDLE = 0,
	AON_STANDBY = 1,
	AON_DORMANT = 2,
	/* deep power states */
	AON_DEEP_IDLE = 3,
	AON_DEEP_STANDBY = 4,
	AON_DEEP_DORMANT = 5,
	/* active power state */
	AON_ACTIVE = 6,
};

/* This enum indicates the power states entries/exits of the
 * target.
 */
enum aon_pstate_action {
	AON_PSTATE_ENTRY = 0,
	AON_PSTATE_EXIT = 1,
};

/* This indicates the status of the IVC request sent from
 * CCPLEX was successful or not.
 */
enum aon_dbg_status {
	AON_DBG_STATUS_OK = 0,
	AON_DBG_STATUS_ERROR = 1,
};

/* The following enum indicates the mods request and response
 * status messages.
 */
enum aon_mods {
	AON_MODS_LOOPS_TEST = 6,
	AON_MODS_RESULT = 7,
	AON_MODS_CRC = 11,
	AON_MODS_STATUS_OK = 0,
	AON_MODS_STATUS_ERROR = 1,
};

/* The following enum indicates the ping request from ccplex to
 * SPE. SPE returns the fw version of SPE in the data field and
 * 0 in the status field.
 */
enum aon_ping {
	AON_PING_TEST = 8,
	AON_REQUEST_TYPE_MAX = 11,
};

/* The following enum indicates if we are reading from or writing
 * to the target i.e AON.
 */
enum aon_xfer_flag {
	AON_XFER_FLAG_READ = 0,
	AON_XFER_FLAG_WRITE = 1,
};

/* This struct is used to query or set the threshold of a power state
 * on the target.
 * Fields:
 * flag:	to indicate read or write
 * state:	target power state
 */
struct aon_pm_threshold {
	/* enum aon_xfer_flag */
	u16 flag;
	/* enum aon_pm_states */
	u16 state;
	u32 val;
};

/* This struct is used to query or set the target power state.
 * Fields:
 * flag:	to indicate read or write
 * state:	target power state
 */
struct aon_pm_target_pstate {
	/* enum aon_xfer_flag */
	u16 flag;
	/* enum aon_pm_states */
	u16 state;
};

/* This struct is used to query or set the wake timeout for the target.
 * Fields:
 * flag:	to indicate read or write
 * timeout:	timeout to wake
 */
struct aon_pm_wake_timeout {
	/* enum aon_xfer_flag */
	u32 flag;
	u32 timeout;
};

/* This struct is used to query or set the wake timeout for the target.
 * Fields:
 * force_entry:	when set forces the target to sleep
 */
struct aon_pm_force_sleep {
	u32 force_entry;
};

/* This struct is used to extract the time spent in each power state.
 * Fields:
 * state_durations:		duration of shallow/deep/active states indexed
 *				by enum	aon_pm_states, in ms
 */
struct aon_pm_state_time {
	u64 state_durations[6];
};

/* This struct is used to extract the power state entry counts for the target.
 * Fields:
 * state_counts:		number of times each power state is entered
 */
struct aon_pm_state_count {
	u32 state_entry_counts[6];
};

/* This struct is used to query or set whether VDD_RTC will be lowered to
   retention level when SPE is in deep dormant state.
 * Fields:
 * flag:	to indicate read or write
 * enable:	to indicate whether retention is enabled or not
 */
struct aon_pm_vdd_rtc_retention {
	/* enum aon_xfer_flag */
	u16 flag;
	u16 enable;
};

/* This struct is used to query the number of times SPE entered SC8.
 * Fields:
 * count:	sc8 count
 */

struct aon_pm_sc8_count {
	u32 count;
};
/* This struct is used to send the loop count to perform the mods test
 * on the target.
 * Fields:
 * loops:	number of times mods test should be run
 */
struct aon_mods_xfer {
	u32 loops;
};

/* This struct is used to send the CRC32 of the SPE text section to the target.
 * Fields:
 * crc:		CRC32 of the text section.
 */
struct aon_mods_crc_xfer {
	uint32_t crc;
};

/* This struct is used to extract the firmware version of the SPE.
 * Fields:
 * data:	buffer to store the version string. Uses u8
 */
struct aon_ping_xfer {
	u8 data[64];
};

/* This struct encapsulates all the pm debug requests into an union */
struct aon_pm_dbg_xfer {
	union {
		struct aon_pm_threshold threshold;
		struct aon_pm_target_pstate pstate;
		struct aon_pm_wake_timeout wake_tout;
		struct aon_pm_force_sleep force_sleep;
		struct aon_pm_state_time state_times;
		struct aon_pm_state_count state_counts;
		struct aon_pm_vdd_rtc_retention retention;
		struct aon_pm_sc8_count sc8_count;
	} type;
};

/* This struct encapsulates the type of the request and the respective
 * data associated with that request.
 * Fields:
 * req_type:	indicates the type of the request be it pm related,
 *		mods or ping.
 * data:	Union of structs of all the request types.
 */
struct aon_dbg_request {
	/* enum aon_pm_request, aon_mods_request or ping */
	u32 req_type;
	union {
		struct aon_pm_dbg_xfer pm_xfer;
		struct aon_mods_xfer mods_xfer;
		struct aon_ping_xfer ping_xfer;
	} data;
};

/* This struct encapsulates the type of the response and the respective
 * data associated with that response.
 * Fields:
 * resp_type:	indicates the type of the response be it pm related,
 *		mods or ping.
 * status:	response in regard to the request i.e success/failure.
 *		In case of mods, this field is the result.
 * data:	Union of structs of all the request/response types.
 */
struct aon_dbg_response {
	/* enum aon_pm_request, aon_mods_request or ping */
	u32 resp_type;
	u32 status;
	union {
		struct aon_pm_dbg_xfer pm_xfer;
		struct aon_ping_xfer ping_xfer;
		struct aon_mods_crc_xfer crc_xfer;
	} data;
};

#endif
