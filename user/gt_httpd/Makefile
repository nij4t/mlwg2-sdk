#
# HTTPD Makefile
#
TOOL_CHAIN_PATH=/opt/buildroot-gcc342/bin/
CC=$(TOOL_CHAIN_PATH)mipsel-linux-gcc
LD=$(TOOL_CHAIN_PATH)mipsel-linux-ld
STRIP=$(TOOL_CHAIN_PATH)mipsel-linux-strip
CFLAGS  += -Wall -s -O2   
CFLAGS	+= -I. -I../../../linux-2.6.18/include -I../nvram -DINFINEON
CFLAGS	+= -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
#CFLAGS	+= -I. -I../../../linux-2.6.18/include -I../nvram
LDFLAGS += -L../nvram -lnvram -L../gt_utilities -lgtutils -L../gt_log -lgtlog -lpthread -lm
BUILD_ROOTFS_DIR=../../romfs/
#CFLAGS  +=  -DTFTP_LOAD

#for https, use /tmp/www/server.pem
#CFLAGS  += -I../openssl-0.9.8e/include -DSUPPORT_HTTPS -DTMP_PEM
#LDFLAGS += -L../openssl-0.9.8e -lssl

#common:
OBJS = httpd.o crc.o common.o 
#customer:
OBJS += setup.o internet.o wireless.o security.o lanconfig.o adminconfig.o diagnostic.o vsp.o tmpram.o

all: httpd

clean:
	rm -f httpd

httpd: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

romfs:
	$(ROMFSINST) /usr/sbin/httpd
	cp -af www/ www_tmp
	find ./www_tmp -name ".svn" | xargs rm -rf
#	tar -jcf html.tar.bz2 www
#	cp -af html.tar.bz2 $(BUILD_ROOTFS_DIR)usr/sbin/html.tar.bz2
#	cp -af language/string_ENG.tar.bz2 $(BUILD_ROOTFS_DIR)usr/sbin/string_ENG.tar.bz2
	cp -af language/ENG/ ENG_tmp
	find ./ENG_tmp -name ".svn" | xargs rm -rf	
	cp -af language/ZN_TW/ ZN_TW_tmp
	find ./ZN_TW -name ".svn" | xargs rm -rf	
	cp -af language/JP/ JP_tmp
	find ./JP -name ".svn" | xargs rm -rf	
	
	cp -af language/FRCA/ FRCA_tmp
	find ./FRCA -name ".svn" | xargs rm -rf	
	
	cp -af language/ES/ ES_tmp
	find ./ES -name ".svn" | xargs rm -rf	
	
##	cp -af language/string_ZN_TW.tar.bz2 $(BUILD_ROOTFS_DIR)usr/sbin/string_ZN_TW.tar.bz2
##	cp -af language/string_ZN_CH.tar.bz2 $(BUILD_ROOTFS_DIR)usr/sbin/string_ZN_CH.tar.bz2
##	cp -af language/string_KO.tar.bz2 $(BUILD_ROOTFS_DIR)usr/sbin/string_KO.tar.bz2
#	cp -af language/string_DE.tar.bz2 $(BUILD_ROOTFS_DIR)usr/sbin/string_DE.tar.bz2
#	cp -af language/string_FR.tar.bz2 $(BUILD_ROOTFS_DIR)usr/sbin/string_FR.tar.bz2
#	cp -af language/string_IT.tar.bz2 $(BUILD_ROOTFS_DIR)usr/sbin/string_IT.tar.bz2
#	cp -af language/string_ES.tar.bz2 $(BUILD_ROOTFS_DIR)usr/sbin/string_ES.tar.bz2
##	cp -af language/string_THAI_data.tar.bz2 $(BUILD_ROOTFS_DIR)usr/sbin/string_THAI_data.tar.bz2
##	cp -af language/string_THAI_help.tar.bz2 $(BUILD_ROOTFS_DIR)usr/sbin/string_THAI_help.tar.bz2
	rm -rf $(BUILD_ROOTFS_DIR)/www
	rm -rf $(BUILD_ROOTFS_DIR)/usr/sbin/ENG
	rm -rf $(BUILD_ROOTFS_DIR)/usr/sbin/ZN_TW
	rm -rf $(BUILD_ROOTFS_DIR)/usr/sbin/JP
	
	rm -rf $(BUILD_ROOTFS_DIR)/usr/sbin/FRCA
	rm -rf $(BUILD_ROOTFS_DIR)/usr/sbin/ES
	
	mv www_tmp/ $(BUILD_ROOTFS_DIR)/www
	mv ENG_tmp/ $(BUILD_ROOTFS_DIR)/usr/sbin/ENG
	mv ZN_TW_tmp/ $(BUILD_ROOTFS_DIR)/usr/sbin/ZN_TW
	mv JP_tmp/ $(BUILD_ROOTFS_DIR)/usr/sbin/JP
	
	mv FRCA_tmp/ $(BUILD_ROOTFS_DIR)/usr/sbin/FRCA
	mv ES_tmp/ $(BUILD_ROOTFS_DIR)/usr/sbin/ES
	
#install: httpd
#	install -D httpd $(INSTALL_EXEC)/httpd
#	$(STRIP) $(INSTALL_EXEC)/httpd
#	install -D server.pem $(INSTALL_EXEC)/server.pem
#	rm -rf ../../../buildroot/build_mips_nofpu/root/www
#	cp -af ./www $(INSTALL_EXEC)/../../
#	rm -rf ../../../buildroot/build_mips_nofpu/root/www/Thumbs.db
