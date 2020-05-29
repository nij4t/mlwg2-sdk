#! /bin/sh

result="`ls /dev/ttyUSB0 | wc -l`";
while [ "$result" == "1" ]
do
	sleep 5;
	result="`ls /dev/ttyUSB0 | wc -l`";
done
/sbin/autoconn3G.sh disconnect $
exit 0
