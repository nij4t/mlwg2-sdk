#!/bin/sh
PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin:/usr/shell
RC_CONF=/etc/rc.conf
SCRIPT=$0

TZFILE=/etc/TZ

#. $RC_CONF
DatetimeTimezone=`nvram get DatetimeTimezone`
DatetimeDstEnable=`nvram get DatetimeDstEnable`
DatetimeDstStart=`nvram get DatetimeDstStart`
DatetimeDstEnd=`nvram get DatetimeDstEnd`

[ "$DatetimeTimezone" = "" ] && DatetimeTimezone=GMT-8
tzdata=$DatetimeTimezone
case $DatetimeTimezone in
	[A-Z][A-Z][A-Z])
		tzdata=$DatetimeTimezone+0;;
	[A-Z][A-Z][A-Z][0-9]* |\
	[A-Z][A-Z][A-Z][01][0-9]* |\
	[A-Z][A-Z][A-Z]+[0-9]* |\
	[A-Z][A-Z][A-Z]+[01][0-9]*)
		tzdata=`echo $DatetimeTimezone | sed 's/+*[0-9].*//'`-`echo $DatetimeTimezone | sed 's/^[A-Z]\{3,4\}+*//'`;;
	[A-Z][A-Z][A-Z]-[0-9]* |\
	[A-Z][A-Z][A-Z]-[01][0-9]*)
		tzdata=`echo $DatetimeTimezone | sed 's/-/+/'`;;
	*)
		echo ERROR;
		exit 1;;
esac

if [ "$DatetimeDstEnable" = "1" ]; then
	tzdata="$tzdata"DST,M`echo $DatetimeDstStart | sed -e 's/.\{8\}$//'`.`echo $DatetimeDstStart | sed -e 's/^.\{2\}//' -e 's/.\{6\}$//'`.`echo $DatetimeDstStart | sed -e 's/^.\{4\}//' -e 's/.\{4\}$//'`/`echo $DatetimeDstStart | sed -e 's/^.\{6\}//' -e 's/.\{2\}$//'`:`echo $DatetimeDstStart | sed -e 's/^.\{8\}//'`,M`echo $DatetimeDstEnd | sed -e 's/.\{8\}$//'`.`echo $DatetimeDstEnd | sed -e 's/^.\{2\}//' -e 's/.\{6\}$//'`.`echo $DatetimeDstEnd | sed -e 's/^.\{4\}//' -e 's/.\{4\}$//'`/`echo $DatetimeDstEnd | sed -e 's/^.\{6\}//' -e 's/.\{2\}$//'`:`echo $DatetimeDstEnd | sed -e 's/^.\{8\}//'`
fi

# tricky, the first opened file is used as stdout if stdout(td 1) is closed.
# so we output to /dev/null first
echo "" > /dev/null
#echo $tzdata > $TZFILE
echo "GMT-8" > $TZFILE

