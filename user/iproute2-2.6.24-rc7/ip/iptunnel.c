/*
 * iptunnel.c	       "ip tunnel"
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 *
 *
 * Changes:
 *
 * Rani Assaf <rani@magic.metawire.com> 980929:	resolve addresses
 * Rani Assaf <rani@magic.metawire.com> 980930:	do not allow key for ipip/sit
 * Phil Karn <karn@ka9q.ampr.org>	990408:	"pmtudisc" flag
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/if_arp.h>
#include <linux/ip.h>
#include <linux/if_tunnel.h>

#include "rt_names.h"
#include "utils.h"
#include "ip_common.h"
#include "tunnel.h"

static void usage(void) __attribute__((noreturn));

static void usage(void)
{
	fprintf(stderr, "Usage: ip tunnel { add | change | del | show } [ NAME ]\n");
	fprintf(stderr, "          [ mode { ipip | gre | sit } ] [ remote ADDR ] [ local ADDR ]\n");
	fprintf(stderr, "          [ [i|o]seq ] [ [i|o]key KEY ] [ [i|o]csum ]\n");
	fprintf(stderr, "          [ ttl TTL ] [ tos TOS ] [ [no]pmtudisc ] [ dev PHYS_DEV ]\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Where: NAME := STRING\n");
	fprintf(stderr, "       ADDR := { IP_ADDRESS | any }\n");
	fprintf(stderr, "       TOS  := { NUMBER | inherit }\n");
	fprintf(stderr, "       TTL  := { 1..255 | inherit }\n");
	fprintf(stderr, "       KEY  := { DOTTED_QUAD | NUMBER }\n");
	exit(-1);
}

static int parse_args(int argc, char **argv, int cmd, struct ip_tunnel_parm *p)
{
	int count = 0;
	char medium[IFNAMSIZ];

	memset(p, 0, sizeof(*p));
	memset(&medium, 0, sizeof(medium));

	p->iph.version = 4;
	p->iph.ihl = 5;
#ifndef IP_DF
#define IP_DF		0x4000		/* Flag: "Don't Fragment"	*/
#endif
	p->iph.frag_off = htons(IP_DF);

	while (argc > 0) {
		if (strcmp(*argv, "mode") == 0) {
			NEXT_ARG();
			if (strcmp(*argv, "ipip") == 0 ||
			    strcmp(*argv, "ip/ip") == 0) {
				if (p->iph.protocol && p->iph.protocol != IPPROTO_IPIP) {
					fprintf(stderr,"You managed to ask for more than one tunnel mode.\n");
					exit(-1);
				}
				p->iph.protocol = IPPROTO_IPIP;
			} else if (strcmp(*argv, "gre") == 0 ||
				   strcmp(*argv, "gre/ip") == 0) {
				if (p->iph.protocol && p->iph.protocol != IPPROTO_GRE) {
					fprintf(stderr,"You managed to ask for more than one tunnel mode.\n");
					exit(-1);
				}
				p->iph.protocol = IPPROTO_GRE;
			} else if (strcmp(*argv, "sit") == 0 ||
				   strcmp(*argv, "ipv6/ip") == 0) {
				if (p->iph.protocol && p->iph.protocol != IPPROTO_IPV6) {
					fprintf(stderr,"You managed to ask for more than one tunnel mode.\n");
					exit(-1);
				}
				p->iph.protocol = IPPROTO_IPV6;
			} else {
				fprintf(stderr,"Cannot guess tunnel mode.\n");
				exit(-1);
			}
		} else if (strcmp(*argv, "key") == 0) {
			unsigned uval;
			NEXT_ARG();
			p->i_flags |= GRE_KEY;
			p->o_flags |= GRE_KEY;
			if (strchr(*argv, '.'))
				p->i_key = p->o_key = get_addr32(*argv);
			else {
				if (get_unsigned(&uval, *argv, 0)<0) {
					fprintf(stderr, "invalid value of \"key\"\n");
					exit(-1);
				}
				p->i_key = p->o_key = htonl(uval);
			}
		} else if (strcmp(*argv, "ikey") == 0) {
			unsigned uval;
			NEXT_ARG();
			p->i_flags |= GRE_KEY;
			if (strchr(*argv, '.'))
				p->i_key = get_addr32(*argv);
			else {
				if (get_unsigned(&uval, *argv, 0)<0) {
					fprintf(stderr, "invalid value of \"ikey\"\n");
					exit(-1);
				}
				p->i_key = htonl(uval);
			}
		} else if (strcmp(*argv, "okey") == 0) {
			unsigned uval;
			NEXT_ARG();
			p->o_flags |= GRE_KEY;
			if (strchr(*argv, '.'))
				p->o_key = get_addr32(*argv);
			else {
				if (get_unsigned(&uval, *argv, 0)<0) {
					fprintf(stderr, "invalid value of \"okey\"\n");
					exit(-1);
				}
				p->o_key = htonl(uval);
			}
		} else if (strcmp(*argv, "seq") == 0) {
			p->i_flags |= GRE_SEQ;
			p->o_flags |= GRE_SEQ;
		} else if (strcmp(*argv, "iseq") == 0) {
			p->i_flags |= GRE_SEQ;
		} else if (strcmp(*argv, "oseq") == 0) {
			p->o_flags |= GRE_SEQ;
		} else if (strcmp(*argv, "csum") == 0) {
			p->i_flags |= GRE_CSUM;
			p->o_flags |= GRE_CSUM;
		} else if (strcmp(*argv, "icsum") == 0) {
			p->i_flags |= GRE_CSUM;
		} else if (strcmp(*argv, "ocsum") == 0) {
			p->o_flags |= GRE_CSUM;
		} else if (strcmp(*argv, "nopmtudisc") == 0) {
			p->iph.frag_off = 0;
		} else if (strcmp(*argv, "pmtudisc") == 0) {
			p->iph.frag_off = htons(IP_DF);
		} else if (strcmp(*argv, "remote") == 0) {
			NEXT_ARG();
			if (strcmp(*argv, "any"))
				p->iph.daddr = get_addr32(*argv);
		} else if (strcmp(*argv, "local") == 0) {
			NEXT_ARG();
			if (strcmp(*argv, "any"))
				p->iph.saddr = get_addr32(*argv);
		} else if (strcmp(*argv, "dev") == 0) {
			NEXT_ARG();
			strncpy(medium, *argv, IFNAMSIZ-1);
		} else if (strcmp(*argv, "ttl") == 0) {
			unsigned uval;
			NEXT_ARG();
			if (strcmp(*argv, "inherit") != 0) {
				if (get_unsigned(&uval, *argv, 0))
					invarg("invalid TTL\n", *argv);
				if (uval > 255)
					invarg("TTL must be <=255\n", *argv);
				p->iph.ttl = uval;
			}
		} else if (strcmp(*argv, "tos") == 0 ||
			   matches(*argv, "dsfield") == 0) {
			__u32 uval;
			NEXT_ARG();
			if (strcmp(*argv, "inherit") != 0) {
				if (rtnl_dsfield_a2n(&uval, *argv))
					invarg("bad TOS value", *argv);
				p->iph.tos = uval;
			} else
				p->iph.tos = 1;
		} else {
			if (strcmp(*argv, "name") == 0) {
				NEXT_ARG();
			}
			if (matches(*argv, "help") == 0)
				usage();
			if (p->name[0])
				duparg2("name", *argv);
			strncpy(p->name, *argv, IFNAMSIZ);
			if (cmd == SIOCCHGTUNNEL && count == 0) {
				struct ip_tunnel_parm old_p;
				memset(&old_p, 0, sizeof(old_p));
				if (tnl_get_ioctl(*argv, &old_p))
					return -1;
				*p = old_p;
			}
		}
		count++;
		argc--; argv++;
	}


	if (p->iph.protocol == 0) {
		if (memcmp(p->name, "gre", 3) == 0)
			p->iph.protocol = IPPROTO_GRE;
		else if (memcmp(p->name, "ipip", 4) == 0)
			p->iph.protocol = IPPROTO_IPIP;
		else if (memcmp(p->name, "sit", 3) == 0)
			p->iph.protocol = IPPROTO_IPV6;
	}

	if (p->iph.protocol == IPPROTO_IPIP || p->iph.protocol == IPPROTO_IPV6) {
		if ((p->i_flags & GRE_KEY) || (p->o_flags & GRE_KEY)) {
			fprintf(stderr, "Keys are not allowed with ipip and sit.\n");
			return -1;
		}
	}

	if (medium[0]) {
		p->link = tnl_ioctl_get_ifindex(medium);
		if (p->link == 0)
			return -1;
	}

	if (p->i_key == 0 && IN_MULTICAST(ntohl(p->iph.daddr))) {
		p->i_key = p->iph.daddr;
		p->i_flags |= GRE_KEY;
	}
	if (p->o_key == 0 && IN_MULTICAST(ntohl(p->iph.daddr))) {
		p->o_key = p->iph.daddr;
		p->o_flags |= GRE_KEY;
	}
	if (IN_MULTICAST(ntohl(p->iph.daddr)) && !p->iph.saddr) {
		fprintf(stderr, "Broadcast tunnel requires a source address.\n");
		return -1;
	}
	return 0;
}


