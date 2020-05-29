
SSL_CFLAGS += -I$(ROOTDIR)/lib/libssl/include/

ifeq ($(MODE),1)
WWWDIR := $(ROOTDIR)/user/www_skype
else
WWWDIR := $(ROOTDIR)/user/www
endif

CONFIG_CROSS_COMPILE := mipsel-linux

CONFIGURE_CFG = \
        --host=$(CONFIG_CROSS_COMPILE)                              \
        --enable-maintainer-mode \
        --target="" \
        --enable-ipv6 \
        --with-pcre \
        --without-bzip2                                         \
        --without-zlib \
		--with-webdav-props \
		--with-webdav-locks \
		--enable-static=yes \
		XML_LIBS="-L$(ROOTDIR)/user/libxml2-2.9.1/romfs/lib" \
		XML_CFLAGS="-I$(ROOTDIR)/user/libxml2-2.9.1/" \
		SQLITE_LIBS="-L$(ROOTDIR)/lib/libsqlite3/.libs -lsqlite3" \
		SQLITE_CFLAGS="-I$(ROOTDIR)/lib/libsqlite3" \
		PCRE_LIB="-L$(ROOTDIR)/lib/pcre-8.01/.libs -lpcre" \
        PCRECONFIG=$(ROOTDIR)/lib/pcre-8.01/pcre-config

#CONFIGURE_CFG += \
	--without-bzip2 \
	--without-zlib \
	--with-openssl \
	--with-openssl-libs=$(ROOTDIR)/lib/libssl/ \
	--with-openssl-includes=$(ROOTDIR)/lib/libssl/include/

all: Makefile
	AM_MAKEFLAGS="-f Makefile" \
	$(MAKE) -f Makefile

wwwlink:
	-rm -f www
	@if [ ! -d $(WWWDIR) ] ; then \
		echo "ERROR: There is no www directory in $(WWWDIR)." ; \
		exit 1 ; \
	fi
	ln -sf $(WWWDIR) www

ifeq ($(CONFIG_LIB_JSON),y)
JSON_CFLAGS += -DLIGHTTPD_SUPPORT_JSON
JSON_LDFLAGS += -ljson
endif

#-L$(ROOTDIR)/lib/libshared -lshared

Makefile:
	chmod u+x ./configure; \
	CC=$(CROSS_COMPILE)gcc
	CFLAGS+="$(JSON_CFLAGS) -I$(ROOTDIR)/lib/ -I$(ROOTDIR)/lib/pcre-8.01/ -I$(ROOTDIR)/lib/libshared -DHAVE_PCRE_H=1 -DHAVE_LIBPCRE=1 -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -I$(ROOTDIR)/user/libuuid-1.0.2/romfs/include/ -I$(ROOTDIR)/user/libxml2-2.9.1 -I$(ROOTDIR)/user/libxml2-2.9.1/romfs/include/libxml2  -DHAVE_LIBXML_H=1 -DHAVE_LIBXML2=1 -I$(ROOTDIR)/lib/libsqlite3/" \
	LDFLAGS+="-L$(ROOTDIR)/user/nvram -lnvram -L$(ROOTDIR)/lib/lib $(JSON_LDFLAGS) -L$(ROOTDIR)/lib/lib -lpcre -L$(ROOTDIR)/user/libxml2-2.9.1/romfs/lib/ -lxml2 -L$(ROOTDIR)/lib/lib -lsqlite3 -lpthread -ldl -L$(ROOTDIR)/user/libuuid-1.0.2/romfs/lib -luuid" \
	./configure $(CONFIGURE_CFG)

#
	#CFLAGS+="$(SSL_CFLAGS) $(JSON_CFLAGS) -I$(ROOTDIR)/include -I$(ROOTDIR)/lib/libshared -I$(ROOTDIR)/lib/pcre-7.8/ -DHAVE_PCRE_H=1 -DHAVE_LIBPCRE=1" \
#	LDFLAGS+="$(SSL_LDFLAGS) $(JSON_LDFLAGS) -L$(ROOTDIR)/lib/ -L$(ROOTDIR)/lib/libshared -lshared" \
#	LIBS="$(SSL_LIBS)" \
#	./configure $(CONFIGURE_CFG)


