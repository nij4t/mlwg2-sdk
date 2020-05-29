CFLAGS				+= -s -O2 -Wall

CFLAGS				+= $(PATHNVRAM)
CFLAGS				+= $(PATHUSER)
LDFLAGS				+= $(LIBNVRAM)
LDFLAGS += -L../nvram -lnvram -L../gt_utilities -lgtutils -L../gt_log -lgtlog -lpthread -lm

OBJS					= DMS_set.o
EXEC					= DMS_set


#
#	Keeps following rule, DONOT MODIFY
#
EXEC_CLEAN		= objclean
DEP						= depend

all: $(EXEC)

objclean:
	rm -f *.o *~ $(EXEC) .depend

depend:
	$(MKDEPTOOL)/mkdep $(CFLAGS) -- *.c > .depend 
