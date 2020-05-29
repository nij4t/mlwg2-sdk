#
# LOG API Makefile
#

TOOL_CHAIN_PATH=/opt/buildroot-gcc342/bin/
CC=$(TOOL_CHAIN_PATH)mipsel-linux-gcc
LD=$(TOOL_CHAIN_PATH)mipsel-linux-ld
STRIP=$(TOOL_CHAIN_PATH)mipsel-linux-strip

CFLAGS	+= -I./ -Wall
CFLAGS	+= -s -O2
LDFLAGS += -L.

all: libgtlog.so

clean:
	rm -f *.a *.so

libgtlog.so: log.o
	$(LD) -shared -o $@ $^

romfs:
	$(ROMFSINST) /lib/libgtlog.so

