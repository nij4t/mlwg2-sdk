###########################################################################
#
# Makefile -- Top level uClinux makefile.
#
# Copyright (c) 2001-2004, SnapGear (www.snapgear.com)
# Copyright (c) 2001, Lineo
#

VERSIONPKG = 3.2.0
VERSIONSTR = $(CONFIG_VENDOR)/$(CONFIG_PRODUCT) Version $(VERSIONPKG)

############################################################################
#
# Lets work out what the user wants, and if they have configured us yet
#

ifeq (.config,$(wildcard .config))
include .config

#changed by Steven Liu
#all: ucfront cksum subdirs romfs image
all: change_mod uClibc++_only lib_only user_only modules romfs linux ufsd image
#all: change_mod uClibc++_only lib_only user_only  modules romfs linux image
else
all: config_error
endif

change_mod:
	cd vendors/Ralink/MT7620/; chmod 655 makedevlinks;
	cd vendors/Ralink/MT7620/; chmod 655 rcS;
	cd user/psntp-0.1/; chmod 655 psntpdate;
	cd user/rt2880_app/scripts; chmod 675 usb_autocheck.sh;
	chmod 675 do_CRC.sh;
	cd user/rt2880_app/scripts; chmod 675 automount.sh;
	cd user/rt2880_app/scripts; chmod 675 automount_basic.sh;
	cd user/rt2880_app/scripts; chmod 675 upgrade.sh;
	cd user/gt_utilities/shell; chmod 675 *;
	cd user/rt2880_app/scripts; chmod 675 usb_sync.sh;
	cd user/rt2880_app/scripts; chmod 675 checkeasy.sh;
	cd user/rt2880_app/scripts; chmod 675 *.sh;
	cd lib/pcre-8.01/; chmod 777 configure;
	cd user/ufsd_file; chmod 675 configure;
	cd lib/libsqlite3/; chmod 777 configure;
	cd user/ppp-2.4.2/pppd; chmod 777 ip-down;
	cd user/ppp-2.4.2/pppd; chmod 777 ip-up;
	cd user/KWBox; chmod 777 Tester_run;

ufsd:
	cd user/ufsd_file/;\
	./configure --host="mips" --target="mips" CC=/opt/buildroot-gcc342/bin/mipsel-linux-uclibc-gcc --with-ks-dir=$(ROOTDIR)/$(LINUXDIR) --with-kb-dir=$(ROOTDIR)/$(LINUXDIR) make driver
	cd user/ufsd_file/; make clean;
	cd user/ufsd_file/;\
	make ARCH=mips CROSS-COMPILE=/opt/buildroot-gcc342/bin/mipsel-linux-uclibc-gcc driver

HTTPDDIR = user/httpd-2.2.9/
httpd-2.2.9:
	chmod 744 $(HTTPDDIR)configure;
	chmod 744 $(HTTPDDIR)srclib/apr/apr-1-config;
	chmod 744 $(HTTPDDIR)srclib/apr/build/mkdir.sh;
	chmod 744 $(HTTPDDIR)srclib/apr/build/get-version.sh;
	chmod 744 $(HTTPDDIR)srclib/apr-util/build/get-version.sh;
	chmod 744 $(HTTPDDIR)srclib/apr-util/apu-1-config;
	chmod 744 $(HTTPDDIR)build/mkdir.sh;
	chmod 744 $(HTTPDDIR)build/PrintPath;
	chmod 744 $(HTTPDDIR)build/get-version.sh;
	chmod 744 $(HTTPDDIR)build/install.sh;
	cd $(HTTPDDIR);\
	LDFLAGS="-L$(ROOTDIR)/user/nvram -lnvram" CFLAGS="-I$(ROOTDIR)/user/nvram -lpthread -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64" ./configure --host=arm-linux --target=arm-linux CC=/opt/buildroot-gcc342/bin/mipsel-linux-uclibc-gcc --enable-dav --enable-dav-fs --enable-dav-lock --with-included-apr --with-mpm=worker ac_cv_file__dev_zero=yes ac_cv_func_setpgrp_void=yes apr_cv_process_shared_works=yes apr_cv_mutex_robust_shared=yes apr_cv_tcp_nodelay_with_cork=yes ac_cv_sizeof_long_long=8;
	cd $(HTTPDDIR); $(MAKE) clean;
	cd $(HTTPDDIR)srclib/apr/; $(MAKE) clean;
	cd $(HTTPDDIR)srclib/apr-util/; $(MAKE) clean;
	cd $(HTTPDDIR)srclib/pcre/; $(MAKE) clean;
	-cd $(HTTPDDIR); $(MAKE);
	cp $(HTTPDDIR)backup/dftables $(HTTPDDIR)srclib/pcre/
	-cd $(HTTPDDIR); $(MAKE);
	sleep 5;
	cp $(HTTPDDIR)backup/gen_test_char $(HTTPDDIR)server/
	cd $(HTTPDDIR); $(MAKE);
	rm -rf $(HTTPDDIR)docs;
	cp -rf $(HTTPDDIR)docs_new $(HTTPDDIR)docs
