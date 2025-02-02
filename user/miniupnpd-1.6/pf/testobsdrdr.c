/* $Id: //WIFI_SOC/release/SDK_4_1_0_0/source/user/miniupnpd-1.6/pf/testobsdrdr.c#1 $ */
/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * (c) 2006-2011 Thomas Bernard
 * This software is subject to the conditions detailed
 * in the LICENCE file provided within the distribution */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <syslog.h>

#include "obsdrdr.h"

/*int logpackets = 1;*/
int runtime_flags = 0;
const char * tag = 0;

void
list_rules(void);

void
list_eports_tcp(void)
{
	unsigned short * port_list;
	unsigned int number = 0;
	unsigned int i;
	port_list = get_portmappings_in_range(0, 65535, IPPROTO_TCP, &number);
	printf("%u ports redirected (TCP) :", number);
	for(i = 0; i < number; i++)
	{
		printf(" %hu", port_list[i]);
	}
	printf("\n");
	free(port_list);
}

void
test_index(void)
{
	char ifname[16/*IFNAMSIZ*/];
	char iaddr[32];
	char desc[64];
	char rhost[32];
	unsigned short iport = 0;
	unsigned short eport = 0;
	int proto = 0;
	unsigned int timestamp;
	ifname[0] = '\0';
	iaddr[0] = '\0';
	rhost[0] = '\0';
	if(get_redirect_rule_by_index(0, ifname, &eport, iaddr, sizeof(iaddr),
	                              &iport, &proto, desc, sizeof(desc),
	                              rhost, sizeof(rhost),
                                  &timestamp, 0, 0) < 0)
	{
		printf("get.._by_index : no rule\n");
	}
	else
	{
		printf("%s %u -> %s:%u proto %d\n", ifname, (unsigned int)eport,
		       iaddr, (unsigned int)iport, proto);
		printf("description: \"%s\"\n", desc);
	}
}

int
main(int arc, char * * argv)
{
	char buf[32];
	char desc[64];
	/*char rhost[32];*/
	unsigned short iport;
	unsigned int timestamp;
	u_int64_t packets = 0;
	u_int64_t bytes = 0;

	openlog("testobsdrdr", LOG_PERROR, LOG_USER);
	if(init_redirect() < 0)
	{
		fprintf(stderr, "init_redirect() failed\n");
		return 1;
	}
	//add_redirect_rule("ep0", 12123, "192.168.1.23", 1234);
	//add_redirect_rule2("ep0", 12155, "192.168.1.155", 1255, IPPROTO_TCP);
	add_redirect_rule2("ep0", "8.8.8.8", 12123, "192.168.1.125", 1234,
	                   IPPROTO_UDP, "test description", 0);
	//add_redirect_rule2("em0", 12123, "127.1.2.3", 1234,
	//                   IPPROTO_TCP, "test description tcp");

	list_rules();
	list_eports_tcp();


	if(get_redirect_rule("xl1", 4662, IPPROTO_TCP,
	                     buf, sizeof(buf), &iport, desc, sizeof(desc),
	                     &timestamp,
	                     &packets, &bytes) < 0)
		printf("get_redirect_rule() failed\n");
	else
	{
		printf("\n%s:%d '%s' packets=%llu bytes=%llu\n", buf, (int)iport, desc,
		       packets, bytes);
	}
#if 0
	if(delete_redirect_rule("ep0", 12123, IPPROTO_UDP) < 0)
		printf("delete_redirect_rule() failed\n");
	else
		printf("delete_redirect_rule() succeded\n");

	if(delete_redirect_rule("ep0", 12123, IPPROTO_UDP) < 0)
		printf("delete_redirect_rule() failed\n");
	else
		printf("delete_redirect_rule() succeded\n");
#endif
	//test_index();

	//clear_redirect_rules();
	//list_rules();

	return 0;
}


