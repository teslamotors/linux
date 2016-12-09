#ifndef _TRUSTY_LOG_H_
#define _TRUSTY_LOG_H_

/*
 * Ring buffer that supports one secure producer thread and one
 * linux side consumer thread.
 */
struct log_rb {
	volatile uint32_t alloc;
	volatile uint32_t put;
	uint32_t sz;
	volatile char data[0];
} __packed;

#define SMC_SC_SHARED_LOG_VERSION	SMC_STDCALL_NR(SMC_ENTITY_LOGGING, 0)
#define SMC_SC_SHARED_LOG_ADD		SMC_STDCALL_NR(SMC_ENTITY_LOGGING, 1)
#define SMC_SC_SHARED_LOG_RM		SMC_STDCALL_NR(SMC_ENTITY_LOGGING, 2)

#define TRUSTY_LOG_API_VERSION	1

#define VMM_DUMP_VERSION 1

struct dump_data {
	uint32_t    length;
	uint8_t     data[0];
} __packed;

struct dump_header {
	uint8_t     vmm_version[64]; /* version of the vmm */
	uint8_t     signature[16];   /* signature for the dump structure */
	uint8_t     error_info[32];  /* filename:linenum */
	uint16_t    cpuid;
} __packed;

struct deadloop_dump {
	uint16_t    size_of_this_struct;
	uint16_t    version_of_this_struct;
	uint32_t    is_valid;
	struct dump_header    header;
	struct dump_data      data;
} __packed;

#endif