static int do_add(int cmd, int argc, char **argv)
{
	struct ip_tunnel_parm p;

	if (parse_args(argc, argv, cmd, &p) < 0)
		return -1;

	if (p.iph.ttl && p.iph.frag_off == 0) {
		fprintf(stderr, "ttl != 0 and noptmudisc are incompatible\n");
		return -1;
	}

	switch (p.iph.protocol) {
	case IPPROTO_IPIP:
		return tnl_add_ioctl(cmd, "tunl0", p.name, &p);
	case IPPROTO_GRE:
		return tnl_add_ioctl(cmd, "gre0", p.name, &p);
	case IPPROTO_IPV6:
		return tnl_add_ioctl(cmd, "sit0", p.name, &p);
	default:
		fprintf(stderr, "cannot determine tunnel mode (ipip, gre or sit)\n");
		return -1;
	}
	return -1;
}

static int do_del(int argc, char **argv)
{
	struct ip_tunnel_parm p;

	if (parse_args(argc, argv, SIOCDELTUNNEL, &p) < 0)
		return -1;

	switch (p.iph.protocol) {
	case IPPROTO_IPIP:
		return tnl_del_ioctl("tunl0", p.name, &p);
	case IPPROTO_GRE:
		return tnl_del_ioctl("gre0", p.name, &p);
	case IPPROTO_IPV6:
		return tnl_del_ioctl("sit0", p.name, &p);
	default:
		return tnl_del_ioctl(p.name, p.name, &p);
	}
	return -1;
}

