#! /bin/sh
# $Id: //WIFI_SOC/release/SDK_4_1_0_0/source/user/miniupnpd-1.1/netfilter/iptables_flush.sh#1 $
IPTABLES=iptables

#flush all rules owned by miniupnpd
$IPTABLES -t nat -F MINIUPNPD
$IPTABLES -t filter -F MINIUPNPD

