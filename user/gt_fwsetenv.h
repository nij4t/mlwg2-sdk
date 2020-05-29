#ifndef _GT_FWSETENV_H_
#define _GT_FWSETENV_H_

#include <stdio.h>
#include <fcntl.h>

#define		MTDSIZE		65536 				// 0x10000 = 64K byte
#define		MTDPATH		"/dev/mtd1"		// mtd partition for uboot environment
#define		CHKLENGTH		1024				// only check first 1024 bytes for saving time

#endif /* _GT_FWSETENV_H_ */
