#!/bin/sh
PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin
SCRIPT=$0

UDHCPD_BIN=udhcpd
UDHCPD_PID_FILE=/tmp/udhcpd.pid
UDHCPD_LEASE_FILE=/tmp/udhcpd.leases
UDHCPD_CONF_FILE=/tmp/udhcpd.conf
UDHCPD_INTERFACE=br0
RESOLV_CONF=/etc/resolv.conf

# Get the necessary enviro's
#. /ramdisk/flash/rc.conf
dns_server1=`nvram get DHCPSDNSServer1`
dns_server2=`nvram get DHCPSDNSServer2`
dns_server3=`nvram get DHCPSDNSServer3`
wan_dns1=`nvram get WANDNSServer1`
wan_dns2=`nvram get WANDNSServer1`
wan_dns3=`nvram get WANDNSServer1`
wan_domainname=`nvram get WanDomainName`
dhcp_dns_relay_enable=`nvram get DHCPSDNSRelayEnable`
#lan_ip=`nvram get LanIPAddr`
#Jayo 08/11/28: set lan IP to Default Gateway
DHCPSEnable=`nvram get DHCPSEnable`
lan_ip=`nvram get LanIPAddr`
LanNetmask=`nvram get LanNetmask`
DHCPSIPStart=`nvram get DHCPSIPStart`
DHCPSIPEnd=`nvram get DHCPSIPEnd`
DHCPSLeaseTime=`nvram get DHCPSLeaseTime`
DHCPSDefaultGW=`nvram get DHCPSDefaultGW`
DHCPSDomainName=`nvram get DHCPSDomainName`
DHCPSMaxUser=`nvram get DHCPSMaxUser`
DHCPSWINSServer=`nvram get DHCPSWINSServer`
DHCPSStaticLease1=`nvram get DHCPSStaticLease1`
DHCPSStaticLease2=`nvram get DHCPSStaticLease2`
DHCPSStaticLease3=`nvram get DHCPSStaticLease3`
DHCPSStaticLease4=`nvram get DHCPSStaticLease4`
DHCPSStaticLease5=`nvram get DHCPSStaticLease5`
DHCPSStaticLease6=`nvram get DHCPSStaticLease6`
DHCPSStaticLease7=`nvram get DHCPSStaticLease7`
DHCPSStaticLease8=`nvram get DHCPSStaticLease8`
DHCPSStaticLease9=`nvram get DHCPSStaticLease9`
DHCPSStaticLease10=`nvram get DHCPSStaticLease10`
#Jayo 2009/08/27 : for DHCP Server advance settings
DHCPSDefaultGWStatus=`nvram get DHCPSDefaultGWStatus`
DHCPSDomainNameStatus=`nvram get DHCPSDomainNameStatus`
DHCPSWINSServerStatus=`nvram get DHCPSWINSServerStatus`
WanCurrentWINSServer=`nvram get WanCurrentWINSServer`
DHCPSDNSServerStatus=`nvram get DHCPSDNSServerStatus`
DHCPSExcludedIPAll=`nvram get DHCPSExcludedIPAll`
WanCurrentDomainName=`nvram get WanCurrentDomainName`
# routines ##########################################################

