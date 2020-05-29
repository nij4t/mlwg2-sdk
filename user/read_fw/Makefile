#
# EasyConf control scripts Makefile
#
# Copyright (C) 2002 Gemtek
#
# $Id: Makefile,v 1.1.1.1 2004/04/28 07:18:25 tom Exp $
#

CFLAGS	+= -s -O2 -Wall -I./include

#CROSS_COMPILE=mips-linux-
#CC=mips-linux-gcc

TOOL_CHAIN_PATH=/opt/buildroot-gcc342/bin/
CC=$(TOOL_CHAIN_PATH)mipsel-linux-gcc
LD=$(TOOL_CHAIN_PATH)mipsel-linux-ld
STRIP=$(TOOL_CHAIN_PATH)mipsel-linux-strip

OBJS		+= read_fw.o

all: read_fw

clean:
	rm -f *~ read_fw *.bak

install: all
	install -d $(INSTALLDIR)/bin
	install read_fw $(INSTALLDIR)/bin
	
read_fw: $(OBJS) 
	$(CC) -o $@ $(OBJS) $(LDFLAGS) 

#$(OBJS): %.o: %.c Makefile
#	 $(CC) $(CFLAGS) -c $< -o $@
	 
romfs:
	$(ROMFSINST) /usr/sbin/read_fw
