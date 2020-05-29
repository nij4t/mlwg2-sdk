NVDIR = $(ROOTDIR)/lib/libnvram
CFLAGS += -I$(NVDIR) 
LDFLAGS  += -L$(ROOTDIR)/lib/lib
LDFLAGS += -lnvram
EXEC = ated
SRCS = ated.c


#EXTRA_CFLAGS += -DDBG

all: $(EXEC)

$(EXEC): $(ROOTDIR)/$(LINUXDIR)/include/linux/autoconf.h
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) $(LDFLAGS) -o $@ $@.o $(LDLIBS)

romfs:
	$(ROMFSINST) /bin/$(EXEC)

clean:
	-rm -f $(EXEC)

