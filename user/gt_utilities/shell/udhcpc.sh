#!/bin/sh
PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin
SCRIPT=$0

UDHCPC_BIN=/sbin/udhcpc
UDHCPC_PID_FILE=/tmp/udhcpc.pid
UDHCPC_LEASE_FILE=/tmp/udhcpc.lease
#UDHCPC_SCRIPT_FILE=/usr/shell/udhcpc.script

# Get the enviro's
wan_hostname=`nvram get DeviceName`
RouterMode=`nvram get RouterMode`
if [ "$1" = "lan_dhcpc_start" ]; then
	UDHCPC_INTERFACE=br0
	UDHCPC_SCRIPT_FILE=/usr/shell/udhcpc.lan_check.script
elif [ "$RouterMode" = "0" ] || [ "$1" = "wds_bridge_start" ]; then
	UDHCPC_INTERFACE=br0
	UDHCPC_SCRIPT_FILE=/usr/shell/udhcpc.br0.script
elif [ "$1" = "ethernet_start" ]; then
	UDHCPC_INTERFACE=vlan0002
	UDHCPC_SCRIPT_FILE=/usr/shell/udhcpc.script
else
#	UDHCPC_INTERFACE=vlan0002
	UDHCPC_INTERFACE=apcli0
       	UDHCPC_SCRIPT_FILE=/usr/shell/udhcpc.script
fi


# routines ##########################################################

start () {
	if [ "$wan_hostname" = "" ]; then
		$UDHCPC_BIN -p $UDHCPC_PID_FILE -s $UDHCPC_SCRIPT_FILE -i $UDHCPC_INTERFACE -b
	else
		$UDHCPC_BIN -h "$wan_hostname" -p $UDHCPC_PID_FILE -s $UDHCPC_SCRIPT_FILE -i $UDHCPC_INTERFACE -b
	fi
}

lan_stop () {
	if [ -f $UDHCPC_PID_FILE ]; then
		kill -TERM `cat $UDHCPC_PID_FILE` || err=1
		[ -f $UDHCPC_PID_FILE ] && (rm $UDHCPC_PID_FILE || err=1)
	else
		pid=`pidof udhcpc`
		[ ! -z "$pid" ] && ( kill -TERM $pid || err=1)
	fi
	#Clear status info in nvram
        nvram set WanCurrentIP=""
        nvram set WanCurrentNetmask=""
      	nvram set WanCurrentGateway=""
     	nvram set WanCurrentDNS1=""
      	nvram set WanCurrentDNS2=""
     	nvram set WanCurrentDNS3=""
     	nvram set WanCurrentMTU=""
     	nvram set WanCurrentHostname=""
     	nvram set WanCurrentDomainName=""
     	nvram set WanCurrentWINSServer=""
     	nvram set WanCurrentDHCPLeaseStart=""
     	nvram set WanCurrentDHCPLeaseEnd=""
       	nvram set WanCurrentDHCPServ=""
}

stop () {
	local err; err=0
	local pid
	if [ -f $UDHCPC_PID_FILE ]; then
		kill -TERM `cat $UDHCPC_PID_FILE` || err=1
		[ -f $UDHCPC_PID_FILE ] && (rm $UDHCPC_PID_FILE || err=1)
	else
		pid=`pidof udhcpc`
		[ ! -z "$pid" ] && ( kill -TERM $pid || err=1)
	fi
	[ -f $UDHCPC_LEASE_FILE ] && (rm -f $UDHCPC_LEASE_FILE || err=1)
	ifconfig $UDHCPC_INTERFACE 0.0.0.0 || err=1
	#Clear status info in nvram
        nvram set WanCurrentIP=""
        nvram set WanCurrentNetmask=""
      	nvram set WanCurrentGateway=""
     	nvram set WanCurrentDNS1=""
      	nvram set WanCurrentDNS2=""
     	nvram set WanCurrentDNS3=""
     	nvram set WanCurrentMTU=""
     	nvram set WanCurrentHostname=""
     	nvram set WanCurrentDomainName=""
     	nvram set WanCurrentWINSServer=""
     	nvram set WanCurrentDHCPLeaseStart=""
     	nvram set WanCurrentDHCPLeaseEnd=""
       	nvram set WanCurrentDHCPServ=""
       	#echo "" > $RESOLV_CONF
	# Jayo 2010/01/12 : for checking Wan/Lan network segment
	#if [ -f "/tmp/WanLan" ] ; then
	#	unlink "/tmp/WanLan"
	#	led 7 off
	#fi
     	route del default 
        #/usr/sbin/gt_utils wan_service stop &
	return $err
}

release () {
	kill -USR2 `pidof udhcpc`
}

renew ()
{
	kill -USR1 `pidof udhcpc`
}

usage () {
        echo "$0 [start|stop|restart|reload|config]"
        exit 1
}

# main ##########################################################

[ -z "$1" ] && usage;

for action in $*; do
	case $action in 
		lan_dhcpc_start)	start;;
		lan_dhcpc_stop)		lan_stop;;
		wds_bridge_start)	start;;
		start)		start;;
		stop)		stop;;
		release)	release;;
		renew)		renew;;
		reload)		;;
		restart)	stop; sleep 1; start;;
		ethernet_start)		start;;
		*)		usage;;
	esac
	if [ $? != "0" ] ; then
		echo $SCRIPT $action error
		exit 1
	fi

	echo $SCRIPT $action ok
#	if [ $action == "start" ] ; then
#		led 43 on
	if [ $action == "stop" ] ; then
		led 43 off
		killall widrive_aloha_wan
		killall ntp.sh
	fi
done

exit 0
