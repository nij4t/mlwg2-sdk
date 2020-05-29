#ifndef _NVRAM_LIB_H_
#define _NVRAM_LIB_H_

/*This is the only file you need to include!*/

#define NVRAM_MAC_LEN 		6
#define NVRAM_MAC_LAN 		0
#define NVRAM_MAC_WAN 	1
#define MAC_LAN_DEF 		0x22
#define MAC_WAN_DEF 		0x33
#define NVRAM_NAME_MAX		64+1  	//32 characters for max length of variable name
#define NVRAM_VAL_MAX		400+1  	//256 characters for max length of variable name
#define RAM_SETSIZE 		1600
#define NVRAM_PAIR_MAXLEN 	(NVRAM_NAME_MAX+NVRAM_VAL_MAX)
#define NVRAM_MAX_TUPLE		1600

#define CONF_TMP_FILE   "/tmp/conf.tmp" /* Save configurations to temp file */
#define CONF_RC4_FILE   "/tmp/conf.rc4" /* Save configurations to temp file */
#define DEFAULT_RC4_KEY "AirStation"

/*nvram_lib.c*/
typedef struct data_info {
	char name[NVRAM_NAME_MAX];
	char value[NVRAM_VAL_MAX];
} DATA_INFO;

/***name-value pair for share_memory***/
struct nvram_shm_tuple {
        char name[NVRAM_NAME_MAX];
        char value[NVRAM_VAL_MAX];
        unsigned int hashNum;
};
extern int nvram_init(void);
extern void nvram_rc(void);
extern int nvram_clean(void);
extern char *nvram_get(const char *name);
extern int nvram_set(const char *name, const char *value);
extern int nvram_unset(const char *name);
extern int nvram_show(void);
extern int nvram_upgrade(const char *image, unsigned int size);
extern int nvram_write_wlan_config(const char *image, unsigned int size);
extern int nvram_clean_mac(void);
extern int nvram_get_mac(char *mac, char which);
extern int nvram_set_mac(char *mac, char which);
extern int nvram_commit();
extern void save_config(char *filename);
extern int load_config(char *filename);
extern int nvram_save_config_to_ram(char *filename);
extern void factory_default();

/* 
 * Get the value of an NVRAM variable
 * @param	name	name of variable to get
 * @return	value of variable or NUL if undefined
 */
#define nvram_safe_get(name) (nvram_get(name) ? nvram_get(name) : "")

/*
 * Match an NVRAM variable
 * @param	name	name of variable to match
 * @param	match	value to compare against value of variable
 * @return	TRUE if variable is defined and its value is string equal to match or FALSE otherwise
 */
#define nvram_match(name, match) ({ \
	const char *value = nvram_get(name); \
	(value && !strcmp(value, match)); \
})

/*
 * Match an NVRAM variable
 * @param	name	name of variable to match
 * @param	match	value to compare against value of variable
 * @return	TRUE if variable is defined and its value is not string equal to invmatch or FALSE otherwise
 */
#define nvram_invmatch(name, invmatch) ({ \
	const char *value = nvram_get(name); \
	(value && strcmp(value, invmatch)); \
})

//jeff:add 2007/9/3 for TR-069 notification
//#define EVENT_NVRAM_COMMIT SIGUSR1
#define EVENT_NVRAM_COMMIT 16 //SIGUSR1 is 16 for this platform
#define PID_FL "/tmp/nvramd.pid"
#define CWMP_IP "127.0.0.1"
extern int nvram_commit_tr069();
//jeff-end

#endif /* _NVRAM_LIB_H_ */
