

all:
	./configure --host=mips --target=mips CC=$(CROSS_COMPILE)gcc --prefix=$(ROOTDIR)/user/libxml2-2.9.1/romfs  --without-zlib --without-python --without-iconv --enable-static=no --enable-shared=yes
	AM_MAKEFLAGS="-f Makefile" \
	$(MAKE) -f Makefile
	AM_MAKEFLAGS="-f Makefile" \
	$(MAKE) install -f Makefile

romfs:

	
