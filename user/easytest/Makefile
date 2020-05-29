#
# easytest (Product Test Program) Makefile
#
#CC = mips-linux-uclibc-gcc
CFLAGS	+= -I. -I$(SRCBASE)/include -Wall
CFLAGS	+= -Os #-s
CFLAGS	+= -I../nvram/include
LDFLAGS	+= -L../nvram -lnvram -L../gt_utilities -lled

vpath %.c $(TOP)/shared

OBJS	= easytest.o

all: clean easytest

easytest: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

clean:
	rm -rf easytest

romfs:
	cp easytest $(ROMFSDIR)/bin/easytest
	$(STRIP) $(ROMFSDIR)/bin/easytest

install:
	install -D easytest $(INSTALLDIR)/bin/easytest
	$(STRIP) $(INSTALLDIR)/bin/easytest