##LDFLAGS="-L$(ROOTDIR)/user/nvram -lnvram" CFLAGS="-I$(ROOTDIR)/user/nvram -lpthread -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64" ./configure --host=arm-linux --target=arm-linux CC=/opt/buildroot-gcc342/bin/mipsel-linux-uclibc-gcc --enable-so --enable-dav --enable-dav-fs --enable-dav-lock --with-included-apr --with-mpm=worker --enable-ssl --with-ssl=/usr/local/ssl/ ac_cv_file__dev_zero=yes ac_cv_func_setpgrp_void=yes apr_cv_process_shared_works=yes apr_cv_mutex_robust_shared=yes apr_cv_tcp_nodelay_with_cork=yes ac_cv_sizeof_long=8;

libiconv-1.11:
	cd user/libiconv-1.11/; \
	$(MAKE) clean; \
	$(MAKE) ;



############################################################################
#
# Get the core stuff worked out
#

LINUXDIR = $(CONFIG_LINUXDIR)
LIBCDIR  = $(CONFIG_LIBCDIR)
ROOTDIR  = $(shell pwd)
PATH	 := $(PATH):$(ROOTDIR)/tools
HOSTCC   = cc
IMAGEDIR = $(ROOTDIR)/images
ROMFSDIR = $(ROOTDIR)/romfs
ROMFSINST= romfs-inst.sh
SCRIPTSDIR = $(ROOTDIR)/config/scripts
TFTPDIR    = /tftpboot
BUILD_START_STRING ?= $(shell date "+%a, %d %b %Y %T %z")

#
# For linux-2.6 kernel, it doesn't know VPATH parameter in Makefile.
# That means object files will be generated at source directory.
#
# When we want to built both AP and STA driver in root filesystem, we MUST disable compiler's -j option to 
# avoid different CPU build WiFi driver at the same time.
#
# by Steven
#
-include $(LINUXDIR)/.config

#ifneq ($(CONFIG_RT2860V2_AP),)
# ifneq ($(CONFIG_RT2860V2_STA),)
#  HOST_NCPU := 1   # AP & STA driver
# else
#  HOST_NCPU := $(shell if [ -f /proc/cpuinfo ]; then n=`grep -c processor /proc/cpuinfo`; if [ $$n -gt 1 ];then echo 2; else echo $$n; fi; else echo 1; fi)  
# endif
#else
#  HOST_NCPU := $(shell if [ -f /proc/cpuinfo ]; then n=`grep -c processor /proc/cpuinfo`; if [ $$n -gt 1 ];then echo 2; else echo $$n; fi; else echo 1; fi)  
#endif
#--

#uncomment this line if you want to use single cpu to compiile your code.
HOST_NCPU = 1

LINUX_CONFIG  = $(ROOTDIR)/$(LINUXDIR)/.config
CONFIG_CONFIG = $(ROOTDIR)/config/.config
BUSYBOX_CONFIG = $(ROOTDIR)/user/busybox/.config
MODULES_CONFIG = $(ROOTDIR)/modules/.config


CONFIG_SHELL := $(shell if [ -x "$$BASH" ]; then echo $$BASH; \
	  else if [ -x /bin/bash ]; then echo /bin/bash; \
	  else echo sh; fi ; fi)

