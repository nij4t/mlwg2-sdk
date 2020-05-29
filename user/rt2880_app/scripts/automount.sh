#! /bin/sh
if [ "$1" == "" ]; then
echo "parameter is none" 
exit 1
else
	echo "***** $1 *****"
fi
mounted=`mount | grep $1 | wc -l`
devnum=`echo $1 | tail -c 2`
dev=`echo $1 | head -c 3`
media_name=""

scsidev=`ls /sys/block/$dev/device/scsi_disk`
scsinum1=`echo $scsidev | cut -d ':' -f 1`
#scsinum2=`echo $scsidev | cut -d ':' -f 2`
#scsinum3=`echo $scsidev | cut -d ':' -f 3`
#scsinum4=`echo $scsidev | cut -d ':' -f 4`
#scsipath="/sys/block/$dev/device/scsi_disk/$scsidev/subsystem/$scsinum1:$scsinum2:$scsinum3:1"
deviceport0=`cat /proc/bus/usb/devices | grep "Lev=02 Prnt=02 Port=00" | cut -d '=' -f 7 | head -c 3`
deviceport1=`cat /proc/bus/usb/devices | grep "Lev=02 Prnt=02 Port=01" | cut -d '=' -f 7 | head -c 3`
#deviceport2=`cat /proc/bus/usb/devices | grep "Lev=02 Prnt=02 Port=02" | cut -d '=' -f 7 | head -c 3`
deviceport0_line=`cat /proc/bus/usb/devices | grep -n "Lev=02 Prnt=02 Port=00" | cut -d ':' -f 1`
deviceport1_line=`cat /proc/bus/usb/devices | grep -n "Lev=02 Prnt=02 Port=01" | cut -d ':' -f 1`
device_SerialNumber=`cat /proc/scsi/usb-storage/$scsinum1 | grep "Serial Number" | cut -d ' ' -f 3`
device_line=`cat /proc/bus/usb/devices | grep -n "$device_SerialNumber" | cut -d ':' -f 1`

if [ "`nvram get scsi_count`" == "" ]; then
	scsinum=`expr $scsinum1 + 3`
else
	scsi_count=`nvram get scsi_count`
	scsinum=`expr $scsinum1 + $scsi_count`
fi
if [ $scsinum -lt 10 ]; then
	scsinum="  $scsinum"
elif [ $scsinum -lt 100 ]; then 
	scsinum=" $scsinum"
fi

echo "scsinum=$scsinum deviceport0=$deviceport0 deviceport1=$deviceport1 deviceport2=$deviceport2"

media_rmname=""
# mounted, assume we umount
if [ $mounted -ge 1 ]; then
media_rmname=`mount | grep $1 | cut -d '/' -f 5 | cut -d ' ' -f 1`
echo "R/media/$media_rmname" 
echo "R/media/$media_rmname"
widrive_aloha_out $media_rmname 
if [ "`echo $media_rmname | head -c 3`" == "USB" ] ; then
	rm -f /tmp/security_usb_login_list
	rm -f /tmp/security_usb_login_time
	killall security_usb_loop
	nvram set current_security_usb_in=0;
fi
killall sambaset smbd nmbd
if ! umount "/media/$media_rmname"; then
sambaset `ls /media` &
exit 1
fi

