#
#	Makefile -- Build instructions for user level apps
#

.EXPORT_ALL_VARIABLES:
.PHONY: config all romfs clean prune

#
# Include architecture specific build rules.
#
ifndef ROOTDIR
ROOTDIR=..
endif

UCLINUX_BUILD_USER=1
-include $(LINUX_CONFIG)
-include $(CONFIG_CONFIG)
-include $(BUSYBOX_CONFIG)
-include $(ARCH_CONFIG)
-include $(MODULES_CONFIG)

VEND=$(ROOTDIR)/vendors

#
# must run the vendor build first
#
dir_v = $(VEND)/$(CONFIG_VENDOR)/$(CONFIG_PRODUCT)/.
dir_p = $(ROOTDIR)/prop

dir_y = 
dir_n =
dir_  =

dir_$(CONFIG_GT_LIB_LIBNVRAM_FORCE)	    += nvram
dir_$(CONFIG_GT_LIB_LOG_FORCE)	    	    += gt_log
dir_$(CONFIG_GT_UTILITIES_FORCE)	    += gt_utilities
dir_$(CONFIG_GT_APP_QOS_FORCE)	    	    += gt_qos
dir_$(CONFIG_GT_WLAN_UTILITY_FORCE)	    += rtmpinit
dir_$(CONFIG_GT_MASS_PRODUCT_FORCE)         += mass_product
dir_$(CONFIG_GT_IPV6_PASSTHROUGH_UTILITY)   += ipv6passthru_util
dir_$(CONFIG_GT_AP_SERV_UTILITY)   	    += ap_serv
dir_$(CONFIG_GT_APP_PSNTP_FORCE)	    += psntp-0.1
dir_$(CONFIG_GT_APP_DDNS_FORCE)	    	    += ddns
dir_$(CONFIG_GT_APP_GUI_FORCE)	    	    += gt_httpd
dir_$(CONFIG_USER_802_1X)		    += 802.1x
dir_$(CONFIG_USER_RT2561_802_1X)	    += rt61apd
dir_$(CONFIG_USER_BIGPOND) 		    += bigpond
dir_$(CONFIG_USER_BONNIE)		    += bonnie++
dir_$(CONFIG_USER_BRCTL_BRCTL)              += bridge-utils
dir_$(CONFIG_USER_BT_UTILS)                 += bt-utils
dir_$(CONFIG_USER_BUSYBOX_BUSYBOX)          += busybox
dir_$(CONFIG_USER_CTORRENT)                 += ctorrent-dnh3.2
dir_$(CONFIG_USER_DHCP6)                    += dhcp6
dir_$(CONFIG_DHRYSTONE)		    	    += dhrystone
##dir_$(CONFIG_USER_DNSMASQ)                  += dnsmasq-2.40
#dir_$(CONFIG_USER_DNSMASQ)                  += dnsmasq-2.59
dir_$(CONFIG_USER_DNSMASQ)                  += dnsmasq-2.22
dir_$(CONFIG_USER_DISKTYPE)		    += disktype-9
dir_$(CONFIG_USER_ETHTOOL)		    += ethtool
dir_$(CONFIG_USER_GDBSERVER_GDBREPLAY)      += gdbserver
dir_$(CONFIG_USER_GDBSERVER_GDBSERVER)      += gdbserver
dir_$(CONFIG_USER_MATRIXSSL)                += matrixssl-1.8.3
##dir_$(CONFIG_USER_GOAHEAD_HTTPD)            += goahead
ifeq ($(CONFIG_LINUXDIR), linux-2.6.21.x)
#ifeq ($(CONFIG_RALINKAPP_SWQOS),y)
dir_$(CONFIG_USER_IPTABLES_IPTABLES)	    += iptables-1.4.0rc1
dir_$(CONFIG_USER_IPTABLES_IP6TABLES)	    += iptables-1.4.0rc1
#else
#dir_$(CONFIG_USER_IPTABLES_IPTABLES)   	    += iptables
#dir_$(CONFIG_USER_IPTABLES_IP6TABLES)	    += iptables
#endif
else #linux 2.4
ifeq ($(CONFIG_LINUXDIR), linux-2.6.36.x)
dir_$(CONFIG_USER_IPTABLES_IPTABLES)	    += iptables-1.4.10
dir_$(CONFIG_USER_IPTABLES_IP6TABLES)	    += iptables-1.4.10
else
ifeq ($(CONFIG_LINUXDIR), linux-2.6.36MT.x)
dir_$(CONFIG_USER_IPTABLES_IPTABLES)	    += iptables-1.4.10
dir_$(CONFIG_USER_IPTABLES_IP6TABLES)	    += iptables-1.4.10
else
dir_$(CONFIG_USER_IPTABLES_IPTABLES)	    += iptables
dir_$(CONFIG_USER_IPTABLES_IP6TABLES)	    += iptables
endif
endif
endif
#dir_$(CONFIG_GT_APP_MINIUPNPD_FORCE)	    += miniupnpd 
dir_$(CONFIG_GT_APP_MINIUPNPD_FORCE)	    += miniupnpd-1.6

