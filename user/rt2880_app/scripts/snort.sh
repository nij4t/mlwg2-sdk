#!/bin/sh
#
# $Id: //WIFI_SOC/release/SDK_4_1_0_0/source/user/rt2880_app/scripts/snort.sh#1 $
#
# usage: snort.sh
#

se=`nvram_get 2860 SnortEnable`

killall -q snort

# debug
#echo "se=$se"

#run snort
if [ "$se" = "1" ]; then
/bin/snort -c /etc_ro/snort.conf -l /var/log -s &
fi
