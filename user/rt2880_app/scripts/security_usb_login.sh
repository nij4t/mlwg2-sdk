#! /bin/sh

check_CDROMSize="`Tester_run 1 | grep "CDROMSize=0" | wc -l`";
check_IsLogin="`Tester_run  1 | grep "IsLogin Private= " | cut -d ' ' -f 7`";
check_IsLogin2="`Tester_run  1 | grep "IsLogin Private= " | wc -l`";
check_NoPWS="`Tester_run  1 | grep "IsExisted PWS Private=0" | wc -l`";
if [ "$1" == "check" ] ; then
	if [ "$check_CDROMSize" == "1" ] ; then
		nvram set current_security_usb_in=0;
		exit 0
	fi
	if [ "$check_NoPWS" == "1" ] ; then
		nvram set current_security_usb_in=2;
		exit 0
	fi
	if [ "$check_IsLogin2" == "0" ] ; then
		nvram set current_security_usb_in=0;
		exit 0
	fi
	nvram set current_security_usb_in=1;
	exit 0;
fi
if [ "$1" == "time" ] ; then
	if [ "$check_CDROMSize" == "1" ] ; then
		exit 0
	fi
	if [ "$check_IsLogin" == "0" ] ; then
		exit 0
	fi
	client_mac="`cat /proc/net/arp | grep "$2 " | cut -c 42-58`";
	if [ "$client_mac" == "" ] ; then
		exit 0;
	fi
	check_login_list="`cat /tmp/security_usb_login_list | grep "$2 " | wc -l`";
	if [ "$check_login_list" != "1" ] ; then
		exit 0;
	fi
	time_login_list="`cat /tmp/security_usb_login_time | grep "$2 $client_mac" | wc -l`";
	if [ "$time_login_list" != "1" ] ; then
		echo "$2 $client_mac" >> /tmp/security_usb_login_time
	fi
	exit 0;
fi
#echo login_ip=$1;
if [ "$check_CDROMSize" == "1" ] ; then
	exit 0
fi
if [ "$check_IsLogin" == "0" ] ; then
	security_usb_password="`nvram get security_usb_password`";
	Tester_run 3 $security_usb_password > /tmp/security_usb_login_status
	security_usb_login_status="`cat /tmp/security_usb_login_status | grep "UDV_Login:(00)return OK." | wc -l`";
	if [ "$security_usb_login_status" != "1" ] ; then
		rm -f /tmp/security_usb_login_status
		exit 0
	fi
	rm -f /tmp/security_usb_login_list
	rm -f /tmp/security_usb_login_time
	killall security_usb_loop
	rm -f /tmp/security_usb_login_status
	security_usb.sh
	nvram set current_security_usb_password=$security_usb_password
	arping -I br0 -w 3 -f $1
	client_mac="`cat /proc/net/arp | grep "$1 " | cut -c 42-58`";
	if [ "$client_mac" == "" ] ; then
		exit 0;
	fi
	echo "$1 $client_mac" >> /tmp/security_usb_login_time
	echo "$1 $client_mac" >> /tmp/security_usb_login_list
	security_usb_loop $1 $client_mac &
	sleep 3
	while [ "`mount | grep "USB" | wc -l`" == "0" ]
	do
		echo "Security USB not UP, do again!!"
		Tester_run 4
		Tester_run 3 $security_usb_password
		security_usb.sh
		sleep 3
	done
	exit 0
fi
if [ "`nvram get security_usb_password`" == "`nvram get current_security_usb_password`" ] ; then
	arping -I br0 -w 3 -f $1
	client_mac="`cat /proc/net/arp | grep "$1 " | cut -c 42-58`";
	if [ "$client_mac" == "" ] ; then
		exit 0;
	fi
	time_login_list="`cat /tmp/security_usb_login_time | grep "$1 " | wc -l`";
	if [ "$time_login_list" != "1" ] ; then
		echo "$1 $client_mac" >> /tmp/security_usb_login_time
	fi
	check_login_list="`cat /tmp/security_usb_login_list | grep "$1 " | wc -l`";
	if [ "$check_login_list" == "1" ] ; then
		exit 0;
	fi
	echo "$1 $client_mac" >> /tmp/security_usb_login_list
	security_usb_loop $1 $client_mac &
fi
exit 0