ifeq (config.arch,$(wildcard config.arch))
ifeq ($(filter %_default, $(MAKECMDGOALS)),)
include config.arch
ARCH_CONFIG = $(ROOTDIR)/config.arch
export ARCH_CONFIG
endif
endif

# May use a different compiler for the kernel
KERNEL_CROSS_COMPILE ?= $(CROSS_COMPILE)
#KERNEL_CROSS_COMPILE = /opt/timesys/toolchains/mipsisa32r2el-linux/bin/mipsel-linux-
ifneq ($(SUBARCH),)
# Using UML, so make the kernel and non-kernel with different ARCHs
MAKEARCH = $(MAKE) ARCH=$(SUBARCH) CROSS_COMPILE=$(CROSS_COMPILE)
MAKEARCH_KERNEL = $(MAKE) ARCH=$(ARCH) SUBARCH=$(SUBARCH) CROSS_COMPILE=$(KERNEL_CROSS_COMPILE)
else
MAKEARCH = $(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE)
MAKEARCH_KERNEL = $(MAKE)  ARCH=$(ARCH) CROSS_COMPILE=$(KERNEL_CROSS_COMPILE)
endif
#DIRS    =  $(ROOTDIR)/vendors include uClibc++ user lib
DIRS    =  $(ROOTDIR)/vendors include uClibc++ lib user

export VENDOR PRODUCT ROOTDIR LINUXDIR HOSTCC CONFIG_SHELL
export CONFIG_CONFIG BUSYBOX_CONFIG LINUX_CONFIG MODULES_CONFIG ROMFSDIR SCRIPTSDIR
export VERSIONPKG VERSIONSTR ROMFSINST PATH IMAGEDIR RELFILES TFTPDIR
export BUILD_START_STRING
export HOST_NCPU