static void print_tunnel(struct ip_tunnel_parm *p)
{
	char s1[1024];
	char s2[1024];
	char s3[64];
	char s4[64];

	inet_ntop(AF_INET, &p->i_key, s3, sizeof(s3));
	inet_ntop(AF_INET, &p->o_key, s4, sizeof(s4));

	/* Do not use format_host() for local addr,
	 * symbolic name will not be useful.
	 */
	printf("%s: %s/ip  remote %s  local %s ",
	       p->name,
	       tnl_strproto(p->iph.protocol),
	       p->iph.daddr ? format_host(AF_INET, 4, &p->iph.daddr, s1, sizeof(s1))  : "any",
	       p->iph.saddr ? rt_addr_n2a(AF_INET, 4, &p->iph.saddr, s2, sizeof(s2)) : "any");

	if (p->link) {
		char *n = tnl_ioctl_get_ifname(p->link);
		if (n)
			printf(" dev %s ", n);
	}

	if (p->iph.ttl)
		printf(" ttl %d ", p->iph.ttl);
	else
		printf(" ttl inherit ");

	if (p->iph.tos) {
		SPRINT_BUF(b1);
		printf(" tos");
		if (p->iph.tos&1)
			printf(" inherit");
		if (p->iph.tos&~1)
			printf("%c%s ", p->iph.tos&1 ? '/' : ' ',
			       rtnl_dsfield_n2a(p->iph.tos&~1, b1, sizeof(b1)));
	}

	if (!(p->iph.frag_off&htons(IP_DF)))
		printf(" nopmtudisc");

	if ((p->i_flags&GRE_KEY) && (p->o_flags&GRE_KEY) && p->o_key == p->i_key)
		printf(" key %s", s3);
	else if ((p->i_flags|p->o_flags)&GRE_KEY) {
		if (p->i_flags&GRE_KEY)
			printf(" ikey %s ", s3);
		if (p->o_flags&GRE_KEY)
			printf(" okey %s ", s4);
	}

	if (p->i_flags&GRE_SEQ)
		printf("%s  Drop packets out of sequence.\n", _SL_);
	if (p->i_flags&GRE_CSUM)
		printf("%s  Checksum in received packet is required.", _SL_);
	if (p->o_flags&GRE_SEQ)
		printf("%s  Sequence packets on output.", _SL_);
	if (p->o_flags&GRE_CSUM)
		printf("%s  Checksum output packets.", _SL_);
}

