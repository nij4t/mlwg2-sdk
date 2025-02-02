/*	$Id: //WIFI_SOC/release/SDK_4_1_0_0/source/user/dhcp6/dhcp6.h#1 $	*/
/*	ported from KAME: dhcp6.h,v 1.32 2002/07/04 15:03:19 jinmei Exp	*/

/*
 * Copyright (C) 1998 and 1999 WIDE Project.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * draft-ietf-dhc-dhcpv6-26
 */

#ifndef __DHCP6_H_DEFINED
#define __DHCP6_H_DEFINED

/* Error Values */
#define DH6ERR_FAILURE		16
#define DH6ERR_AUTHFAIL		17
#define DH6ERR_POORLYFORMED	18
#define DH6ERR_UNAVAIL		19
#define DH6ERR_OPTUNAVAIL	20

/* Message type */
#define DH6_SOLICIT	1
#define DH6_ADVERTISE	2
#define DH6_REQUEST	3
#define DH6_CONFIRM	4
#define DH6_RENEW	5
#define DH6_REBIND	6
#define DH6_REPLY	7
#define DH6_RELEASE	8
#define DH6_DECLINE	9
#define DH6_RECONFIGURE	10
#define DH6_INFORM_REQ	11
#define DH6_RELAY_FORW	12
#define DH6_RELAY_REPL	13

/* Predefined addresses */
#define DH6ADDR_ALLAGENT	"ff02::1:2"
#define DH6ADDR_ALLSERVER	"ff05::1:3"
#define DH6PORT_DOWNSTREAM	"546"
#define DH6PORT_UPSTREAM	"547"

/* Protocol constants */

/* timer parameters (msec, unless explicitly commented) */
#define MIN_SOL_DELAY	500
#define MAX_SOL_DELAY	1000
#define SOL_TIMEOUT	1000
#define SOL_MAX_RT	120000
#define INF_TIMEOUT	1000
#define INF_MAX_DELAY	1000
#define INF_MAX_RT	120000
#define REQ_TIMEOUT	1000
#define REQ_MAX_RT	30000
#define REQ_MAX_RC	10	/* Max Request retry attempts */
#define REN_TIMEOUT	10000	/* 10secs */
#define REN_MAX_RT	600000	/* 600secs */
#define REB_TIMEOUT	10000	/* 10secs */
#define REB_MAX_RT	600000	/* 600secs */
#define DEC_TIMEOUT	1000
#define DEC_MAX_RC	5
#define REL_TIMEOUT	1000
#define REL_MAX_RC	5
#define REC_TIMEOUT	2000
#define REC_MAX_RC	8
#define CNF_TIMEOUT	1000
#define CNF_MAX_RD	10000
#define CNF_MAX_RT	4000

#define DHCP6_DURATITION_INFINITE 0xffffffff
#define DHCP6_ELAPSEDTIME_MAX	0xffff

#define IF_RA_OTHERCONF 0x80
#define IF_RA_MANAGED   0x40
#define RTM_F_PREFIX	0x800

#ifndef MAXDNAME
#define MAXDNAME 255
#endif
#define MAXDN 100

#define RESOLV_CONF_FILE "/etc/resolv.conf"
#define RESOLV_CONF_BAK_FILE "/etc/resolv.conf.dhcpv6.bak"
#define RESOLV_CONF_DHCPV6_FILE "/etc/resolv.conf.dhcpv6"
char resolv_dhcpv6_file[254];

#define RADVD_CONF_FILE "/etc/radvd.conf"
#define RADVD_CONF_BAK_FILE "/etc/radvd.conf.dhcpv6.bak"
#define RADVD_CONF_DHCPV6_FILE "/etc/radvd.conf.dhcpv6"
char radvd_dhcpv6_file[254];


typedef enum { IANA, IATA, IAPD} iatype_t;

typedef enum { ACTIVE, RENEW,
	       REBIND, EXPIRED,
	       INVALID } state_t;
/* Internal data structure */

struct duid {
	u_int8_t duid_len;	/* length */
	char *duid_id;		/* variable length ID value (must be opaque) */
};

/* iaid info for the IA_NA */
struct dhcp6_iaid_info {
	u_int32_t iaid;
	u_int32_t renewtime;
	u_int32_t rebindtime;
};

/* dhcpv6 addr */
struct dhcp6_addr {
	u_int32_t validlifetime;
	u_int32_t preferlifetime;
	struct in6_addr addr;
	u_int8_t plen;
	iatype_t type;
	u_int16_t status_code;
	char *status_msg;
};

struct dhcp6_lease {
	TAILQ_ENTRY(dhcp6_lease) link;
	char hostname[1024];
	struct in6_addr linklocal;
	struct dhcp6_addr lease_addr;
	iatype_t addr_type;
	state_t state;
	struct dhcp6_iaidaddr *iaidaddr;
	time_t start_date;
	/* address assigned on the interface */
	struct dhcp6_timer *timer;
};

struct dhcp6_listval {
	TAILQ_ENTRY(dhcp6_listval) link;

	union {
		int uv_num;
		struct in6_addr uv_addr6;
		struct dhcp6_addr uv_dhcp6_addr;
		struct dhcp6_lease uv_dhcp6_lease;
	} uv;
};

#define val_num uv.uv_num
#define val_addr6 uv.uv_addr6
#define val_dhcp6addr uv.uv_dhcp6_addr
#define val_dhcp6lease uv.uv_dhcp6_lease

TAILQ_HEAD(dhcp6_list, dhcp6_listval);

