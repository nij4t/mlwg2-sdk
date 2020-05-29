#! /bin/sh

#FW_buf=`ls /media/*/Kingston_widrive_v*.*.*.bin`
FW_buf=`ls /media/*/mlw5200fw_v*.*.*.bin`
nvram upgrade $FW_buf

FW_buf2=`ls /media/*/mlwG2_v*.*.*.bin`
nvram upgrade $FW_buf2

exit 0

