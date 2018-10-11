#!/bin/bash

# Network interface name (e.g. Fedora=p2p1, Ubuntu=eth2, OpenSUSE=ens01)
INTERFACE=p2p1

# Replace default interface if provided by argument
if [ -n "$1" ]
  then
    INTERFACE=$1
fi

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
