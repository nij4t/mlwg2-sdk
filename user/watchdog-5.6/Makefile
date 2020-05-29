BUILD_ROOTFS_DIR=../../romfs/

all:
	$(MAKE) -C src

romfs:
	$(MAKE) -C src romfs
	$(ROMFSINST)  /etc_ro/watchdog.conf
	#$(ROMFSINST)  /sbin/wdg.sh
	ln -sf /tmp/watchdog $(BUILD_ROOTFS_DIR)/dev/watchdog

clean:
	$(MAKE) -C src clean
	
