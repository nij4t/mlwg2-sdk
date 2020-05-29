/* vi: set sw=4 ts=4: */
/*
 * static_leases.c -- Couple of functions to assist with storing and
 * retrieving data for static leases
 *
 * Wade Berrier <wberrier@myrealbox.com> September 2004
 *
 */

#include "common.h"
#include "dhcpd.h"


/* Takes the address of the pointer to the static_leases linked list,
 *   Address to a 6 byte mac address
 *   Address to a 4 byte ip address */
int addStaticLease(struct static_lease **lease_struct, uint8_t *mac, uint32_t *ip)
{
	struct static_lease *cur;
	struct static_lease *new_static_lease;

	/* Build new node */
	new_static_lease = xmalloc(sizeof(struct static_lease));
	new_static_lease->mac = mac;
	new_static_lease->ip = ip;
	new_static_lease->next = NULL;

	/* If it's the first node to be added... */
	if (*lease_struct == NULL) {
		*lease_struct = new_static_lease;
	} else {
		cur = *lease_struct;
		while (cur->next) {
			cur = cur->next;
		}

		cur->next = new_static_lease;
	}

	return 1;
}

/* Check to see if a mac has an associated static lease */
uint32_t getIpByMac(struct static_lease *lease_struct, void *arg)
{
	uint32_t return_ip;
	struct static_lease *cur = lease_struct;
	uint8_t *mac = arg;

	return_ip = 0;

	while (cur) {
		/* If the client has the correct mac  */
		if (memcmp(cur->mac, mac, 6) == 0) {
			return_ip = *(cur->ip);
		}

		cur = cur->next;
	}

	return return_ip;
}

/* Check to see if an ip is reserved as a static ip */
uint32_t reservedIp(struct static_lease *lease_struct, uint32_t ip)
{
	struct static_lease *cur = lease_struct;

	uint32_t return_val = 0;

	while (cur) {
		/* If the client has the correct ip  */
		if (*cur->ip == ip)
			return_val = 1;

		cur = cur->next;
	}

	return return_val;
}

#if ENABLE_FEATURE_UDHCP_DEBUG
/* Print out static leases just to check what's going on */
/* Takes the address of the pointer to the static_leases linked list */
void printStaticLeases(struct static_lease **arg)
{
	/* Get a pointer to the linked list */
	struct static_lease *cur = *arg;

	while (cur) {
		/* printf("PrintStaticLeases: Lease mac Address: %x\n", cur->mac); */
		printf("PrintStaticLeases: Lease mac Value: %x\n", *(cur->mac));
		/* printf("PrintStaticLeases: Lease ip Address: %x\n", cur->ip); */
		printf("PrintStaticLeases: Lease ip Value: %x\n", *(cur->ip));

		cur = cur->next;
	}
}
#endif


#ifdef GT_NOT_LEASE_IP

/* Jayo 2009/08/27 : add for not lease ip  */
int addNotLeaseIP(struct not_lease_ip_range **not_lease_list_ptr, uint32_t range_start, uint32_t range_end)
{
        struct not_lease_ip_range *range_node = (struct not_lease_ip_range *) malloc(sizeof(struct not_lease_ip_range));
        range_node->start_val = range_start;
        range_node->end_val = range_end;
        range_node->next = NULL;

        /* head insert */
        if(*not_lease_list_ptr == NULL)
        {
                *not_lease_list_ptr = range_node;
                return 0;
        }

        /*  Sort by start value of range  */
        struct not_lease_ip_range *ptr = *not_lease_list_ptr;
	struct not_lease_ip_range *pre = *not_lease_list_ptr;
        for(;ptr != NULL; pre = ptr,ptr = ptr->next)
        {
                if(range_node->start_val < ptr->start_val)
                {
                        /* head insert */
                        if(ptr = *not_lease_list_ptr)
                        {
                                range_node->next = *not_lease_list_ptr;
                                *not_lease_list_ptr = range_node;
                                return 0;
                        }
			/* internal insert */
                        range_node->next = pre->next;
                        pre->next = range_node;
                        return 0;
                }
        }

        /* tail insert */
        pre->next = range_node;

	return 0;
}

/*  Jayo 2009/08/27 : Judge IP is not lease ip or not  */
int is_not_lease_ip(uint32_t addr)
{
        struct not_lease_ip_range *IP_range_ptr = server_config.not_lease_list_ptr;
        while(IP_range_ptr != NULL)
        {
                if(addr < IP_range_ptr->start_val)
                {
                        return 0;
                }
                if(addr >= IP_range_ptr->start_val && addr <= IP_range_ptr->end_val)
                {
                        return 1;
                }
                IP_range_ptr = IP_range_ptr->next;
        }
        return 0;
}

#endif
