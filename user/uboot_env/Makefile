#include ../../config.mk

ifeq ($(RTCHIP),RT3883)
CFLAGS		+= -O1 -Wall -I$(ROOTDIR)/include -I. $(PATHUSER) -DRTCHIP
else
CFLAGS          += -O1 -Wall -I$(ROOTDIR)/include -I. $(PATHUSER)
endif

OBJS		= main.o mtd.o crc32.o

#CFLAGS		+= $(PATHNVRAM)
CFLAGS	+= -I$(ROOTDIR)/$(LINUXDIR)/drivers/char -I$(ROOTDIR)/$(LINUXDIR)/include
LDFLAGS         += -L../nvram -lnvram


all: clean uboot_env

uboot_env:
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)
	
clean:
	rm -f uboot_env *.bak
	
romfs:
	cp uboot_env $(ROMFSDIR)/bin/uboot_env
