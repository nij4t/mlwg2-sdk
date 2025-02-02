#! /bin/sh
# $Id: //WIFI_SOC/release/SDK_4_1_0_0/source/user/miniupnpd-1.6/netfilter/iptables_init_and_clean.sh#1 $
# Improved Miniupnpd iptables init script.
# Checks for state of filter before doing anything..

EXTIF=eth0
IPTABLES=/sbin/iptables
EXTIP="`LC_ALL=C /sbin/ifconfig $EXTIF | grep 'inet ' | awk '{print $2}' | sed -e 's/.*://'`"
NDIRTY="`LC_ALL=C /sbin/iptables -t nat -L -n | grep 'MINIUPNPD' | awk '{printf $1}'`"
FDIRTY="`LC_ALL=C /sbin/iptables -t filter -L -n | grep 'MINIUPNPD' | awk '{printf $1}'`"
echo "External IP = $EXTIP"

if [[ $NDIRTY = "MINIUPNPDChain" ]]; then
        echo "Nat table dirty; Cleaning..."
        $IPTABLES -t nat -F MINIUPNPD
elif [[ $NDIRTY = "Chain" ]]; then
        echo "Dirty NAT chain but no reference..? Fixsted."
        $IPTABLES -t nat -A PREROUTING -d $EXTIP -i $EXTIF -j MINIUPNPD
        $IPTABLES -t nat -F MINIUPNPD
else
        echo "NAT table clean..initalizing.."
        $IPTABLES -t nat -N MINIUPNPD
        $IPTABLES -t nat -A PREROUTING -d $EXTIP -i $EXTIF -j MINIUPNPD
fi
if [[ $FDIRTY = "MINIUPNPDChain" ]]; then
        echo "Filter table dirty; Cleaning..."
        $IPTABLES -t filter -F MINIUPNPD
elif [[ $FDIRTY = "Chain" ]]; then
        echo "Dirty filter chain but no reference..? Fixsted."
        $IPTABLES -t filter -I FORWARD 4 -i $EXTIF ! -o $EXTIF -j MINIUPNPD
        $IPTABLES -t filter -F MINIUPNPD
else
        echo "Filter table clean..initalizing.."
        $IPTABLES -t filter -N MINIUPNPD
        $IPTABLES -t filter -I FORWARD 4 -i $EXTIF ! -o $EXTIF -j MINIUPNPD
fi