dir_$(CONFIG_GT_APP_MINIUPNPC_FORCE)        += miniupnpc-1.3

dir_$(CONFIG_USER_IXIA_ENDPOINT)	    += ixia_endpoint
dir_$(CONFIG_USER_INADYN)                   += inadyn
dir_$(CONFIG_USER_IGMP_PROXY)  		    += igmpproxy
dir_$(CONFIG_USER_IPROUTE2)  		    += iproute2-2.6.24-rc7
dir_$(CONFIG_USER_LMBENCH3)		    += lmbench3
dir_$(CONFIG_USER_LLTD)			    += lldt
dir_$(CONFIG_USER_LSUSB)		    += lsusb
##dir_$(CONFIG_USER_MINIUPNPD)		    += miniupnpd-1.1
dir_$(CONFIG_USER_MTDUTILS)                 += mtd-utils
dir_$(CONFIG_USER_MTD_WRITE)         	    += mtd_write
dir_$(CONFIG_USER_MKDOSFS)                  += mkdosfs-2.11
dir_$(CONFIG_USER_NETCAT)		    += netcat
dir_$(CONFIG_USER_NETSTAT_NAT)		    += netstat-nat
dir_$(CONFIG_USER_NTPCLIENT_NTPCLIENT)      += ntpclient
dir_$(CONFIG_USER_NTPCLIENT_ADJTIMEX)       += ntpclient
##dir_$(CONFIG_USER_NTFS3G)		    += ntfs-3g
#dir_$(CONFIG_USER_NTFSPROGS)                += ntfsprogs
dir_$(CONFIG_USER_OPENSWAN)                 += openswan-2.4.9
dir_$(CONFIG_USER_OPENSSL)	   	    += openssl-0.9.8e
dir_$(CONFIG_USER_OPENSSL)	   	    += openssl-0.9.8e
dir_$(CONFIG_USER_PORTMAP)                  += portmap
dir_$(CONFIG_USER_PPPOECD)                  += ppp-2.4.1/pppoecd
dir_$(CONFIG_USER_PPPD)                     += ppp-2.4.2
dir_$(CONFIG_USER_PPPD_PPPSTATS)            += ppp-2.4.4
dir_$(CONFIG_USER_PPPD_PPPDUMP)             += ppp-2.4.4
dir_$(CONFIG_USER_PPTP_CLIENT)              += pptp-client
##dir_$(CONFIG_USER_PCIUTIL_LSPCI)            += pciutils-3.0.0
##dir_$(CONFIG_USER_PCIUTIL_SETPCI)           += pciutils-3.0.0
dir_$(CONFIG_USER_P910ND)                   += p910nd-0.91
#dir_$(CONFIG_USER_RADVD)                    += radvd-1.0
dir_$(CONFIG_RALINKAPP)		    	    += rt2880_app
dir_$(CONFIG_USER_RP_L2TP)		    += rp-l2tp-0.4
dir_$(CONFIG_USER_RPPPPOE_RELAY)  	    += rp-pppoe-3.8
dir_$(CONFIG_USER_RPPPPOE_SNIFF)  	    += rp-pppoe-3.8
dir_$(CONFIG_USER_STRACE_STRACE)            += strace
dir_$(CONFIG_USER_STUPID_FTPD)              += stupid-ftpd
#dir_$(CONFIG_USER_SAMBA)		    += samba-3.0.2
dir_$(CONFIG_USER_SAMBA)		    += samba-3.0.2_GNE
dir_$(CONFIG_USER_SAMBA)		    += samba-3.0.2_set
dir_$(CONFIG_USER_TOTD)                     += totd-1.5
dir_$(CONFIG_USER_TCPDUMP)                  += tcpdump
dir_$(CONFIG_USER_UVC_STREAM)		    += uvc_stream
dir_$(CONFIG_USER_UPNP_IGD)                 += linux-igd
dir_$(CONFIG_USER_WIRELESS_TOOLS)           += wireless_tools
dir_$(CONFIG_USER_WATCHDOG)                 += watchdog-5.6
dir_$(CONFIG_USER_WPA_SUPPLICANT)	    += wpa_supplicant-0.5.7
dir_$(CONFIG_USER_WSC)			    += wsc_upnp
dir_$(CONFIG_USER_ETHTOOL)		    += ethtool
dir_$(CONFIG_USER_ZEBRA)		    += zebra-0.95a_ripd
dir_$(CONFIG_USER_USBMODESWITCH)	    += usb_modeswitch-0.9.5
dir_$(CONFIG_USER_COMGT)		    += comgt-0.32
dir_$(CONFIG_USER_SDPARM) 		    += sdparm-1.02
dir_$(CONFIG_USER_3G)	 		    += hso-1.6
#dir_$(CONFIG_USER_ECO)	 		    += eco
# for changing uboot env for MST RAM-boot inspection FW
dir_y += gt_fwsetenv
dir_y += read_fw
dir_y += mdev
dir_y += ControlUtilityServer
dir_y += widrive_aloha
dir_y += easytest
##dir_y += iperf-2.0.5
dir_y += iperf-1.7.0
dir_y += matrixssl
dir_y += matrixtunnel
dir_y += libxml2-2.9.1
dir_y += libuuid-1.0.2
dir_y += lighttpd-1.4.25
# DLNA DMS
dir_y       += dlna-GTK-DMS
dir_y       += DMS_set
#dir_y       += dlna-autoipd
dir_y       += uboot_env
dir_y       += KWBox

