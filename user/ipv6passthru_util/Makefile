INSTALL_EXEC := ../../romfs/usr/sbin/

CFLAGS  += -I. -Wall -I../nvram

LDFLAGS += -L../nvram -lnvram
LDFLAGS += -L.
TOOL_CHAIN_PATH=/opt/buildroot-gcc342/bin/
CC=$(TOOL_CHAIN_PATH)mipsel-linux-gcc
LD=$(TOOL_CHAIN_PATH)mipsel-linux-ld
STRIP=$(TOOL_CHAIN_PATH)mipsel-linux-strip

OBJS := ipv6passthru_util.o

all: ipv6passthru_util

clean:
	rm -f *~ ipv6passthru_util
	rm -f $(INSTALL_EXEC)/ipv6passthru_util

ipv6passthru_util: $(OBJS)
	$(CC) $(CFLAG) -o $@ $^ $(LDFLAGS)
	$(STRIP) ipv6passthru_util
romfs:
	$(ROMFSINST)  /usr/sbin/ipv6passthru_util
	


