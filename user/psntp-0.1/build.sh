#!/bin/sh
APPS_NAME="psntpdate"
if [ -e sysconfig.sh ]; then
	. sysconfig.sh
	. config.sh
	. model_config.sh
else
        echo "Application "$APPS_NAME" not configured"
        exit 0
fi                                                                                                                                       

display_info "----------------------------------------------------------------------"
display_info "-----------------------  build $APPS_NAME		 -------------------"
display_info "----------------------------------------------------------------------"

parse_args $@

if [ "$1" = "config_only" ] ;then
#       aclocal
#       autoheader
#       autoconf
#       automake --foreign --add-missing
#       AR=${IFX_AR} AS=${IFX_AS} LD=${IFX_LD} NM=${IFX_NM} CC=${IFX_CC} BUILDCC=${IFX_HOSTCC} CXX=${IFX_CXX} RANLIB=${IFX_RANLIB} OBJCOPY=${IFX_OBJCOPY} OBJDUMP=${IFX_OBJDUMP} TARGET=${TARGET} HOST=${HOST} BUILD=${BUILD} ./configure --target=${TARGET} --host=${HOST} --build=${BUILD} --prefix=/usr --with-linux=${KERNEL_SOURCE_DIR}include
#       ifx_error_check $?
#       echo -n > .config_ok
	exit 0
fi

if [ $BUILD_CLEAN -eq 1 ]; then
#	make distclean
#	[ ! $BUILD_CONFIGURE -eq 1 ] && exit 0
	make clean
	exit 0
fi

make AR=${IFX_AR} AS=${IFX_AS} LD=${IFX_LD} NM=${IFX_NM} CC=${IFX_CC} STRIP=${IFX_STRIP} BUILDCC=${IFX_HOSTCC} GCC=${IFX_CC} CXX=${IFX_CXX} CPP=${IFX_CPP} RANLIB=${IFX_RANLIB} IFX_CFLAGS="${IFX_CFLAGS}" IFX_LDFLAGS="${IFX_LDFLAGS}" KERNEL_DIR=${KERNEL_SOURCE_DIR} NO_IPV6=1 PREFIX=/usr NO_SHARED_LIBS=1 BUILD_2MB_PACKAGE=0

#make install DESTDIR=${BUILD_ROOTFS_DIR} PREFIX=/usr NO_IPV6=1 AR=${IFX_AR} AS=${IFX_AS} LD=${IFX_LD} NM=${IFX_NM} CC=${IFX_CC} BUILDCC=${IFX_HOSTCC} GCC=${IFX_CC} CXX=${IFX_CXX} CPP=${IFX_CPP} RANLIB=${IFX_RANLIB} IFX_CFLAGS="${IFX_CFLAGS}" IFX_LDFLAGS="${IFX_LDFLAGS}" KERNEL_DIR=${KERNEL_SOURCE_DIR} NO_SHARED_LIBS=1
#ifx_error_check $? 

install -D psntpdate ${BUILD_ROOTFS_DIR}/usr/sbin/
cp -f psntpdate ${BUILD_ROOTFS_DIR}/usr/sbin/
$IFX_STRIP ${BUILD_ROOTFS_DIR}/usr/sbin/psntpdate