#TEST
#dir_$(CONFIG_USER_HELLO_WORLD)		    += hello_world
#--

all: config
	#$(MAKE) -j$(HOST_NCPU) $(sort $(dir_y) $(dir_v) $(dir_p)) || exit $$?
	$(MAKE) -j$(HOST_NCPU) $(dir_y) $(dir_v) $(dir_p) || exit $$?

#
# add directory dependancies here
#
.PHONY: $(sort $(dir_y) $(dir_p))

$(sort $(dir_y) $(dir_p)):  config
	[ ! -d "$@" ] || ( touch $@/.sgbuilt_user && $(MAKE) -j1 -C $@ ) || exit $$?

%_only:
	touch $(@:_only=)/.sgbuilt_user && $(MAKE) -j1 -C $(@:_only=)

%_clean:
	$(MAKE) -j1 -C $(@:_clean=) clean; rm -f $(@:_clean=)/.sgbuilt_user; true

romfs:
	for i in $(sort $(dir_y)) $(dir_p) ; do \
		[ ! -d $$i ] || $(MAKE) -C $$i romfs || exit $$? ; \
	done

clean:
	-for i in $(dir_v) $(sort $(dir_y) $(dir_n) $(dir_)) $(dir_p) ; do \
		if [ -f $$i/.sgbuilt_user ]; then \
			$(MAKE) -C $$i clean ; \
			rm -f $$i/.sgbuilt_user; \
		fi; \
	done

prune:
	-for i in $(sort $(dir_n) $(dir_)) ; do \
		found=0; \
		for j in $(sort $(dir_y)) ; do \
			if [ $$i == $$j ]; then \
				found=1; \
			fi; \
		done; \
		if [ $$found == 0 ]; then \
			[ "$$i" = "openswan" ] && make prune_openswan; \
			rm -fr $$i; \
		fi; \
	done

prune_openswan:
	@mkdir $(ROOTDIR)/$@
	@mv $(ROOTDIR)/openswan/linux/net/ipsec/{Config.in*,Kconfig} $(ROOTDIR)/$@/
	@rm -fr $(ROOTDIR)/openswan
	@mkdir -p $(ROOTDIR)/openswan/linux/net/ipsec
	@mv $(ROOTDIR)/$@/* $(ROOTDIR)/openswan/linux/net/ipsec/
	@rm -fr $(ROOTDIR)/$@
