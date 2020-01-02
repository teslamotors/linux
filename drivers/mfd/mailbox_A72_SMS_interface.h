#ifndef MAILBOX_A72_SMS_INTERFACE_H
#define MAILBOX_A72_SMS_INTERFACE_H

#include "../mtd/devices/mailbox_services.h"
#include "mailbox_service_pmic.h"
#include "mailbox_service_comm.h"
#include "mailbox_service_tmu.h"
#include "mailbox_service_rtc.h"
//#include "mailbox_service_recovery.h"
#include "../mtd/devices/mailbox_service_boot.h"
//#include "../mtd/devices/mailbox_service_sleep.h"
#include "mailbox_service_ap.h"

#define SMS_MAILBOX_STATUS_OK                                   0x0

#define SMS_MAILBOX_STATUS_REQUEST_PENDING                      0x1

#define SMS_MAILBOX_STATUS_INVALID_ARGUMENTS                    0xFFFF1000

#define SMS_MAILBOX_STATUS_NOT_SUPPORTED                        0xFFFF1001

#define SMS_MAILBOX_STATUS_BUSY                                 0xFFFF1002

#define SMS_MAILBOX_STATUS_INVALID_COMMAND                      0xFFFFFFFE

#define SMS_MAILBOX_STATUS_INVALID_REQUEST                      0xFFFFFFFF

#endif
