/* vi: set sw=4 ts=4: */
/*
 * leases.c -- tools to manage DHCP leases
 * Russ Dill <Russ.Dill@asu.edu> July 2001
 */

#include "common.h"
#include "dhcpd.h"
#include "../../../nvram/nvram_lib.h"

/* Find the oldest expired lease, NULL if there are no expired leases */
static struct dhcpOfferedAddr *oldest_expired_lease(void)
{
	struct dhcpOfferedAddr *oldest = NULL;
// TODO: use monotonic_sec()
	unsigned long oldest_lease = time(0);
	unsigned i;

	for (i = 0; i < server_config.max_leases; i++)
		if (oldest_lease > leases[i].expires) {
			oldest_lease = leases[i].expires;
			oldest = &(leases[i]);
		}
	return oldest;
}


/* clear every lease out that chaddr OR yiaddr matches and is nonzero */
static void clear_lease(const uint8_t *chaddr, uint32_t yiaddr)
{
	unsigned i, j;

	for (j = 0; j < 16 && !chaddr[j]; j++)
		continue;

	for (i = 0; i < server_config.max_leases; i++)
		if ((j != 16 && memcmp(leases[i].chaddr, chaddr, 16) == 0)
		 || (yiaddr && leases[i].yiaddr == yiaddr)
		) {
			memset(&(leases[i]), 0, sizeof(leases[i]));
		}
}


/* add a lease into the table, clearing out any old ones */
struct dhcpOfferedAddr *add_lease(uint8_t *hostname, const uint8_t *chaddr, uint32_t yiaddr, unsigned long lease, uint8_t *vendor_string ) //Jimmy
{
	struct dhcpOfferedAddr *oldest;

	/* clean out any old ones */
	clear_lease(chaddr, yiaddr);

	oldest = oldest_expired_lease();

	if (oldest) {
		if (hostname) {
			uint8_t length = *(hostname-1); 
			if (length>15) length = 15; 
			memcpy(oldest->hostname, hostname, length); 
			oldest->hostname[length] = 0; 
		}
		memcpy(oldest->chaddr, chaddr, 16);
		oldest->yiaddr = yiaddr;
		oldest->expires = time(0) + lease;
        // 20070328 Jimmy add below: for dump dhcp option 12 & 60
        if(vendor_string != NULL)
                strcpy(oldest->vendor, vendor_string);
        // 20070328 Jimmy add above
	}

	return oldest;
}


/* true if a lease has expired */
int lease_expired(struct dhcpOfferedAddr *lease)
{
	return (lease->expires < (unsigned long) time(0));
}


/* Find the first lease that matches chaddr, NULL if no match */
struct dhcpOfferedAddr *find_lease_by_chaddr(const uint8_t *chaddr)
{
	unsigned i;

	for (i = 0; i < server_config.max_leases; i++)
		if (!memcmp(leases[i].chaddr, chaddr, 16))
			return &(leases[i]);

	return NULL;
}


/* Find the first lease that matches yiaddr, NULL is no match */
struct dhcpOfferedAddr *find_lease_by_yiaddr(uint32_t yiaddr)
{
	unsigned i;

	for (i = 0; i < server_config.max_leases; i++)
		if (leases[i].yiaddr == yiaddr)
			return &(leases[i]);

	return NULL;
}


/* check is an IP is taken, if it is, add it to the lease table */
static int nobody_responds_to_arp(uint32_t addr)
{
	/* 16 zero bytes */
	static const uint8_t blank_chaddr[16] = { 0 };
	/* = { 0 } helps gcc to put it in rodata, not bss */

	struct in_addr temp;
	int r;

	r = arpping(addr, server_config.server, server_config.arp, server_config.interface);
	if (r)
		return r;

	temp.s_addr = addr;
#ifdef GT_NEW_LOG
	char log_info[128] = {'\0'};
	snprintf(log_info, sizeof(log_info),"%s belongs to someone, reserving it for %u seconds",
        inet_ntoa(temp), (unsigned)server_config.conflict_time);
	addLog(_LOGINFO, log_info);
#else
	bb_info_msg("%s belongs to someone, reserving it for %u seconds",
		inet_ntoa(temp), (unsigned)server_config.conflict_time);
#endif
	add_lease(blank_chaddr, blank_chaddr, addr, server_config.conflict_time, NULL);
	return 0;
}

unsigned int filterByMask(char * netmask)
{
	unsigned int fil,mask;
	//char debug[256];
	char * addr = &mask;
	char * fil_byte = &fil;
	sscanf(netmask,"%d.%d.%d.%d", addr,addr+1,addr+2,addr+3);
	fil_byte[0]=~addr[3];
	fil_byte[1]=~addr[2];
	fil_byte[2]=~addr[1];
	fil_byte[3]=~addr[0];
	//sprintf(debug,"echo \"udhcpd addr:%X %X %X %X\" > /tmp/filterbymask", fil_byte[0],fil_byte[1],fil_byte[2],fil_byte[3]);
	//system(debug);
	return fil;
}


/* find an assignable address, if check_expired is true, we check all the expired leases as well.
 * Maybe this should try expired leases by age... */
uint32_t find_address(int check_expired)
{
	uint32_t addr, ret;
	struct dhcpOfferedAddr *lease = NULL;

	char mask[32];
	//char debug[256];
	unsigned int filter;
	strcpy(mask, nvram_safe_get("LanNetmask"));
	
	//sprintf(debug, "echo \"udhcpd mask:%s\" > /tmp/netmask ", mask);
	//system(debug);
	filter = filterByMask(mask);
	//bzero(debug,256);
	//sprintf(debug, "echo \"actual filter:%X\" > /tmp/actfilter ", filter);
	//system(debug);	
	
	addr = server_config.start_ip; /* addr is in host order here */
	for (; addr <= server_config.end_ip; addr++) {
		/* ie, 192.168.55.0 */
		if (!(addr & filter))
			continue;
		/* ie, 192.168.55.255 */
		if ((addr & filter) == filter)
			continue;

#ifdef GT_NOT_LEASE_IP
		/*  Jayo 2009/08/27 : Judge IP is not lease ip or not  */
		if (is_not_lease_ip(addr))
       		continue;
#endif
		/* Only do if it isn't assigned as a static lease */
		ret = htonl(addr);
		if (!reservedIp(server_config.static_leases, ret)) {
			/* lease is not taken */
			lease = find_lease_by_yiaddr(ret);
			/* no lease or it expired and we are checking for expired leases */
			if ((!lease || (check_expired && lease_expired(lease)))
			 && nobody_responds_to_arp(ret) /* it isn't used on the network */
			) {
				return ret;
			}
		}
	}
	return 0;
}
