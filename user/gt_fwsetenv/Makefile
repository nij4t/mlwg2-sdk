
TOOL_CHAIN_PATH=/opt/buildroot-gcc342/bin/
CC=$(TOOL_CHAIN_PATH)mipsel-linux-gcc
LD=$(TOOL_CHAIN_PATH)mipsel-linux-ld
STRIP=$(TOOL_CHAIN_PATH)mipsel-linux-strip

LD_FLAG += -L../nvram -lnvram


OBJS := gt_fwsetenv.o

all: gt_fwsetenv

gt_fwsetenv: $(OBJS)
	$(CC) -o $@ $^ $(LD_FLAG)

clean:
	rm -f *.bak
	rm -f gt_fwsetenv

romfs:
	$(ROMFSINST) /usr/sbin/gt_fwsetenv
	$(STRIP) /usr/sbin/gt_fwsetenv
