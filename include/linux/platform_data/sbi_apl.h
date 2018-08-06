#ifndef SBI_APL_H
#define SBI_APL_H

#define SBI_ADDR_OFFSET 0xD0
#define SBI_DATA_OFFSET 0xD4
#define SBI_STAT_OFFSET 0xD8
#define SBI_ROUT_OFFSET 0xDA
#define SBI_EADD_OFFSET 0xDC

/* Status register */
#define SBI_STAT_POSTED_MASK      0x01
#define SBI_STAT_STATUS_MASK      0x03
#define SBI_STAT_BYTE_ENABLE_MASK 0x0F
#define SBI_STAT_BAR_MASK         0x07
#define SBI_STAT_BUSY_MASK        0x01

#define SBI_APL_SET_POSTED(msg, val) \
	((msg)->posted = ((val) & SBI_STAT_POSTED_MASK))
#define SBI_APL_SET_STATUS(msg, val) \
	((msg)->status = ((val) & SBI_STAT_STATUS_MASK))
#define SBI_APL_SET_BYTE_ENABLE(msg, val) \
	((msg)->byte_enable = ((val) & SBI_STAT_BYTE_ENABLE_MASK))
#define SBI_APL_SET_BAR(msg, val) \
	((msg)->base_address_register = ((val) & SBI_STAT_BAR_MASK))

#define SBI_APL_SET(msg, port, reg, data, opcode, \
		posted, status, be, bar, funct, eaddr) \
	do { \
		(msg)->port_address = (port); \
		(msg)->register_offset = (reg); \
		(msg)->data = (data); \
		SBI_APL_SET_POSTED(msg, opcode); \
		SBI_STAT_STATUS_MASK(msg, status); \
		SBI_APL_SET_BYTE_ENABLE(msg, be); \
		SBI_APL_SET_BAR(msg, bar); \
		(msg)->function_id = (funct); \
		(msg)->extended_register_address = (eaddr); \
	} while (0)

struct sbi_platform_data {
	const char *name;
	unsigned int version;
	unsigned int bus;
	unsigned int p2sb;
	struct mutex *lock;
};

struct sbi_apl_message {
	u8  port_address;
	u16 register_offset;
	u32 data;
	u8  opcode;
	/* 1-bit */
	u8  posted;
	/* 2-bit */
	u8  status;
	/* 4-bit */
	u8  byte_enable;
	/* 3-bit */
	u8  base_address_register;
	u8  function_id;
	u32 extended_register_address;
};

int sbi_apl_commit(struct sbi_apl_message *args);

#endif
