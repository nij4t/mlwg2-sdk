#! /bin/sh

if [ `nvram get easy_test` -eq 0 ]; then
	if [ `ls /media/*/gemtek_easytest.txt | wc -l` -ne 0 ]; then
		easytest &
		iperf -s &
	fi
fi

exit 0

