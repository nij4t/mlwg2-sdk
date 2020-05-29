#ifndef _GET_INFO_H_
#define _GET_INFO_H_
typedef struct gt_ifname
{
	char name[10];
}_ifname;
extern int getIpAddr(char *if_name, char *ip_addr);
extern int getMacAddr(char *if_name, char *mac_addr);
extern int getNetmask(char *if_name, char *netmask);
extern int getDefaultGw(char *gw);
extern int getWanIfName(char *ifname);
extern int getWanIfName2(_ifname *ifname);
extern int getWANIP(char *ip);
extern int getWanLinkStatus();
extern int getWanLinkSpeed();
extern int getWanLinkMultiplexing();
extern int getWanConnectionStatus(void);

/* Stuff, need to be removed */
int get_wireless_mac(void);

#endif
