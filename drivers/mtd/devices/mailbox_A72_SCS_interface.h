#ifndef MAILBOX_A72_SCS_INTERFACE_H
#define MAILBOX_A72_SCS_INTERFACE_H

//#ifdef __aarch64__
    #define A72_MAILBOX                 (0x10080000)
    #define A72_MAILBOX_TO_A72_INT      (A72_MAILBOX + 0x0008)
    #define A72_MAILBOX_TO_SCS_INT      (A72_MAILBOX + 0x001C)
    #define A72_MAILBOX_TO_A72_ISSR_LO  (A72_MAILBOX + 0x0080)
    #define A72_MAILBOX_TO_SCS_ISSR_LO  (A72_MAILBOX + 0x00C0)
    #define A72_MAILBOX_TO_A72_ISSR_HI  (A72_MAILBOX + 0x0100)
    #define A72_MAILBOX_TO_SCS_ISSR_HI  (A72_MAILBOX + 0x0140)
    #define A72_MAILBOX_TO_A72_SEM_LO   (A72_MAILBOX + 0x0180)
    #define A72_MAILBOX_TO_SCS_SEM_LO   (A72_MAILBOX + 0x018C)
    #define A72_MAILBOX_TO_A72_SEM_HI   (A72_MAILBOX + 0x0198)
    #define A72_MAILBOX_TO_SCS_SEM_HI   (A72_MAILBOX + 0x01A4)
//#endif

#include "mailbox_services.h"
#include "mailbox_service_filesystem.h"
//#include "mailbox_service_keys.h"
#include "mailbox_service_otp.h"
#include "mailbox_service_slog.h"
//#include "mailbox_service_elog.h"
#include "mailbox_service_boot.h"
//#include "mailbox_service_sleep.h"
#include "mailbox_service_provisioning.h"

#define A72_MAILBOX_STATUS_OK                                   0x0

#define A72_MAILBOX_STATUS_REQUEST_PENDING                      0x1

#define A72_MAILBOX_STATUS_INVALID_ARGUMENTS                    0xFFFF1000

#define A72_MAILBOX_STATUS_NOT_SUPPORTED                        0xFFFF1001

#define A72_MAILBOX_STATUS_BUSY                                 0xFFFF1002

#define A72_MAILBOX_STATUS_INVALID_COMMAND                      0xFFFFFFFE

#define A72_MAILBOX_STATUS_INVALID_REQUEST                      0xFFFFFFFF

#endif
