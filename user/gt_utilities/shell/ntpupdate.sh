#!/bin/sh
PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin
RC_CONF=/tmp/rc.conf
SCRIPT=$0

NTP_SCRIPT=/usr/shell/ntp.sh
NTPEnable=`nvram get NTPEnable`
NTPSyncInterval=`nvram get NTPSyncInterval`

ntpInterval=0
retry=0
while [ 1 ]
do
	ntpInterval=0;
	if [ "$NTPEnable" = "1" ]; then
		$NTP_SCRIPT start
		if [ "$?" = "0" ]; then
			ntpInterval=$(($NTPSyncInterval*3600));
			retry=0
		else
			if [ $retry -lt 5 ]; then
				ntpInterval=60
				retry=$(($retry+1))
			else
				ntpInterval=3600
			fi
		fi
		echo retry = $retry
	fi

	if [ "$ntpInterval" -le 0 ]; then
		break
	fi

	sleep $ntpInterval
done