static int do_tunnels_list(struct ip_tunnel_parm *p)
{
	char name[IFNAMSIZ];
	unsigned long  rx_bytes, rx_packets, rx_errs, rx_drops,
	rx_fifo, rx_frame,
	tx_bytes, tx_packets, tx_errs, tx_drops,
	tx_fifo, tx_colls, tx_carrier, rx_multi;
	int type;
	struct ip_tunnel_parm p1;

	char buf[512];
	FILE *fp = fopen("/proc/net/dev", "r");
	if (fp == NULL) {
		perror("fopen");
		return -1;
	}

	fgets(buf, sizeof(buf), fp);
	fgets(buf, sizeof(buf), fp);

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		char *ptr;
		buf[sizeof(buf) - 1] = 0;
		if ((ptr = strchr(buf, ':')) == NULL ||
		    (*ptr++ = 0, sscanf(buf, "%s", name) != 1)) {
			fprintf(stderr, "Wrong format of /proc/net/dev. Sorry.\n");
			fclose(fp);	// Ben, Coverity Run: 62 (2009-11-09 20:45) CID: 4473
			return -1;
		}
		if (sscanf(ptr, "%ld%ld%ld%ld%ld%ld%ld%*d%ld%ld%ld%ld%ld%ld%ld",
			   &rx_bytes, &rx_packets, &rx_errs, &rx_drops,
			   &rx_fifo, &rx_frame, &rx_multi,
			   &tx_bytes, &tx_packets, &tx_errs, &tx_drops,
			   &tx_fifo, &tx_colls, &tx_carrier) != 14)
			continue;
		if (p->name[0] && strcmp(p->name, name))
			continue;
		type = tnl_ioctl_get_iftype(name);
		if (type == -1) {
			fprintf(stderr, "Failed to get type of [%s]\n", name);
			continue;
		}
		if (type != ARPHRD_TUNNEL && type != ARPHRD_IPGRE && type != ARPHRD_SIT)
			continue;
		memset(&p1, 0, sizeof(p1));
		if (tnl_get_ioctl(name, &p1))
			continue;
		if ((p->link && p1.link != p->link) ||
		    (p->name[0] && strcmp(p1.name, p->name)) ||
		    (p->iph.daddr && p1.iph.daddr != p->iph.daddr) ||
		    (p->iph.saddr && p1.iph.saddr != p->iph.saddr) ||
		    (p->i_key && p1.i_key != p->i_key))
			continue;
		print_tunnel(&p1);
		if (show_stats) {
			printf("%s", _SL_);
			printf("RX: Packets    Bytes        Errors CsumErrs OutOfSeq Mcasts%s", _SL_);
			printf("    %-10ld %-12ld %-6ld %-8ld %-8ld %-8ld%s",
			       rx_packets, rx_bytes, rx_errs, rx_frame, rx_fifo, rx_multi, _SL_);
			printf("TX: Packets    Bytes        Errors DeadLoop NoRoute  NoBufs%s", _SL_);
			printf("    %-10ld %-12ld %-6ld %-8ld %-8ld %-6ld",
			       tx_packets, tx_bytes, tx_errs, tx_colls, tx_carrier, tx_drops);
		}
		printf("\n");
	}
	fclose(fp); // Ben, Coverity Run: 62 (2009-11-09 20:45) CID: 4473
	return 0;
}

static int do_show(int argc, char **argv)
{
	int err;
	struct ip_tunnel_parm p;

	if (parse_args(argc, argv, SIOCGETTUNNEL, &p) < 0)
		return -1;

	switch (p.iph.protocol) {
	case IPPROTO_IPIP:
		err = tnl_get_ioctl(p.name[0] ? p.name : "tunl0", &p);
		break;
	case IPPROTO_GRE:
		err = tnl_get_ioctl(p.name[0] ? p.name : "gre0", &p);
		break;
	case IPPROTO_IPV6:
		err = tnl_get_ioctl(p.name[0] ? p.name : "sit0", &p);
		break;
	default:
		do_tunnels_list(&p);
		return 0;
	}
	if (err)
		return -1;

	print_tunnel(&p);
	printf("\n");
	return 0;
}

int do_iptunnel(int argc, char **argv)
{
	switch (preferred_family) {
	case AF_UNSPEC:
		preferred_family = AF_INET;
		break;
	case AF_INET:
		break;
	/*
	 * This is silly enough but we have no easy way to make it
	 * protocol-independent because of unarranged structure between
	 * IPv4 and IPv6.
	 */
	case AF_INET6:
		return do_ip6tunnel(argc, argv);
	default:
		fprintf(stderr, "Unsupported family:%d\n", preferred_family);
		exit(-1);
	}

	if (argc > 0) {
		if (matches(*argv, "add") == 0)
			return do_add(SIOCADDTUNNEL, argc-1, argv+1);
		if (matches(*argv, "change") == 0)
			return do_add(SIOCCHGTUNNEL, argc-1, argv+1);
		if (matches(*argv, "del") == 0)
			return do_del(argc-1, argv+1);
		if (matches(*argv, "show") == 0 ||
		    matches(*argv, "lst") == 0 ||
		    matches(*argv, "list") == 0)
			return do_show(argc-1, argv+1);
		if (matches(*argv, "help") == 0)
			usage();
	} else
		return do_show(0, NULL);

	fprintf(stderr, "Command \"%s\" is unknown, try \"ip tunnel help\".\n", *argv);
	exit(-1);
}
