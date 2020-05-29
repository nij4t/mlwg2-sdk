#! /bin/sh

while [ 1 ]
do
	sd=`ls /media/ | wc -l`
	if [ "$sd" != "0" ];then
		sync
	fi
	sleep 5
done
exit 0

