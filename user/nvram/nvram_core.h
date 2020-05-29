#ifndef _NVRAM_CORE_H_
#define _NVRAM_CORE_H_

#include <stdio.h> //for stderr
#include "nvram_lib.h"
/*dev nodes*/
#define KERNEL_DEV_NODE 	"/dev/mtd3"
#define ROOT_DEV_NODE 		"/dev/mtd4"
#define KERNEL_2_DEV_NODE 	"/dev/mtd4"
#define ROOT_2_DEV_NODE 	"/dev/mtd5"
#ifdef RT3052
#define RT3052_WLAN_CONFIG	"/dev/mtd2"
#endif

#define CRAMFS_MAGIC 	0x28cd3d45	/* firmware upgrade magic number for discovering the file system block */
//#define SQUASHFS_MAGIC	0x71736873
#define SQUASHFS_MAGIC	0x73717368

#define FILE_SYSTEM_MAGIC SQUASHFS_MAGIC
//#define FILE_SYSTEM_MAGIC CRAMFS_MAGIC

/*Files store data*/
#define FILE_CONF "/etc/config/conf.dat"
#define FILE_MAC "/etc/config/mac.dat"
#define FILE_MAC_BAK "/etc/config/mac.bak"
#define FILE_TR069 "/etc/config/tr069.dat"
#define TR_069_ENTRY_NUM 22
#define MAC_ENTRY_NUM 10

/* Cash 2006-02-21 for RAM based conf-file	*/
#define RAM_CONF "/tmp/ramconf.dat"
#define RC_CONF "/tmp/rc.conf"
#define DHCPD_CONF "/etc/udhcpd.conf"

typedef int mtd_fd_t;
/*Structures*/
/***name-value pair for sys_default***/
struct nvram_tuple {
	char *name;
	char *value;
	struct nvram_tuple *next;
};

/*File format*/
/***Configuration File***/
/*******************
  Format:
  name1=value1'\0'name2=value2'\0'....CONF'\0'
 *******************/
#define FILE_CONF_END "CONF"

/***MAC File***/
/*******************
  Format:
  MAC_LANMAC_WANMAC'\0'
 *******************/
#define FILE_MAC_END "MAC"
#define FILE_TR069_END "TR069"

/*#define MAC_NO_SIZE 1
#define MAC_DATA_OFFSET (MAC_NO_SIZE)*/
 
/*MACROs*/
#define cprintf(fmt, args...) do { \
		fprintf(stderr, fmt, ## args); \
} while (0)
/*error reporter*/
#define eprintf(fmt, args...) cprintf("%s: " fmt, __FUNCTION__, ## args)
#ifdef DEBUG
#define dprintf(fmt, args...) cprintf("%s: " fmt, __FUNCTION__, ## args)
#else
#define dprintf(fmt, args...)
#endif

#define MIN(x,y) ((x<y)? x:y)
#define MAX(x,y) ((x>y)? x:y)

//#define BLOCK_SIZE	0x20000 	//128K for each firmware writing unit
#define BLOCK_SIZE	0x10000 	//64K

#endif /* _NVRAM_CORE_H_ */