.PHONY: ucfront
ucfront: tools/ucfront/*.c
	$(MAKE) -C tools/ucfront
	ln -sf $(ROOTDIR)/tools/ucfront/ucfront tools/ucfront-gcc
	ln -sf $(ROOTDIR)/tools/ucfront/ucfront tools/ucfront-g++
	ln -sf $(ROOTDIR)/tools/ucfront/ucfront-ld tools/ucfront-ld

.PHONY: cksum
cksum: tools/sg-cksum/*.c
	$(MAKE) -C tools/sg-cksum
	ln -sf $(ROOTDIR)/tools/sg-cksum/cksum tools/cksum

############################################################################

#
# Config stuff,  we recall ourselves to load the new config.arch before
# running the kernel and other config scripts
#

.PHONY: config.tk config.in

config.in:
	@chmod u+x config/mkconfig
	config/mkconfig > config.in

config.tk: config.in
	$(MAKE) -C $(SCRIPTSDIR) tkparse
	ARCH=dummy $(SCRIPTSDIR)/tkparse < config.in > config.tmp
	@if [ -f /usr/local/bin/wish ];	then \
		echo '#!'"/usr/local/bin/wish -f" > config.tk; \
	else \
		echo '#!'"/usr/bin/wish -f" > config.tk; \
	fi
	cat $(SCRIPTSDIR)/header.tk >> ./config.tk
	cat config.tmp >> config.tk
	rm -f config.tmp
	echo "set defaults \"/dev/null\"" >> config.tk
	echo "set help_file \"config/Configure.help\"" >> config.tk
	cat $(SCRIPTSDIR)/tail.tk >> config.tk
	chmod 755 config.tk

.PHONY: xconfig
xconfig: config.tk
	@wish -f config.tk
	@if [ ! -f .config ]; then \
		echo; \
		echo "You have not saved your config, please re-run make config"; \
		echo; \
		exit 1; \
	 fi
	@chmod u+x config/setconfig
	@config/setconfig defaults
	@if egrep "^CONFIG_DEFAULTS_KERNEL=y" .config > /dev/null; then \
		$(MAKE) linux_xconfig; \
	 fi
	@if egrep "^CONFIG_DEFAULTS_MODULES=y" .config > /dev/null; then \
		$(MAKE) modules_xconfig; \
	 fi
	@if egrep "^CONFIG_DEFAULTS_VENDOR=y" .config > /dev/null; then \
		$(MAKE) config_xconfig; \
	 fi
	@if egrep "^CONFIG_DEFAULTS_BUSYBOX=y" .config > /dev/null; then \
		$(MAKE) -C user/busybox menuconfig; \
	 fi
	@if egrep "^CONFIG_DEFAULTS_UCLIBC=y" .config > /dev/null; then \
		$(MAKE) -C lib menuconfig; \
	 fi
	@if egrep "^CONFIG_DEFAULTS_UCLIBC_PLUS_PLUS=y" .config > /dev/null; then \
		$(MAKE) -C uClibc++ menuconfig; \
	 fi
	@config/setconfig final

.PHONY: config
config: config.in
	@HELP_FILE=config/Configure.help \
		$(CONFIG_SHELL) $(SCRIPTSDIR)/Configure config.in
	@chmod u+x config/setconfig
	@config/setconfig defaults
	@if egrep "^CONFIG_DEFAULTS_KERNEL=y" .config > /dev/null; then \
		$(MAKE) linux_config; \
	 fi
	@if egrep "^CONFIG_DEFAULTS_MODULES=y" .config > /dev/null; then \
		$(MAKE) modules_config; \
	 fi
	@if egrep "^CONFIG_DEFAULTS_VENDOR=y" .config > /dev/null; then \
		$(MAKE) config_config; \
	 fi
	@if egrep "^CONFIG_DEFAULTS_BUSYBOX=y" .config > /dev/null; then \
		$(MAKE) -C user/busybox menuconfig; \
	 fi
	@if egrep "^CONFIG_DEFAULTS_UCLIBC=y" .config > /dev/null; then \
		$(MAKE) -C lib menuconfig; \
	 fi
	@if egrep "^CONFIG_DEFAULTS_UCLIBC_PLUS_PLUS=y" .config > /dev/null; then \
		$(MAKE) -C uClibc++ menuconfig; \
	 fi
	@config/setconfig final

.PHONY: menuconfig
menuconfig: config.in
	$(MAKE) -C $(SCRIPTSDIR)/lxdialog all
	@HELP_FILE=config/Configure.help \
		$(CONFIG_SHELL) $(SCRIPTSDIR)/Menuconfig config.in
	@if [ ! -f .config ]; then \
		echo; \
		echo "You have not saved your config, please re-run make config"; \
		echo; \
		exit 1; \
	 fi
	@chmod u+x config/setconfig
	@config/setconfig defaults
	@if egrep "^CONFIG_DEFAULTS_KERNEL=y" .config > /dev/null; then \
		$(MAKE) linux_menuconfig; \
	 fi
	@if egrep "^CONFIG_DEFAULTS_MODULES=y" .config > /dev/null; then \
		$(MAKE) modules_menuconfig; \
	 fi
	@if egrep "^CONFIG_DEFAULTS_VENDOR=y" .config > /dev/null; then \
		$(MAKE) config_menuconfig; \
	 fi
	@if egrep "^CONFIG_DEFAULTS_BUSYBOX=y" .config > /dev/null; then \
		$(MAKE) -C user/busybox menuconfig; \
	 fi
	@if egrep "^CONFIG_DEFAULTS_UCLIBC=y" .config > /dev/null; then \
		$(MAKE) -C lib menuconfig; \
	 fi
	@if egrep "^CONFIG_DEFAULTS_UCLIBC_PLUS_PLUS=y" .config > /dev/null; then \
		$(MAKE) -C uClibc++ menuconfig; \
	 fi
	@config/setconfig final

.PHONY: oldconfig
oldconfig: config.in
	@HELP_FILE=config/Configure.help \
		$(CONFIG_SHELL) $(SCRIPTSDIR)/Configure -d config.in
	@$(MAKE) oldconfig_linux
	@$(MAKE) oldconfig_modules
	@$(MAKE) oldconfig_config
	@chmod u+x config/setconfig
	@config/setconfig final

.PHONY: modules
modules:
	. $(LINUXDIR)/.config; if [ "$$CONFIG_MODULES" = "y" ]; then \
		[ -d $(LINUXDIR)/modules ] || mkdir $(LINUXDIR)/modules; \
		$(MAKEARCH_KERNEL) -C $(LINUXDIR) modules; \
	fi

.PHONY: modules_install
modules_install:
	. $(LINUXDIR)/.config; if [ "$$CONFIG_MODULES" = "y" ]; then \
		[ -d $(ROMFSDIR)/lib/modules ] || mkdir -p $(ROMFSDIR)/lib/modules; \
		$(MAKEARCH_KERNEL) -C $(LINUXDIR) INSTALL_MOD_PATH=$(ROMFSDIR) DEPMOD="../user/busybox/examples/depmod.pl" modules_install; \
		rm -f $(ROMFSDIR)/lib/modules/*/build; \
		rm -f $(ROMFSDIR)/lib/modules/*/source; \
		find $(ROMFSDIR)/lib/modules -type f -name "*o" | xargs -r $(STRIP) -R .comment -R .note -g --strip-unneeded; \
	fi

linux_xconfig:
	KCONFIG_NOTIMESTAMP=1 $(MAKEARCH_KERNEL) -C $(LINUXDIR) xconfig
linux_menuconfig:
	KCONFIG_NOTIMESTAMP=1 $(MAKEARCH_KERNEL) -C $(LINUXDIR) menuconfig
linux_config:
	KCONFIG_NOTIMESTAMP=1 $(MAKEARCH_KERNEL) -C $(LINUXDIR) config
modules_xconfig:
	[ ! -d modules ] || $(MAKEARCH) -C modules xconfig
modules_menuconfig:
	[ ! -d modules ] || $(MAKEARCH) -C modules menuconfig
modules_config:
	[ ! -d modules ] || $(MAKEARCH) -C modules config
modules_clean:
	-[ ! -d modules ] || $(MAKEARCH) -C modules clean
config_xconfig:
	$(MAKEARCH) -C config xconfig
config_menuconfig:
	$(MAKEARCH) -C config menuconfig
config_config:
	$(MAKEARCH) -C config config
oldconfig_config:
	$(MAKEARCH) -C config oldconfig
oldconfig_modules:
	[ ! -d modules ] || $(MAKEARCH) -C modules oldconfig
oldconfig_linux:
	KCONFIG_NOTIMESTAMP=1 $(MAKEARCH_KERNEL) -C $(LINUXDIR) oldconfig

############################################################################
#
# normal make targets
#

.PHONY: romfs
#romfs: romfs.subdirs modules_install romfs.post
romfs: romfs_clean romfs.subdirs modules_install ufsdk_romfs romfs.post
#romfs: romfs_clean romfs.subdirs modules_install romfs.post

.PHONY: romfs.subdirs
romfs.subdirs:
	for dir in vendors $(DIRS) ; do [ ! -d $$dir ] || $(MAKEARCH) -C $$dir romfs || exit 1 ; done

.PHONY: romfs.post
romfs.post:
	$(MAKEARCH) -C vendors romfs.post
	-find $(ROMFSDIR)/. -name CVS | xargs -r rm -rf

.PHONY: image
image:
	[ -d $(IMAGEDIR) ] || mkdir $(IMAGEDIR)
	rm -rf $(IMAGEDIR)/*
	$(MAKEARCH) -C vendors image
	cp $(IMAGEDIR)/$(USER)_uImage $(TFTPDIR)
	./do_CRC.sh

.PHONY: release
release:
	make -C release release

romfs_clean:
	rm -rf $(ROMFSDIR)/* ;

ufsdk_romfs:
	if [ ! -e $(ROOTDIR)/user/ufsd_file/ufsd.ko ] ; \
	then \
		${MAKE} linux ufsd ; \
	fi
	cp user/ufsd_file/ufsd.ko $(ROMFSDIR)/lib/modules/;
#	cp user/ufsd_file/jnl.ko $(ROMFSDIR)/lib/modules/;
	cd user/ufsd_file/tools; chmod 777 *;
	cp user/ufsd_file/tools/chkexfat $(ROMFSDIR)/bin/;
	cp user/ufsd_file/tools/chkntfs $(ROMFSDIR)/bin/;
	mkdir -p $(ROMFSDIR)/lib/modules/2.6.36/kernel/drivers/net/wireless/rt2860v2_ap/ ;
	cp linux-2.6.36.x/drivers/net/wireless/rt2860v2_ap/rt2860v2_ap.ko $(ROMFSDIR)/lib/modules/2.6.36/kernel/drivers/net/wireless/rt2860v2_ap/ ;

tuxera_romfs:
	cp user/tuxera-file/exfat/kernel-module/texfat.ko $(ROMFSDIR)/lib/modules/;
	cp user/tuxera-file/ntfs/kernel-module/tntfs.ko $(ROMFSDIR)/lib/modules/;

libiconv-1.11_romfs:
	cd user/libiconv-1.11/; \
        $(MAKE) DESTDIR=$(ROMFSDIR) install; \
        rm -rf $(ROMFSDIR)/usr/local/bin ; \
        rm -rf $(ROMFSDIR)/usr/local/lib/libiconv.a ; \
        rm -rf $(ROMFSDIR)/usr/local/share ; 
	cd $(ROMFSDIR)/lib/; \
		rm -rf libiconv.so.2; \
        ln -s /usr/local/lib/preloadable_libiconv.so libiconv.so.2;
	cd $(ROMFSDIR)/usr/local/apache2/; \
        rm -rf logs ; \
        ln -s /tmp/logs logs;

httpd-2.2.9_romfs:
	cd $(HTTPDDIR); \
        $(MAKE) DESTDIR=$(ROMFSDIR) install; \
        rm -rf $(ROMFSDIR)/usr/local/apache2/manual;
	cp $(HTTPDDIR)backup/server.crt $(ROMFSDIR)/usr/local/apache2/conf/;
	cp $(HTTPDDIR)backup/server.key $(ROMFSDIR)/usr/local/apache2/conf/;


%_fullrelease:
	@echo "This target no longer works"
	@echo "Do a make -C release $@"
	exit 1
#
# fancy target that allows a vendor to have other top level
# make targets,  for example "make vendor_flash" will run the
# vendor_flash target in the vendors directory
#

vendor_%:
	$(MAKEARCH) -C vendors $@

.PHONY: linux
linux linux%_only:
	@if [ $(LINUXDIR) = linux-2.4.x -a ! -f $(LINUXDIR)/.depend ] ; then \
		echo "ERROR: you need to do a 'make dep' first" ; \
		exit 1 ; \
	fi
	$(MAKEARCH_KERNEL) -j$(HOST_NCPU) -C $(LINUXDIR) $(LINUXTARGET) || exit 1
	if [ -f $(LINUXDIR)/vmlinux ]; then \
		ln -f $(LINUXDIR)/vmlinux $(LINUXDIR)/linux ; \
	fi

.PHONY: sparse
sparse:
	$(MAKEARCH_KERNEL) -C $(LINUXDIR) C=1 $(LINUXTARGET) || exit 1

.PHONY: sparseall
sparseall:
	$(MAKEARCH_KERNEL) -C $(LINUXDIR) C=2 $(LINUXTARGET) || exit 1

.PHONY: subdirs
subdirs: lib uClibc++ linux modules
	for dir in $(DIRS) ; do [ ! -d $$dir ] || $(MAKEARCH) -C $$dir || exit 1 ; done

dep:
	@if [ ! -f $(LINUXDIR)/.config ] ; then \
		echo "ERROR: you need to do a 'make config' first" ; \
		exit 1 ; \
	fi
	
	@if [ $(LINUXDIR) = linux-2.6.21.x ] ; then \
	$(MAKEARCH_KERNEL) -C $(LINUXDIR) prepare ; \
	fi
	
	@if [ $(LINUXDIR) = linux-2.6.36MT.x ] ; then \
		$(MAKEARCH_KERNEL) -C $(LINUXDIR) prepare ; \
		rm -fr $(LINUXDIR)/include/asm; \
		rm -fr $(LINUXDIR)/include/linux/autoconf.h; \
		rm -fr $(LINUXDIR)/arch/mips/include/asm/rt2880; \
		ln -sf ../arch/mips/include/asm $(LINUXDIR)/include/asm; \
		ln -sf ../../include/generated/autoconf.h $(LINUXDIR)/include/linux/autoconf.h; \
		ln -sf ./mach-ralink $(LINUXDIR)/arch/mips/include/asm/rt2880; \
	fi
	
	@if [ $(LINUXDIR) = linux-2.6.36.x ] ; then \
		$(MAKEARCH_KERNEL) -C $(LINUXDIR) prepare ; \
		rm -fr $(LINUXDIR)/include/asm; \
		rm -fr $(LINUXDIR)/arch/mips/include/asm/rt2880; \
		rm -fr $(LINUXDIR)/include/linux/autoconf.h; \
		ln -sf ../arch/mips/include/asm $(LINUXDIR)/include/asm; \
		ln -sf ./mach-ralink $(LINUXDIR)/arch/mips/include/asm/rt2880; \
		ln -sf ../../include/generated/autoconf.h $(LINUXDIR)/include/linux/autoconf.h; \
	fi

	$(MAKEARCH_KERNEL) -C $(LINUXDIR) dep

# This one removes all executables from the tree and forces their relinking
.PHONY: relink
relink:
	find user prop vendors -type f -name '*.gdb' | sed 's/^\(.*\)\.gdb/\1 \1.gdb/' | xargs rm -f

clean: modules_clean
	for dir in $(LINUXDIR) $(DIRS); do [ ! -d $$dir ] || $(MAKEARCH) -C $$dir clean ; done
	rm -rf $(ROMFSDIR)/*
	rm -f $(IMAGEDIR)/*
	rm -f config.tk
	rm -f $(LINUXDIR)/linux
	rm -rf $(LINUXDIR)/net/ipsec/alg/libaes $(LINUXDIR)/net/ipsec/alg/perlasm
	rm -f $(LINUXDIR)/arch/mips/ramdisk/*.gz

real_clean mrproper: clean
	-$(MAKEARCH_KERNEL) -C $(LINUXDIR) mrproper
	-$(MAKEARCH) -C config clean
	-$(MAKEARCH) -C lib distclean
	-$(MAKEARCH) -C user/busybox mrproper
	rm -rf romfs config.in config.arch config.tk images
	rm -f modules/config.tk
	rm -rf .config .config.old .oldconfig autoconf.h

distclean: mrproper
	-$(MAKEARCH_KERNEL) -C $(LINUXDIR) distclean
	-rm -f user/tinylogin/applet_source_list user/tinylogin/config.h

%_only:
	@case "$(@)" in \
	*/*) d=`expr $(@) : '\([^/]*\)/.*'`; \
	     t=`expr $(@) : '[^/]*/\(.*\)'`; \
	     $(MAKEARCH) -C $$d $$t;; \
	*)   $(MAKEARCH) -C $(@:_only=);; \
	esac

