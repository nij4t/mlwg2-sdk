EXE = widrive_aloha
OBJS = widrive_aloha.o

#stdlibc++ known issue: 
#If your program  needs libstdc++, please turn off code optimization
CFLAGS += -O0
LDFLAGS += -L../nvram -lnvram

all: $(OBJS)
	$(CXX) -o $(EXE) $(OBJS) $(LDFLAGS)

.cpp.o:
	$(CXX) $(CFLAGS) -c $< 

romfs:
	$(ROMFSINST) /bin/$(EXE)

clean:
	@rm -rf $(EXE)