typedef enum { DHCP6_LISTVAL_NUM, DHCP6_LISTVAL_ADDR6,
	       DHCP6_LISTVAL_DHCP6ADDR, DHCP6_LISTVAL_DHCP6LEASE } dhcp6_listval_type_t;

struct domain_list {
	struct domain_list *next;
	char name[MAXDNAME];
};

struct dns_list {
	struct dhcp6_list addrlist;
	struct domain_list *domainlist;
};

struct dhcp6_optinfo {
	struct duid clientID;	/* DUID */
	struct duid serverID;	/* DUID */
	u_int16_t elapsed_time;
	struct dhcp6_iaid_info iaidinfo;
	iatype_t type;
	u_int8_t flags;	/* flags for rapid commit, info_only, temp address */
	u_int8_t pref;		/* server preference */
	struct in6_addr server_addr;
	struct dhcp6_list addr_list; /* assigned ipv6 address list */
	struct dhcp6_list reqopt_list; /*  options in option request */
	struct dhcp6_list stcode_list; /* status code */
	struct dns_list dns_list; /* DNS server list */
};

/* DHCP6 base packet format */
struct dhcp6 {
	union {
		u_int8_t m;
		u_int32_t x;
	} dh6_msgtypexid;
	/* options follow */
} __attribute__ ((__packed__));
#define dh6_msgtype	dh6_msgtypexid.m
#define dh6_xid		dh6_msgtypexid.x
#define DH6_XIDMASK	0x00ffffff

/* options */
#define DH6OPT_CLIENTID	1
#define DH6OPT_SERVERID	2
#define DH6OPT_IA_NA 3
#define DH6OPT_IA_TA 4
#define DH6OPT_IADDR 5
#define DH6OPT_ORO 6
#define DH6OPT_PREFERENCE 7
#  define DH6OPT_PREF_UNDEF 0 
#  define DH6OPT_PREF_MAX 255
#define DH6OPT_ELAPSED_TIME 8
#define DH6OPT_CLIENT_MSG 9
#define DH6OPT_SERVER_MSG 10
#define DH6OPT_AUTH 11
#define DH6OPT_UNICAST 12
#define DH6OPT_STATUS_CODE 13


#  define DH6OPT_STCODE_SUCCESS 0
#  define DH6OPT_STCODE_UNSPECFAIL 1
#  define DH6OPT_STCODE_NOADDRAVAIL 2
#  define DH6OPT_STCODE_NOBINDING 3
#  define DH6OPT_STCODE_NOTONLINK 4
#  define DH6OPT_STCODE_USEMULTICAST 5

#  define DH6OPT_STCODE_AUTHFAILED 6
#  define DH6OPT_STCODE_ADDRUNAVAIL 7
#  define DH6OPT_STCODE_CONFNOMATCH 8

#  define DH6OPT_STCODE_NOPREFIXAVAIL 10

#  define DH6OPT_STCODE_UNDEFINE 0xffff

#define DH6OPT_RAPID_COMMIT 14
#define DH6OPT_USER_CLASS 15
#define DH6OPT_VENDOR_CLASS 16
#define DH6OPT_VENDOR_OPTS 17
#define DH6OPT_INTERFACE_ID 18
#define DH6OPT_RECONF_MSG 19

#define DEFAULT_VALID_LIFE_TIME 720000
#define DEFAULT_PREFERRED_LIFE_TIME 360000

/*
 * The option type has not been assigned for the following options.
 * We temporarily adopt values used in the service specification document
 * (200206xx version) by NTT Communications as default values.
 * Note that we'll fix the following definitions when official values are
 * assigned.
 */
#define DH6OPT_DNS_RESOLVERS CONF_DH6OPT_DNS_RESOLVERS
#define DH6OPT_DOMAIN_LIST CONF_DH6OPT_DOMAIN_LIST

/*
 * define PD option according IPv6 Prefix Options for DHCPv6 draft 02
 */
#define DH6OPT_IA_PD CONF_DH6OPT_IA_PD
#define DH6OPT_IAPREFIX CONF_DH6OPT_IAPREFIX

struct dhcp6opt {
	u_int16_t dh6opt_type;
	u_int16_t dh6opt_len;
	/* type-dependent data follows */
} __attribute__ ((__packed__));

/* DUID type 1 */
struct dhcp6_duid_type1 {
	u_int16_t dh6duid1_type;
	u_int16_t dh6duid1_hwtype;
	u_int32_t dh6duid1_time;
	/* link-layer address follows */
} __attribute__ ((__packed__));

/* Prefix Information */
struct dhcp6_prefix_info {
	u_int16_t dh6_pi_type;
	u_int16_t dh6_pi_len;
	u_int32_t preferlifetime;
	u_int32_t validlifetime;
	u_int8_t plen;
	struct in6_addr prefix;
} __attribute__ ((__packed__));

/* status code info */
struct dhcp6_status_info {
	u_int16_t dh6_status_type;
	u_int16_t dh6_status_len;
	u_int16_t dh6_status_code;
} __attribute__ ((__packed__));

/* IPv6 address info */
struct dhcp6_addr_info {
	u_int16_t dh6_ai_type;
	u_int16_t dh6_ai_len;
	struct in6_addr addr;
	u_int32_t preferlifetime;
	u_int32_t validlifetime;
/*	u_int8_t plen;	
	struct dhcp6_status_info status;
*/
} __attribute__ ((__packed__));

#endif /*__DHCP6_H_DEFINED*/
