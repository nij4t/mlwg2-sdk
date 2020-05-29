EXE = server
OBJS = server.o gemtek_wep_passphrase.o

#stdlibc++ known issue: 
#If your program  needs libstdc++, please turn off code optimization

BUILD_ROOTFS_DIR=../../romfs/

CFLAGS  += -I../openssl-0.9.8e/include
LDFLAGS += -L../openssl-0.9.8e -lssl -lcrypto
CFLAGS += -O0
CFLAGS  += -I. -I../shared
LDFLAGS += -L../nvram -lnvram -L../gt_utilities -lgtutils -L../gt_log -lgtlog -lpthread -lm

all: $(OBJS) ssl_test.o ssl_server.o ssl_client.o
	$(CXX) -o $(EXE) $(OBJS) $(LDFLAGS)
	$(CXX) -o ssl_test ssl_test.o $(LDFLAGS) -Wall -lssl -lcrypto
	$(CXX) -o ssl_server ssl_server.o $(LDFLAGS) -Wall -lssl -lcrypto
	$(CXX) -o ssl_client ssl_client.o $(LDFLAGS) -Wall -lssl -lcrypto

.cpp.o:
	$(CXX) $(CFLAGS) -c $< 

romfs:
	$(ROMFSINST) /bin/$(EXE)
#	$(ROMFSINST) /bin/ssl_test
	$(ROMFSINST) /bin/ssl_server
#	$(ROMFSINST) /bin/ssl_client

clean:
	@rm -rf $(EXE)

test:
	mipsel-linux-gcc server.c -o ~/server
.PHONY: client
client:
	./client  192.168.100.250 61908
