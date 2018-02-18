#ifndef _LINUX_RTLTOOL_H
#define _LINUX_RTLTOOL_H

#define SIOCRTLTOOL		SIOCDEVPRIVATE+1

enum rtl_cmd {
        RTLTOOL_READ_MAC=0,
        RTLTOOL_WRITE_MAC,
        RTLTOOL_READ_PHY,
        RTLTOOL_WRITE_PHY,
        RTLTOOL_READ_EPHY,
        RTLTOOL_WRITE_EPHY,
        RTLTOOL_READ_ERI,
        RTLTOOL_WRITE_ERI,
        RTLTOOL_READ_PCI,
        RTLTOOL_WRITE_PCI,
        RTLTOOL_READ_EEPROM,
        RTLTOOL_WRITE_EEPROM,

        RTL_READ_OOB_MAC,
        RTL_WRITE_OOB_MAC,

        RTL_ENABLE_PCI_DIAG,
        RTL_DISABLE_PCI_DIAG,

        RTL_READ_MAC_OCP,
        RTL_WRITE_MAC_OCP,

        RTLTOOL_INVALID
};

struct rtltool_cmd {
        __u32	cmd;
        __u32	offset;
        __u32	len;
        __u32	data;
};

enum mode_access {
        MODE_NONE=0,
        MODE_READ,
        MODE_WRITE
};

#ifdef __KERNEL__
int rtltool_ioctl(struct rtl8168_private *tp, struct ifreq *ifr);
#endif

#endif /* _LINUX_RTLTOOL_H */
