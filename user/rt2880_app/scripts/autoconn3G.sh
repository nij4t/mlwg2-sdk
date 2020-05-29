#! /bin/sh

LOCK_FILE=/var/lock/LOCK.3G.auto
DEV_FILE=/tmp/usb_dev
PID_FILE=/var/lock/autoconnect3G_pid
SUPPORT_3G="12D1:1001:HUAWEI-E169
			12D1:1003:HUAWEI-E220
			07D1:A800:DLink-DWM156
	    0408:EA02:MU-Q101
	    0408:1000:MU-Q101
	    0AF0:6971:OPTION-ICON225
	    1AB7:5700:DATANG-M5731
	    1AB7:5731:DATANG-M5731		
	    FEED:5678:MobilePeak-Titan
	    FEED:0001:MobilePeak-Titan
	    1A8D:1000:BandLuxe-C270
	    1A8D:1009:BandLuxe-C270"		

killall check_3g_out.sh

if [ -f "$LOCK_FILE" ]; then
	exit 0
else
	if [ ! -f "/var/lock" ]; then #move to rcS create
		mkdir -p /var/lock/
	fi
	touch "$LOCK_FILE"
fi

if [ -f "$PID_FILE" ]; then
	exit 0
else
	echo $! > $PID_FILE
	sleep 1
	if [ "`cat $PID_FILE`" != "$!" ] ; then
		exit 0
	fi
	rm -f $PID_FILE
fi

if [ "$1" = "connect" ]; then
	enabled=`nvram get 3g_enabled`
	enabled="1"

	if [ "$enabled" != "1" ]; then
		echo "3G not enabled"
		rm -f $DEV_FILE
		rm -f $LOCK_FILE
		exit 0
	fi

	DEV="Auto";
	cat /proc/bus/usb/devices | sed -n '/.* Vendor=.* ProdID=.*/p' | sed -e 's/.*Vendor=\(.*\) ProdID=\(.*\) Rev.*/\1:\2/' | sed 'y/abcdefghijklmnopqrstuvwxyz/ABCDEFGHIJKLMNOPQRSTUVWXYZ/' > $DEV_FILE

	for i in `cat "$DEV_FILE"`
	do
		for j in $SUPPORT_3G
		do
		k=`echo $j | sed -e 's/\(.*\):\(.*\):.*/\1:\2/'`
		if [ $i = $k  ]; then
			k=`echo $j | sed -e 's/.*:.*:\(.*\)$/\1/'`
			if [ $DEV = "Auto" ] || [ $DEV = "" ] || [ $k = $DEV ] ; then
				`nvram set 3g_wan=1`
				`nvram set WanCurrentIP=""`
				if [ "`nvram get 3g_apn`" == "" ] ; then
					echo "3G APN is NULL, try to get APN"
					comgt -d /dev/ttyUSB0 info | grep "APN" > /tmp/comgt_APN
					if [ "`cat /tmp/comgt_APN | grep ERROR | wc -l`" == "0" ] ; then
						nvram set 3g_apn=`cat /tmp/comgt_APN | cut -d '"' -f 4`
						nvram set 3g_dial=*99***`cat /tmp/comgt_APN | cut -d ',' -f 1 | tail -c 2`#
					fi
					rm -f /tmp/comgt_APN
				fi
				3g.sh $k    
				rm -f $DEV_FILE
				rm -f $LOCK_FILE
				check_3g_out.sh &
				exit 1
			fi
		fi
	    done
	done
elif [ "$1" = "disconnect" ]; then
	killall -q pppd
	hso_connect.sh down
	rm -f $DEV_FILE
	rm -f $LOCK_FILE
	`nvram set 3g_wan=0`
	if [ "`nvram get ethernet_wan`" == "1" ] ; then
		link_detect ethertnet
	else
		link_detect wifi
	fi
	exit 1
fi

rm -f $DEV_FILE
rm -f $LOCK_FILE
exit 0