config () {
	local leasetime; leasetime="$1"; shift
	local dnsstr domainstr
	local line _mac _ip

	# resolv.conf settings may come from staticip/dhcp/pppoe
	if [ -f $RESOLV_CONF ]; then
		for s in `grep '^nameserver' $RESOLV_CONF |sed -e 's/nameserver//'`; do
			if [ "$dnsstr" = "" ]; then
				dnsstr="$s"
			else
				dnsstr="$dnsstr $s"
			fi
		done
		for s in `grep '^search' $RESOLV_CONF |sed -e 's/search//'` \
			 `grep '^domain' $RESOLV_CONF |sed -e 's/domain//'`; do
			domainstr="$s"
		done
	fi
	
	# Cash;
	dnsstr="$dns_server1 $dns_server2 $dns_server3"
	
	# if null str, use default from meta file
	[ "$dnsstr" = "" ] && dnsstr="$wan_dns1 $wan_dns2"
	[ "$domainstr" = "" ] && domainstr="$wan_domainname"
	[ "$domainstr" = "" ] && domainstr="mynetwork"

	if [ "$dnsstr" = "" ] | [ "$dnsstr" = "  " ] ; then
		dnsstr=$lan_ip
	fi

#	nvram set wan_domainname="$domainstr"
#	nvram commit


	#Jayo 2010/02/09: for setting DHCP lease ip range correctly
	gt_utils SetDHCPSRange 
	DHCPSIPEnd=`nvram get DHCPSIPEnd`

#Jayo 08/11/28 : set option router to default gateway & add WINS server option
	echo "# dhcpd configuration file
option subnet	$LanNetmask
start		$DHCPSIPStart
end		$DHCPSIPEnd
option lease	$DHCPSLeaseTime
max_leases	$DHCPSMaxUser
remaining	no
interface	$UDHCPD_INTERFACE
lease_file	$UDHCPD_LEASE_FILE" > $UDHCPD_CONF_FILE

	if [ "$DHCPSDNSServerStatus" == "0" ]; then
		echo "option dns	$lan_ip" >> $UDHCPD_CONF_FILE
	elif [ "$DHCPSDNSServerStatus" == "1"  ]; then
		echo "option dns	$dnsstr" >> $UDHCPD_CONF_FILE
	fi

	if [ "$DHCPSDefaultGWStatus" == "0" ]; then
		echo "option router	$lan_ip" >> $UDHCPD_CONF_FILE
	elif [ "$DHCPSDefaultGWStatus" == "1"  ]; then
		echo "option router	$DHCPSDefaultGW" >> $UDHCPD_CONF_FILE
	fi

	if [ "$WanCurrentWINSServer" != "" ] && [ "$DHCPSWINSServerStatus" == "0" ]; then
		echo "option wins	$WanCurrentWINSServer" >> $UDHCPD_CONF_FILE
	elif [ "$DHCPSWINSServer" != "" ] && [ "$DHCPSWINSServerStatus" == "1" ]; then
		echo "option wins	$DHCPSWINSServer" >> $UDHCPD_CONF_FILE
	fi

	if [ "$WanCurrentDomainName" != "" ] && [ "$DHCPSDomainNameStatus" == "0" ]; then
		echo "option domain	$WanCurrentDomainName" >> $UDHCPD_CONF_FILE
	elif [ "$DHCPSDomainName" != "" ] && [ "$DHCPSDomainNameStatus" == "1" ]; then
		echo "option domain	$DHCPSDomainName" >> $UDHCPD_CONF_FILE
	fi

# Jayo 08/11/26 : Set Not Lease IP 
 	i=1
 	not_lease_ip=`echo $DHCPSExcludedIPAll | cut -d ',' -f $i`
	format_check=`echo $DHCPSExcludedIPAll | grep ','` 
 	while [ "$not_lease_ip" != ""  ]  
 	do
 		echo "not_lease_ip	$not_lease_ip" >> $UDHCPD_CONF_FILE
		if [ "$format_check" == "" ]; then
			break
		fi
 		i=$(($i+1))
 		not_lease_ip=`echo $DHCPSExcludedIPAll | cut -d ',' -f $i`
 	done
##

# Jayo : add for reserved IP
#NetworkIP=`echo $lan_ip | sed -e 's/\.[0-9]*$/\./' `
##

# Static Lease 
#	for line in \
#		"$DHCPSStaticLease1"  "$DHCPSStaticLease2"  "$DHCPSStaticLease3"  "$DHCPSStaticLease4"  "$DHCPSStaticLease5"  \
#		"$DHCPSStaticLease6"  "$DHCPSStaticLease7"  "$DHCPSStaticLease8"  "$DHCPSStaticLease9"  "$DHCPSStaticLease10" ; do

# Jayo 2009/08/28 : for non-limited static lease number
	for line in `nvram show | grep DHCPSStaticLease | sed 's/^[a-z,A-Z,0-9]*=//g' `  ; do

#		"$dhcp_st11" "$dhcp_st12" "$dhcp_st13" "$dhcp_st14" "$dhcp_st15" \
#		"$dhcp_st16" "$dhcp_st17" "$dhcp_st18" "$dhcp_st19" "$dhcp_st20" \
#		"$dhcp_st21" "$dhcp_st22" "$dhcp_st23" "$dhcp_st24" "$dhcp_st25" \
#		"$dhcp_st26" "$dhcp_st27" "$dhcp_st28" "$dhcp_st29" "$dhcp_st30" \
#		"$dhcp_st31" "$dhcp_st32"
		[ -z "$line" ] && continue
		# tricky! get _en _mac _ip
		eval $line
		if [ "$_en" = "1" ]; then
#			echo "static_lease	$_mac	$_ip" >> $UDHCPD_CONF_FILE

			# Jayo : add for reserved IP
			#echo "static_lease	$_mac	$NetworkIP$_ip" >> $UDHCPD_CONF_FILE
			echo "static_lease	$_mac	$_ip" >> $UDHCPD_CONF_FILE
			##
		fi
	done
##
	
# Not Assign IP
#	for line in \
#		"$DHCPSNotAssignIP1"  "$DHCPSNotAssignIP2"  "$DHCPSNotAssignIP3"  "$DHCPSNotAssignIP4"  "$DHCPSNotAssignIP5"  \
#		"$DHCPSNotAssignIP6"  "$DHCPSNotAssignIP7"  "$DHCPSNotAssignIP8"  "$DHCPSNotAssignIP9"  "$DHCPSNotAssignIP10" ; do
#		[ -z "$line" ] && continue
		# tricky! get _mac _ip
#		eval $line
#		echo "no_ip	$_mac " >> $UDHCPD_CONF_FILE
#	done
##
}

start () {
	#rm -rf $UDHCPD_LEASE_FILE
	#rm -rf /tmp/dhcp_table
	rm -rf /tmp/dhcpsTable
	if [ "$DHCPSEnable" = "1" ]; then
		($UDHCPD_BIN $UDHCPD_CONF_FILE &)
	fi
	#/usr/bin/killall -USR1 httpd    #Set USR1 signal to notify httpd to close socket after re-configure LAN interface
}

stop () {
	local err; err=0
	local pid
	if [ -f $UDHCPD_PID_FILE ]; then
		kill -TERM `cat $UDHCPD_PID_FILE` || err=1
#		/usr/bin/killall -TERM udhcpd
		[ -f $UDHCPD_PID_FILE ] && (rm $UDHCPD_PID_FILE || err=1)
	else
		pid=`pidof udhcpd`
		[ ! -z "$pid" ] && ( kill -TERM $pid || err=1)
	fi
	return $err
}

usage () {
        echo "$0 [start|stop|restart|reload|config|config_boot]"
        exit 1
}

# main ##########################################################

[ -z "$1" ] && usage;

for action in $*; do
	case $action in 
		config_boot)	config 60;;
		config)		config $DHCPSLeaseTime;;
		start)		start;;
		stop)		stop;;
		reload)		;;
		restart)	stop; sleep 1; config $DHCPSLeaseTime; start;;
		*)		usage;;
	esac
	if [ $? != "0" ] ; then
		echo $SCRIPT $action error
		exit 1
	fi

	echo $SCRIPT $action ok
done

exit 0
