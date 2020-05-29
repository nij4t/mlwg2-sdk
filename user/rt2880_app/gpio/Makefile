EXEC = gpio
CFLAGS += -I$(ROOTDIR)/$(LINUXDIR)/drivers/char

all: $(EXEC)

$(EXEC):
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $@.o $(LDLIBS)

romfs:
	$(ROMFSINST) /bin/$(EXEC)

clean:
	-rm -f $(EXEC) *.elf *.gdb

