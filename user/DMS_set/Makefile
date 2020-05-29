#
# Copyright (C) 2008 Gemtek Technology Co., Ltd.
#
#	General rule
#		The following rules should be able to support most case		
#
ifeq (Rules.make,$(wildcard Rules.make))
include Rules.make
ifeq (.depend,$(wildcard .depend))
include .depend
endif
endif

#
#	INSTALLBIN MUST be the same as EXEC
#
INSTALLBIN	= DMS_set


all: $(EXEC)

dep: $(DEP)

clean: $(EXEC_CLEAN)

install: all
	install -d $(INSTALLDIR)/bin
	install $(INSTALLBIN) $(INSTALLDIR)/bin


ifeq ($(GMTK_DEVFLAG_NFS),Y)

romfs:
	mkdir -p $(NFSROOTDIR)/bin
	cp $(INSTALLBIN) $(NFSROOTDIR)/bin/
	
else

romfs:
	mkdir -p $(ROMFSDIR)/bin
	cp $(INSTALLBIN) $(ROMFSDIR)/bin

endif
	
$(EXEC): $(OBJS) 
	$(CC) -o $@ $(OBJS) $(LDFLAGS) 
	$(STRIP) $(EXEC)

#$(OBJS): %.o: %.c Makefile
#	 $(CC) $(CFLAGS) -c $< -o $@
	 
gpl:
	rm -f Rules.make *.c *.h *.o .depend


