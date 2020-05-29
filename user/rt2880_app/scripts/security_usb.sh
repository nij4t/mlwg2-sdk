#! /bin/sh

#FW_buf=`ls /media/*/Kingston_widrive_v*.*.*.bin`
FW_buf=`ls /dev/sd*`
hdparm $FW_buf

exit 0

