INTRODUCTION

This component demonstrates various features of the Intel I210 Ethernet
controller. These features can be used for developing Audio/Video Bridging
applications, Industrial Ethernet applications which require precise timing
control over frame transmission, or test harnesses for measuring system
latencies and sampling events.

This component - igb_avb - is limited to the Intel I210 Ethernet controller.
The kernel module can be loaded in parallel to existing in-kernel igb modules
which may be used on other supported Intel LAN controllers. Modifications are
required to the in-kernel drivers if the existing in-kernel igb driver has
support for the Intel I210.

BUILDING

The kernel igb module should be built which supports the latest Linux kernel
3.x PTP clock support. Unlike the standard igb driver, this version enables
PTP by default (and will fail to build without kernel PTP support enabled).

RUNNING

To install the kernel mode driver, you must have root permissions. Typically,
the driver is loaded by removing the currently running igb and running igb_avb:
	sudo rmmod igb
	<optional> sudo modprobe i2c_algo_bit
	<optional> sudo modprobe dca
	<optional> sudo modprobe ptp
	sudo insmod ./igb_avb.ko

Another option is to install the igb_avb driver in the "updates" directory
which will override igb for the other drivers claiming the same device ID. This
will allow the coexistence of igb and igb_avb. Copy igb_avb.ko to:
	sudo cp igb_avb.ko /lib/modules/`uname -r`/updates/
	sudo depmod -a
	modprobe igb_avb

As the AVB Transmit queues (0,1) are mapped to a user-space application,
typical LAN traffic must be steered away from these queues. The driver
implements one method registering an ndo_select_queue handler to map traffic to
queue[3].  Another possibly faster method uses the the transmit packet steering
(XPS) functionality available since 2.6.35. An example script is below

#!/bin/bash

INTERFACE=p2p1
export INTERFACE

rmmod igb
rmmod igb_avb
insmod ./igb_avb.ko
sleep 1
ifconfig $INTERFACE down
echo 0 > /sys/class/net/$INTERFACE/queues/tx-0/xps_cpus
echo 0 > /sys/class/net/$INTERFACE/queues/tx-1/xps_cpus
echo f > /sys/class/net/$INTERFACE/queues/tx-2/xps_cpus
echo f > /sys/class/net/$INTERFACE/queues/tx-3/xps_cpus
ifconfig $INTERFACE up

You map also want to disable the network manager from 'managing' your
interface.  The easiest way is to find the interface configuration scripts on
your distribution.  On Fedora 18, these are located at
/etc/sysconfig/network-scripts/ifcfg-<interface>.  Edit the file to set
'BOOTPROTO=none'. This eliminates DHCP trying to configure the interface while
you may be doing user-space application configuration.
