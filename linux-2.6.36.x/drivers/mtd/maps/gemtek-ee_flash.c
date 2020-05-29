#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>

#define EEPROM_SIZE	0x200
#define EE_FLASH_READ   0
#define EE_FLASH_WRITE  1

unsigned char buf[EEPROM_SIZE];

extern int ra_mtd_write_nm(char *name, loff_t to, size_t len, const u_char *buf);
extern int ra_mtd_read_nm(char *name, loff_t from, size_t len, u_char *buf);

#if 0
static ssize_t drv_read(struct file *filp, char *buf, size_t count, loff_t *ppos)
{
   printk("device read\n");
   return count;
}

static ssize_t drv_write(struct file *filp, const char *buf, size_t count, loff_t *ppos)
{
   printk("device write\n");
   return count;
}
#endif

static int drv_open(struct inode *inode, struct file *filp)
{
   //printk("device open\n");
   return 0;
}

int drv_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
	//printk("device ioctl! cmd = %u\n",cmd);
	switch(cmd)
	{
		case EE_FLASH_READ:
			ra_mtd_read_nm("Factory", 0, EEPROM_SIZE, buf);
			copy_to_user((unsigned char*)arg, buf, EEPROM_SIZE);
			break;
		case EE_FLASH_WRITE:
			copy_from_user(buf, (unsigned char*)arg, EEPROM_SIZE);
			ra_mtd_write_nm("Factory", 0, EEPROM_SIZE, buf);
			break;
		default:
			break;
	}
	return 0;
}

static int drv_release(struct inode *inode, struct file *filp)
{
   //printk("device close\n");
   return 0;
}

struct file_operations drv_fops =
{
#if 0
   read:           drv_read,
   write:          drv_write,
#endif
   unlocked_ioctl:          drv_ioctl,
   open:           drv_open,
   release:        drv_release,
};

#define MAJOR_NUM         200 
#define MODULE_NAME                "flash0"
static int ee_flash_init(void) 
{
   if (register_chrdev(MAJOR_NUM, "flash0", &drv_fops) < 0) 
   {
      printk("<1>%s: can't get major %d\n", MODULE_NAME, MAJOR_NUM);
      return (-EBUSY);
   }
   printk("<1>%s: started\n", MODULE_NAME);
   return 0;
}

static void ee_flash_exit(void) 
{
   unregister_chrdev(MAJOR_NUM, "flash0");
   printk("<1>%s: removed\n", MODULE_NAME);
}
module_init(ee_flash_init);
module_exit(ee_flash_exit);

