#
# gt_utils Makefile
#
#
#


TOOL_CHAIN_PATH=/opt/buildroot-gcc342/bin/
CC=$(TOOL_CHAIN_PATH)mipsel-linux-gcc
LD=$(TOOL_CHAIN_PATH)mipsel-linux-ld
STRIP=$(TOOL_CHAIN_PATH)mipsel-linux-strip
BUILD_ROOTFS_DIR=../../romfs/
FILE=shell/
FILEPATH=../../romfs/usr/

#CFLAGS += -I../nvram
CFLAGS += -DLAN_PORT
#CFLAGS += -DWAN_PORT
CFLAGS += -I../nvram -I../../linux-2.6.36.x/include/

all:
	make clean
	make build

build: libgtutils.so ntp gt_utils link_detect rc4 wlan_forward led pbc_detect sitesurvey memory_monitor wan_check checkWanLan wifidetect power_check auto_connect shutdown_check security_usb_loop
LDFLAGS += -L../nvram -lnvram -L../gt_log -lgtlog -lpthread
OBJ = ppp_util.o firewall.o wan.o lan.o get_info.o route.o ddns.o upnp_util.o bringup_pppoer.o ntp_util.o syslog.o bringup_dhcpr.o dhcpc_lease_time.o nslookup.o qos.o wifi.o wlan_api.o bridge.o wan_check_lib.o killall.o clearStaticLeases.o downgrade_funs.o ee_flash.o 

clean:
	rm -f *.so gt_utils link_detect pbc_detect led memory_monitor sitesurvey wlan_forward ntp wan_check checkWanLan wifidetect power_check auto_connect shutdown_check security_usb_loop

sitesurvey: sitesurvey.o
	$(CC) $(CFLAGS) -o $@ sitesurvey.o $(LDFLAGS)

memory_monitor: memory_monitor.o
	$(CC) $(CFLAGS) -o $@ memory_monitor.o

checkWanLan: checkWanLan.o
	$(CC) $(CFLAGS) -o $@ checkWanLan.o $(LDFLAGS)

ntp: ntp.o
	$(CC) $(CFLAGS) -o $@ ntp.o $(LDFLAGS)

wan_check: wan_check_lib.o wan_check.o
	$(CC) $(CFLAGS) -o $@ wan_check.o wan_check_lib.o $(LDFLAGS) -L. -lgtutils

rc4: 
	$(CC) -o rc4 rc4.c libcrypto.a -I../openssl-0.9.8e/include/

$(OBJ):

libgtutils.so: $(OBJ)
	$(LD) -shared -o $@ $^

gt_utils: main.o 
	$(CC) $(CFLAGS) -o $@ main.o $(LDFLAGS) -L. -lgtutils
	$(STRIP) gt_utils 

link_detect: link_detect.o
	$(CC) $(CFLAGS) -o $@ link_detect.o $(LDFLAGS) -L. -lgtutils 

wlan_forward: wlan_forward.o
	$(CC) $(CFLAGS) -o $@ wlan_forward.o $(LDFLAGS)

led:
#	$(CC) -c -DLED_LIB led_control.c
	$(LD) -shared -o libled.so led_control.o
#	rm -f led_control.o
#	$(CC) $(CFLAGS) -c led_control.c
	$(CC) $(CFLAGS) -o led led_control.o

pbc_detect: pbc_detect.o
	$(CC) $(CFLAGS) -o $@ pbc_detect.o $(LDFLAGS) -L. -lgtutils -L. -lled

wifidetect: wifidetect.o
	$(CC) $(CFLAGS) -o $@ wifidetect.o $(LDFLAGS) -L. -lgtutils -L. -lled

auto_connect: auto_connect.o
	$(CC) $(CFLAGS) -o $@ auto_connect.o $(LDFLAGS) -L. -lgtutils -L. -lled

power_check: power_check.o
	$(CC) $(CFLAGS) -o $@ power_check.o $(LDFLAGS) -L. -lgtutils -L. -lled

shutdown_check: shutdown_check.o
	$(CC) $(CFLAGS) -o $@ shutdown_check.o $(LDFLAGS) -L. -lgtutils -L. -lled

security_usb_loop: security_usb_loop.o
	$(CC) $(CFLAGS) -o $@ security_usb_loop.o $(LDFLAGS) -L. -lgtutils

romfs:
	$(ROMFSINST) /usr/sbin/sitesurvey
	$(ROMFSINST) /usr/sbin/ntp
	$(ROMFSINST) /usr/sbin/gt_utils
	$(ROMFSINST) /usr/sbin/wan_check
	$(ROMFSINST) /usr/sbin/checkWanLan
	$(ROMFSINST) /usr/sbin/link_detect
	$(ROMFSINST) /usr/sbin/pbc_detect