%_clean:
	@case "$(@)" in \
	*/*) d=`expr $(@) : '\([^/]*\)/.*'`; \
	     t=`expr $(@) : '[^/]*/\(.*\)'`; \
	     $(MAKEARCH) -C $$d $$t;; \
	*)   $(MAKEARCH) -C $(@:_clean=) clean;; \
	esac

%_default:
	@if [ ! -f "vendors/$(@:_default=)/config.device" ]; then \
		echo "vendors/$(@:_default=)/config.device must exist first"; \
		exit 1; \
	 fi
	-make clean > /dev/null 2>&1
	cp vendors/$(@:_default=)/config.device .config
	chmod u+x config/setconfig
	yes "" | config/setconfig defaults
	config/setconfig final
	make dep
	make

config_error:
	@echo "*************************************************"
	@echo "You have not run make config."
	@echo "The build sequence for this source tree is:"
	@echo "1. 'make config' or 'make xconfig'"
	@echo "2. 'make dep'"
	@echo "3. 'make'"
	@echo "*************************************************"
	@exit 1

prune: ucfront
	@for i in `ls -d linux-* | grep -v $(LINUXDIR)`; do \
		rm -fr $$i; \
	done
#$(MAKE) -C lib prune
	$(MAKE) -C uClib prune
	$(MAKE) -C user prune
	$(MAKE) -C vendors prune

dist-prep:
	-find $(ROOTDIR) -name 'Makefile*.bin' | while read t; do \
		$(MAKEARCH) -C `dirname $$t` -f `basename $$t` $@; \
	 done

############################################################################
