#! /bin/sh

sd_buf=`ls /dev/sd*`
count=1
dev_sd="";
dev_sd_temp="";
while [ "`echo $sd_buf | cut -d ' ' -f $count`" != "" ]
	do
	dev_sd=`echo $sd_buf | cut -d ' ' -f $count| cut -d '/' -f 3`
	if [ "$dev_sd" == "$dev_sd_temp" ]; then
		exit 0
	fi
	dev_sd_temp="$dev_sd";
	mounted=`mount | grep "$dev_sd " | wc -l`
	if [ $mounted -lt 1 ]; then
		if [ `expr length $dev_sd` -lt 4 ]; then
			/sbin/automount_basic.sh $dev_sd
		else
			/sbin/automount.sh $dev_sd
		fi
	fi
	count=`expr $count + 1`
done

exit 0

