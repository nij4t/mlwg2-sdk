#
# rtmpinit Makefile
#

#Target Directories
#TOP := $(shell (cd ../../ && pwd))
#TARGET := $(TOP)/rootfs_mrvl_rd2_3_0
#INSTALL_EXEC := $(TARGET)/usr/sbin

TOOL_CHAIN_PATH=/opt/buildroot-gcc342/bin/
CC=$(TOOL_CHAIN_PATH)mipsel-linux-gcc
LD=$(TOOL_CHAIN_PATH)mipsel-linux-ld
STRIP=$(TOOL_CHAIN_PATH)mipsel-linux-strip

CFLAGS += -I../../linux-2.6.21.x/include/



INSTALL_EXEC := ../../../buildroot/build_mips_nofpu/root/usr/sbin

CFLAGS	+= -I. -Wall -I../nvram
CFLAGS = -O2 -Wall -g

#jeff:add 2007/7/9
#CFLAGS += -I../log
#LDFLAGS += -L../log -llog
#jef-end

LDFLAGS	+= -L.
LDFLAGS += -L../nvram -lnvram -L../gt_utilities -lgtutils -lpthread -L../gt_log -lgtlog -L../gt_utilities/ -lled


OBJS := rtmpinit.o md5.o
OBJS_WLANRESERV := wlan_reserv.o
OBJS_WSCTRIGGER := wsc_trigger.o
OBJS_APCLI	:= ap_client.o
OBJS_APCLI_RUN	:= ap_client_run.o
OBJS_WLANLED	:= wlan_led_ctrl.o


all: rtmpinit wlan_reserv wsc_trigger ap_client wlan_led_ctrl ap_client_run

clean:
	rm -f *.bak *~ rtmpinit wlan_reserv wsc_trigger ap_client wlan_led_ctrl ap_client_run

rtmpinit: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)
	
wlan_reserv: $(OBJS_WLANRESERV)
	$(CC) -o $@ $^ $(LDFLAGS)

wsc_trigger: $(OBJS_WSCTRIGGER)
	$(CC) -o $@ $^ $(LDFLAGS)

ap_client: $(OBJS_APCLI)
	$(CC) -o $@ $^ $(LDFLAGS)

ap_client_run: $(OBJS_APCLI_RUN)
	$(CC) -o $@ $^ $(LDFLAGS)

wlan_led_ctrl: $(OBJS_WLANLED)
	$(CC) -o $@ $^ $(LDFLAGS)

#install: rtmpinit wlan_reserv wsc_trigger 
romfs:
	$(ROMFSINST) /usr/sbin/rtmpinit
	$(ROMFSINST) /usr/sbin/wlan_reserv
	$(ROMFSINST) /usr/sbin/wsc_trigger
	$(ROMFSINST) /usr/sbin/ap_client
	$(ROMFSINST) /usr/sbin/ap_client_run
	$(ROMFSINST) /usr/sbin/wlan_led_ctrl
	#install -D rtmpinit $(INSTALL_EXEC)/rtmpinit
	#install -D wlan_reserv $(INSTALL_EXEC)/wlan_reserv
	#install -D wsc_trigger $(INSTALL_EXEC)/wsc_trigger
	#install -D ap_client $(INSTALL_EXEC)/ap_client
	$(STRIP) /usr/sbin/rtmpinit
	$(STRIP) /usr/sbin/wlan_reserv
	$(STRIP) /usr/sbin/wsc_trigger
	$(STRIP) /usr/sbin/ap_client
	$(STRIP) /usr/sbin/ap_client_run
	$(STRIP) /usr/sbin/wlan_led_ctrl