if ! rmdir "/media/$media_rmname"; then
sambaset `ls /media` &
exit 1
fi
sambaset `ls /media` &
# not mounted, lets mount under /media
else
count=0;
while [ "$media_name" == "" ]
do
	temp_driver=`cat /proc/bus/usb/devices | grep Driver=usb-storage | wc -l`
	if [ "$temp_driver" == "1" ]; then
		temp_port0=`cat /proc/bus/usb/devices | grep "Lev=02 Prnt=02 Port=00" | wc -l`
		temp_port1=`cat /proc/bus/usb/devices | grep "Lev=02 Prnt=02 Port=01" | wc -l`
		if [ "$temp_port0" == "$temp_port1" ]; then
			deviceport1=""
		fi
	fi
	if [ "$scsinum" == "$deviceport2" ]; then
	echo "***** Micro SD *****"
	if [ -d "/media/Micro_SD"  ]; then
		exit 1
	fi
	media_name="Micro_SD"
	fi
	
	if [ "$scsinum" == "$deviceport0" ]; then
	echo "***** SD Card *****"
	dir__cut=`echo "$1"| cut -c 4-0`
	dir_name="SD_Card$dir__cut"
	error_dir=`mount | grep "/media/$dir_name " | wc -l`
	if [ "$error_dir" == "1" ]; then
		sambaset `ls /media`
		killall lighttpd
		lighttpd -f /usr/lighttpd.conf
		if ! umount "/media/$dir_name"; then
			sambaset `ls /media` &
			exit 1
		fi
		if ! rmdir "/media/$dir_name"; then
			sambaset `ls /media` &
			exit 1
		fi
	fi
	
	if [ -d "/media/$dir_name"  ]; then
		exit 1
	fi	
	media_name="$dir_name"
	fi
	
	if [ "$scsinum" == "$deviceport1" ]; then
	echo "***** USB *****"
	dir__cut=`echo "$1"| cut -c 4-0`
	dir_name="USB$dir__cut"
	
	error_dir=`mount | grep "/media/$dir_name " | wc -l`
	if [ "$error_dir" == "1" ]; then
		sambaset `ls /media`
		killall lighttpd
		lighttpd -f /usr/lighttpd.conf
		if ! umount "/media/$dir_name"; then
			sambaset `ls /media` &
			exit 1
		fi
		if ! rmdir "/media/$dir_name"; then
			sambaset `ls /media` &
			exit 1
		fi
	fi
	if [ -d "/media/$dir_name"  ]; then
		exit 1
	fi
	nvram set current_security_usb_in=0
	
	media_name="$dir_name"
	fi
if [ "$media_name" == "" ]; then
	scsinum=`expr $scsinum + 1`
	if [ $scsinum -lt 10 ]; then
		scsinum="  $scsinum"
	elif [ $scsinum -lt 100 ]; then 
		scsinum=" $scsinum"
	fi
	count=`expr $count + 1`
	if [ $count -ge 10 ]; then
		if [ "$device_line" == "" ]; then
			exit 1
		elif [ "$deviceport0_line" == "" ]; then
			scsinum="$deviceport1";
		elif [ "$deviceport1_line" == "" ]; then
			scsinum="$deviceport0";
		elif [ $deviceport0_line -gt $deviceport1_line ]; then
			if [ $device_line -gt $deviceport0_line ]; then
				scsinum="$deviceport0";
			else
				scsinum="$deviceport1";
			fi
		else
			if [ $device_line -gt $deviceport1_line ]; then
				scsinum="$deviceport1";
			else
				scsinum="$deviceport0";
			fi
		fi
	fi
	if [ $count -ge 11 ]; then
		exit 1
	fi
fi
done
if [ "$media_name" != "" ]; then
	if [ $count -ne 0 ]; then
		scsinum=`expr $scsinum - $scsinum1`
		nvram set scsi_count=$scsinum
	fi
fi
if ! mkdir -p "/media/$media_name"; then
exit 1
fi
chkexfat -f /dev/$1
##chkhfs -f /dev/$1
chkntfs -f /dev/$1
mounted=`mount | grep $1 | wc -l`
num=3
while [ $mounted -lt 1 -a $num -gt 0 ]
do
	mount -o umask=000 "/dev/$1" "/media/$media_name"		
	mounted=`mount | grep $1 | wc -l`
	num=`expr $num - 1`
done
#mounted=`mount | grep $1 | wc -l`
#while [ $mounted -lt 1 -a $num -lt 3 ]
#do
#	ntfs-3g "/dev/$1" "/media/$media_name" -o umask=000,force
#	mounted=`mount | grep $1 | wc -l`
#	num=`expr $num + 1`
#done

if [ $mounted -lt 1 ]; then
rmdir "/media/$media_name"
exit 1
fi
echo "A/media/$media_name" 
echo "A/media/$media_name"
sambaset `ls /media` & 
widrive_aloha $media_name
fi

exit 0
