#!/bin/sh

ROUNDCOUNT=15
while true; do
  CHKBIT=`nvram get HTTPUploadCheck`
  #echo "HTTPUploadCheck=$CHKBIT"
  if [ "$CHKBIT" == "1" ] && [ "$ROUNDCOUNT" == "0" ]; then
    #echo "REBOOT"
    reboot
  elif [ "$CHKBIT" == "1" ]; then
    ROUNDCOUNT=`expr $ROUNDCOUNT - 1`
    #echo "ROUNDCOUNT=$ROUNDCOUNT"
  else
    #echo "BREAK"
    break
  fi
  sleep 2
done
