BUILD_ROOTFS_DIR = ../../romfs/

all: matrixssl


matrixssl:
	make -C ./src all

clean:
	make -C ./src clean
	
romfs:
	cp -af ./src/libmatrixssl.so $(BUILD_ROOTFS_DIR)lib
	cp -af ./certificate-Gemtek/privkeySrv.pem $(BUILD_ROOTFS_DIR)etc/privkeySrv.pem
	cp -af ./certificate-Gemtek/certSrv.pem $(BUILD_ROOTFS_DIR)etc/certSrv.pem
