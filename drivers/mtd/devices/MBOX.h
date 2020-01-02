#ifndef _MBOX_H_
#define _MBOX_H_

//#include <stdint.h>
//#include <stdbool.h>

struct mbox_int {
  uint32_t intgr;
  uint32_t intcr;
  uint32_t intmr;
  uint32_t intsr;
  uint32_t intmsr;
};

#define MAILBOX_REGS_PER_BLOCK 16
#define MAX_REQUEST_ARGUMENTS (MAILBOX_REGS_PER_BLOCK - 3)

struct mbox_issr {
    uint32_t   issr[MAILBOX_REGS_PER_BLOCK];
};

struct mbox_semaphore {
    uint32_t   semaphore[3];
};

struct mbox {
    uint32_t mcu_ctrl;

    uint32_t rsvd;
    struct mbox_int int0;
    struct mbox_int int1;
    uint32_t rsvd2[(0x50-0x30)/4];
    uint32_t version;
    uint32_t rsvd3[(0x80-0x54)/4];

    struct mbox_issr         issr_lo0;
    struct mbox_issr         issr_lo1;
    struct mbox_issr         issr_hi0;
    struct mbox_issr         issr_hi1;

    struct mbox_semaphore    _semaphores_lo0;
    struct mbox_semaphore    _semaphores_lo1;
    struct mbox_semaphore    _semaphores_hi0;
    struct mbox_semaphore    _semaphores_hi1;
};

#define ISSR_INDEX_DATA0   0
#define ISSR_INDEX_DATA1   1
#define ISSR_INDEX_DATA2   2
#define ISSR_INDEX_DATA3   3
#define ISSR_INDEX_DATA4   4
#define ISSR_INDEX_DATA5   5
#define ISSR_INDEX_DATA6   6
#define ISSR_INDEX_DATA7   7
#define ISSR_INDEX_DATA8   8
#define ISSR_INDEX_DATA9   9
#define ISSR_INDEX_DATA10  10
#define ISSR_INDEX_DATA11  11
#define ISSR_INDEX_DATA12  12
#define ISSR_INDEX_REQUEST 13
#define ISSR_INDEX_STATUS  14
#define ISSR_INDEX_SERVICE 15

#ifdef MBOX_COMMAND_RESPONSE_ID
#undef MBOX_COMMAND_RESPONSE_ID
#endif
#define MBOX_COMMAND_RESPONSE_ID 0x80000000

#endif
