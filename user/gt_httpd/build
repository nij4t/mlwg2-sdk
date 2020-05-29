#!/bin/sh
APPS_NAME="Httpd"
if [ -e sysconfig.sh ]; then
        . sysconfig.sh
        . config.sh
        . model_config.sh
else
        echo "Application "$APPS_NAME" not configured"
        exit 0
fi

#if [ $BUILD_CLEAN -eq 1 ]; then
#        make clean
#        exit 0
#fi

display_info "----------------------------------------------------------------------"
display_info "-------------------- build $APPS_NAME    -----------------------------"
display_info "----------------------------------------------------------------------"

IFX_CFLAGS="${IFX_CFLAGS} ${PLATFORM_CFLAGS}"

make AR=${IFX_AR} AS=${IFX_AS} LD=${IFX_LD} NM=${IFX_NM} CC=${IFX_CC} BUILDCC=${IFX_HOSTCC} GCC=${IFX_CC} CXX=${IFX_CXX} CPP=${IFX_CPP} RANLIB=${IFX_RANLIB} STRIP=${IFX_STRIP} IFX_CFLAGS="${IFX_CFLAGS}" IFX_LDFLAGS="${IFX_LDFLAGS}" all

cp -f httpd ${BUILD_ROOTFS_DIR}usr/sbin/
cp -f tail ${BUILD_ROOTFS_DIR}usr/sbin/
cp -f server.pem ${BUILD_ROOTFS_DIR}usr/sbin/
#cp -f tail ${BUILD_ROOTFS_DIR}usr/sbin
#cp -f dev_taber ${BUILD_ROOTFS_DIR}usr/sbin
#cp -f loginTimer ${BUILD_ROOTFS_DIR}usr/sbin
rm -rf ${BUILD_ROOTFS_DIR}usr/sbin/www
cp -R www ${BUILD_ROOTFS_DIR}usr/sbin/
${IFX_STRIP} ${BUILD_ROOTFS_DIR}usr/sbin/httpd
${IFX_STRIP} ${BUILD_ROOTFS_DIR}usr/sbin/tail
