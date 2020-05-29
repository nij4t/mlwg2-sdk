#! /bin/sh

while [ 1 ]
do
dev_buf=`ls /dev/sd*`
num=1
dev=`echo $dev_buf | cut -d ' ' -f $num`
dev_old=""
	while [ "$dev" != "$dev_old" -a "$dev" != "" ]
	do
		dev_rm_buf=`hdparm -g $dev | grep " 0/64/32"`
		if [ "$dev_rm_buf" != "" ]; then
			mounted=`mount | grep "$dev " | wc -l`
			media_rmname=""
			if [ $mounted -ge 1 ]; then
				media_rmname=`mount | grep $dev | cut -d '/' -f 5 | cut -d ' ' -f 1`
				echo "R/media/$media_rmname"
				echo "R/media/$media_rmname" 
				if ! umount "/media/$media_rmname"; then
					exit 1
				fi
				if ! rm -r "/media/$media_rmname"; then
					exit 1
				fi
			fi
		fi
		dev_rm_buf=`hdparm -g $dev | grep "sectors = 0"`
		mounted=`mount | grep "$dev " | wc -l`
		if [ "$mounted" == "0" -a "$dev_rm_buf" == "" -a `expr length $dev` == "8" -a ! -e $dev"1" ]; then
			hdparm -i $dev > /dev/null
			sleep 2
			automount_basic.sh `echo $dev | tail -c 4`
		fi
		hdparm -i $dev > /dev/null
		num=`expr $num + 1`
		dev_old="$dev"
		dev=`echo $dev_buf | cut -d ' ' -f $num`
	done
	`sleep 10`
done
exit 0

