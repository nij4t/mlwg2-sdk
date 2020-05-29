# Uncomment this on Solaris.
#LIBS = -lsocket -lnsl

NAME = dnsmasq
CFLAGS += -O2 -D__uClinux__ -DNO_GETTEXT
CFLAGS += -I../../nvram/ -DFORWARDALL -I../../gt_utilities/
LDFLAGS += -L../../nvram/ -lnvram 
#LDFLAGS += -L../../nvram/ -lnvram -L../../gt_utilities/ -lgtutils

OBJS = cache.o rfc1035.o util.o option.o forward.o isc.o \
       network.o dnsmasq.o dhcp.o lease.o rfc2131.o netlink.o

all: $(NAME)

$(NAME): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) ../../gt_utilities/get_info.o $(EXTRALIBS) $(LDLIBS)
	$(STRIP) --remove-section=.note --remove-section=.comment $@

romfs:
	$(ROMFSINST) /bin/$(NAME)

clean:
	rm -f *~ *.mo *~ *.pot
	rm -f *.o dnsmasq.a dnsmasq core */core

