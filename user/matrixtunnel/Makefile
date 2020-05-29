#
# Matrixtunnel control scripts Makefile
#
# $Id: Makefile,v 1.1.1.1 2004/04/28 07:18:25 tom Exp $
#

CFLAGS	+= -s -O2 -Wall

CFLAGS	+= -I../nvram
#CFLAGS	+= $(PATHUSER)
#CFLAGS  += $(PATHGEM)
CFLAGS  += -I../matrixssl/  -DLINUX

LDFLAGS	+= -L../nvram -lnvram -L../matrixssl/src -lmatrixssl

OBJS		+= matrixtunnel.c  sslSocket.c
BUILD_ROOTFS_DIR = ../../romfs/

all: matrixtunnel

clean:
	rm -f *.o *~ matrixtunnel

install: all
	install -d $(INSTALLDIR)/bin
	install matrixtunnel $(INSTALLDIR)/bin
	
matrixtunnel: $(OBJS) 
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS) 
	$(STRIP) matrixtunnel

#$(OBJS): %.o: %.c Makefile
#	 $(CC) $(CFLAGS) -c $< -o $@

romfs:
	cp -af matrixtunnel $(BUILD_ROOTFS_DIR)bin
