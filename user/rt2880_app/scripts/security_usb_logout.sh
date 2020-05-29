#! /bin/sh

if [ "$1" == "time" ] ; then
	line_num="`cat /tmp/security_usb_login_time | grep -n "$2 " | sed 1q | cut -d ':' -f 1`";
	sed -i "$line_num d" /tmp/security_usb_login_time
elif [ "$1" == "file" ] ; then
	line_num="`cat /tmp/security_usb_login_time | grep -n "$2 " | sed 1q | cut -d ':' -f 1`";
	sed -i "$line_num d" /tmp/security_usb_login_time
	line_num="`cat /tmp/security_usb_login_list | grep -n "$2 " | sed 1q | cut -d ':' -f 1`";
	sed -i "$line_num d" /tmp/security_usb_login_list
elif [ "$1" == "all" ] ; then
		count=0;
		umount_usb="`mount | grep -n USB | cut -d ' ' -f 1 | cut -d '/' -f 3`";
		while [ "$umount_usb" != "" ]
		do
			/sbin/automount.sh $umount_usb
			rm -f /dev/$umount_usb
			count=`expr $count + 1`
			if [ $count -gt 3 ] ; then
				umount_usb="";
			else
				umount_usb="`mount | grep -n USB | cut -d ' ' -f 1 | cut -d '/' -f 3`";
			fi
		done
fi
exit 0