clean:
	rm -rf $(TARGETDIR)
	rm -f www
	AM_MAKEFLAGS="-f Makefile" \
	$(MAKE) -f Makefile distclean

preinstall:
	rm -rf $(TARGETDIR)
	mkdir -p $(TARGETDIR)
	DESTDIR=$(TARGETDIR) \
	AM_MAKEFLAGS="-f Makefile" \
	$(MAKE) -f Makefile install

romfs:
	$(ROMFSINST) -p 755 src/lighttpd /usr/bin/lighttpd
	$(ROMFSINST) -p 755 src/lighttpd-angel /usr/bin/lighttpd-angel
	mkdir -p $(ROMFSDIR)/lighttpd_www
#	$(ROMFSINST) www /lighttpd_www
	mkdir -p $(ROMFSDIR)/usr/local/lib
#	$(ROMFSINST) -s /var/config.bin /lighttpd_www/config.bin
#	$(ROMFSINST) -s /var/config.xml /lighttpd_www/config.xml
#	$(ROMFSINST) -s /var/ram_log.txt /lighttpd_www/ram_log.txt
#	$(ROMFSINST) -s /var/flash_log.txt /lighttpd_www/flash_log.txt
#	$(ROMFSINST) -s /tmp/DetectedRogueAPList.txt /lighttpd_www/DetectedRogueAPList.txt
#	$(ROMFSINST) -s /tmp/KnownAPList.txt /lighttpd_www/KnownAPList.txt
#	$(ROMFSINST) -s /tmp/wap320.pem /lighttpd_www/wap320.crt
	#$(ROMFSINST) -s /mnt/disk1/video /lighttpd_www/video
	#$(ROMFSINST) -s / /lighttpd_www/recording
	#$(ROMFSINST) -p 755 lighttpd.conf /usr/lighttpd_old.conf
	$(ROMFSINST) -p 755 lighttpd.conf /usr/lighttpd.conf
	#$(ROMFSINST) -p 755 lighttpd.user /usr/lighttpd.user
	$(ROMFSINST) -p 755 src/.libs/mod_indexfile.so /usr/local/lib/mod_indexfile.so
	$(ROMFSINST) -p 755 src/.libs/mod_access.so /usr/local/lib/mod_access.so
	$(ROMFSINST) -p 755 src/.libs/mod_auth.so /usr/local/lib/mod_auth.so
	$(ROMFSINST) -p 755 src/.libs/mod_cgi.so /usr/local/lib/mod_cgi.so
	$(ROMFSINST) -p 755 src/.libs/mod_ssi.so /usr/local/lib/mod_ssi.so
	$(ROMFSINST) -p 755 src/.libs/mod_dirlisting.so /usr/local/lib/mod_dirlisting.so
	$(ROMFSINST) -p 755 src/.libs/mod_alias.so /usr/local/lib/mod_alias.so
	$(ROMFSINST) -p 755 src/.libs/mod_staticfile.so /usr/local/lib/mod_staticfile.so
	$(ROMFSINST) -p 755 src/.libs/mod_redirect.so /usr/local/lib/mod_redirect.so
	$(ROMFSINST) -p 755 src/.libs/mod_setenv.so /usr/local/lib/mod_setenv.so
	$(ROMFSINST) -p 755 src/.libs/mod_evasive.so /usr/local/lib/mod_evasive.so
	$(ROMFSINST) -p 755 server.pem /usr/server.pem
#	$(ROMFSINST) -p 755 src/.libs/mod_h264_streaming.so /usr/local/lib/mod_h264_streaming.so
	$(ROMFSINST) -p 755 src/.libs/mod_secdownload.so /usr/local/lib/mod_secdownload.so
	$(ROMFSINST) -p 755 src/.libs/mod_flv_streaming.so /usr/local/lib/mod_flv_streaming.so
	$(ROMFSINST) -p 755 src/.libs/mod_expire.so /usr/local/lib/mod_expire.so
	$(ROMFSINST) -p 755 src/.libs/mod_session.so /usr/local/lib/mod_session.so
	$(ROMFSINST) -p 755 src/.libs/mod_accesslog.so /usr/local/lib/mod_accesslog.so
	$(ROMFSINST) -p 755 src/.libs/mod_webdav.so /usr/local/lib/mod_webdav.so
