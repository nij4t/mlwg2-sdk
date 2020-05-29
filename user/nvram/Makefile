#
# NVRAM Makefile
#
#
#
# NVRAM Makefile
#

TOOL_CHAIN_PATH=/opt/buildroot-gcc342/bin/
CC=$(TOOL_CHAIN_PATH)mipsel-linux-gcc
LD=$(TOOL_CHAIN_PATH)mipsel-linux-ld
STRIP=$(TOOL_CHAIN_PATH)mipsel-linux-strip

CFLAGS += -I../../linux-2.6.36.x/include/ -DMT7620

all: libnvram.so nvram nvram_recover

clean:
	rm -f *.a *.so *.bak nvram nvram_recover

libnvram.so: nvram_lib.o nvram.o sys_default.o gpio.o
	$(LD) -shared -o $@ $^

nvram: main.o
	$(CC) $(CFLAGS) -o $@ $< -L. -lnvram

nvram_recover:
	$(CC) -o $@ $@.o $(CFLAGS)
	$(STRIP) $@
	

romfs:
	$(ROMFSINST) /usr/sbin/nvram
	$(ROMFSINST) /lib/libnvram.so
	$(ROMFSINST) /usr/sbin/nvram_recover
