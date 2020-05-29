

all:
	CFLAGS="" LDFLAGS="" ./configure --host=mips --target=mips CC=$(CROSS_COMPILE)gcc --prefix=$(ROOTDIR)/user/libuuid-1.0.2/romfs
	AM_MAKEFLAGS="-f Makefile" \
	$(MAKE) -f Makefile
	AM_MAKEFLAGS="-f Makefile" \
	$(MAKE) install -f Makefile

romfs:

	