#	$(ROMFSINST) /usr/sbin/rc4
	$(ROMFSINST) /usr/sbin/wlan_forward
	$(ROMFSINST) /usr/sbin/led
	$(ROMFSINST) /lib/libgtutils.so
	$(ROMFSINST) /lib/libled.so
	$(ROMFSINST) /usr/sbin/wifidetect
	$(ROMFSINST) /usr/sbin/power_check
	$(ROMFSINST) /usr/sbin/auto_connect
	$(ROMFSINST) /usr/sbin/shutdown_check
	$(ROMFSINST) /usr/sbin/security_usb_loop
	
	mkdir -p ${BUILD_ROOTFS_DIR}usr/shell/
	cp shell/settimezone.sh ${BUILD_ROOTFS_DIR}usr/shell/settimezone.sh
	cp shell/dnsmasq.sh ${BUILD_ROOTFS_DIR}usr/shell/dnsmasq.sh
	cp shell/ip-down ${BUILD_ROOTFS_DIR}usr/shell/ip-down
	cp shell/ip-up ${BUILD_ROOTFS_DIR}usr/shell/ip-up
	cp shell/ntp.sh ${BUILD_ROOTFS_DIR}usr/shell/ntp.sh
	cp shell/ntpupdate.sh ${BUILD_ROOTFS_DIR}usr/shell/ntpupdate.sh
	cp shell/staticip.sh ${BUILD_ROOTFS_DIR}usr/shell/staticip.sh
	cp shell/udhcpc.script ${BUILD_ROOTFS_DIR}usr/shell/udhcpc.script
	cp shell/udhcpc.br0.script ${BUILD_ROOTFS_DIR}usr/shell/udhcpc.br0.script
	cp shell/udhcpc.lan_check.script ${BUILD_ROOTFS_DIR}usr/shell/udhcpc.lan_check.script
	cp shell/udhcpc.sh ${BUILD_ROOTFS_DIR}usr/shell/udhcpc.sh
	cp shell/udhcpd.sh ${BUILD_ROOTFS_DIR}usr/shell/udhcpd.sh
	cp config/services ${BUILD_ROOTFS_DIR}etc/services
	make -C "../../CRC_tool/"  backup_inner_ver 
	#Jayo 2010/04/20: Install config-vlan.sh to romfs 
	cp ../rt2880_app/scripts/config-vlan.sh ${BUILD_ROOTFS_DIR}/sbin/config-vlan.sh
	sed -e '/^#/d' ${ROOTDIR}/${CONFIG_LINUXDIR}/.config > .config
	chmod 755 .config
	$(ROMFSINST) .config /sbin/config.sh
	#Jayo 2010/04/20 End
	#Jason 2010.10.01: use to reboot when LAN disconnection at GUI upgrade
	cp shell/GUI_upgrade_terminate.sh ${BUILD_ROOTFS_DIR}usr/shell/GUI_upgrade_terminate.sh
	#Jason 2010.10.01

	
	ln -sf /usr/sbin/gt_utils ${BUILD_ROOTFS_DIR}usr/sbin/ppp_utils
	ln -sf /usr/sbin/gt_utils ${BUILD_ROOTFS_DIR}usr/sbin/firewall
	ln -sf /tmp/TZ ${BUILD_ROOTFS_DIR}etc/TZ
	ln -sf /tmp/hosts ${BUILD_ROOTFS_DIR}etc/hosts
	ln -sf /tmp/resolv.conf ${BUILD_ROOTFS_DIR}etc/resolv.conf
	mkdir -p ${BUILD_ROOTFS_DIR}etc/ppp/peers/
	ln -sf /tmp/options.0 ${BUILD_ROOTFS_DIR}etc/ppp/peers/pppoe0
	ln -sf /tmp/options.1 ${BUILD_ROOTFS_DIR}etc/ppp/peers/pppoe1
	ln -sf /tmp/options.2 ${BUILD_ROOTFS_DIR}etc/ppp/peers/pppoe2
	ln -sf /tmp/options.3 ${BUILD_ROOTFS_DIR}etc/ppp/peers/pppoe3
	ln -sf /tmp/options.4 ${BUILD_ROOTFS_DIR}etc/ppp/peers/pppoe4
	ln -sf /usr/shell/ip-up ${BUILD_ROOTFS_DIR}etc/ppp/ip-up
	ln -sf /usr/shell/ip-down ${BUILD_ROOTFS_DIR}etc/ppp/ip-down
	ln -sf /tmp/log ${BUILD_ROOTFS_DIR}/dev/log

