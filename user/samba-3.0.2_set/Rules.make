CFLAGS				+=  -s -O2 -Wall

CFLAGS				+=	$(PATHNVRAM)
CFLAGS				+=	$(PATHUSER)
LDFLAGS				+= 	$(LIBNVRAM)
#LDFLAGS				+=  $(LIBUTILILTY)

OBJS					= sambaset.o shutils.o gmtk_process.o
EXEC					= sambaset


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
