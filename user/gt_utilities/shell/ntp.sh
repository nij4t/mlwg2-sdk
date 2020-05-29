#!/bin/sh
PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin
RC_CONF=/tmp/rc.conf
SCRIPT=$0

NTPDATE_BIN=/usr/sbin/psntpdate
NTPUPDATE_SCRIPT=/usr/shell/ntpupdate.sh
NTP_ARGS="-rf"

NTPEnable=`nvram get NTPEnable`
#NTPDefServer1=`nvram get NTPDefServer1`
NTPDefServer1=time.windows.com
NTPDefServer2=`nvram get NTPDefServer2`
NTPDefServer3=`nvram get NTPDefServer3`

ntpdate() {
	local before after

	if [ -z "$NTPDefServer1" ] &&
		[ -z "$NTPDefServer2" ] &&
		[ -z "$NTPDefServer3" ]; then
		NTPDefServer1=time.nrc.ca
	fi
	
	before=`date +"%s"`
	if { [ -n "$NTPDefServer1" ] && $NTPDATE_BIN $NTP_ARGS $NTPDefServer1; } ||
		{ [ -n "$NTPDefServer2" ] && $NTPDATE_BIN $NTP_ARGS $NTPDefServer2; } ||
		{ [ -n "$NTPDefServer3" ] && $NTPDATE_BIN $NTP_ARGS $NTPDefServer3; }; then
		after=`date +"%s"`
		# store correct time for next boot
		date +"%m%d%H%M%Y.%S" >/tmp/ntp.date
		# adjust timestamp of webctrl session
		#/opt/httpd/cgi-bin/webctrl.cgi --shiftsess `expr $after - $before`
		return 0
	else
		#return 1
		sleep 10;
		ntpdate;
	fi
}

config() {
	if [ "$NTPEnable" = "1" ] ; then 
		ntpdate
	else
		return 0
	fi

}

usage () {
	echo "$0 [config|start]"
	exit 1
}

[ -z "$1" ] && usage;
result=1
for action in $*; do
	case $action in 
		config|start)	config;result=$?;;
		force)		ntpdate;result=$?;;
		update)
				if ps|grep $NTPUPDATE_SCRIPT|grep -v grep; then
					echo $NTPUPDATE_SCRIPT is already start
					result=1
				else
					$NTPUPDATE_SCRIPT &
					result=0
				fi;;
		*)		usage;;
	esac

	echo $SCRIPT $action ok
done

exit $result

