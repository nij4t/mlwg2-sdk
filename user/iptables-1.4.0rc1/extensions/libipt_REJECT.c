/* Shared library add-on to iptables to add customized REJECT support.
 *
 * (C) 2000 Jozsef Kadlecsik <kadlec@blackhole.kfki.hu>
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <iptables.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ipt_REJECT.h>
#include <linux/version.h>

/* If we are compiling against a kernel that does not support
 * IPT_ICMP_ADMIN_PROHIBITED, we are emulating it.
 * The result will be a plain DROP of the packet instead of
 * reject. -- Maciej Soltysiak <solt@dns.toxicfilms.tv>
 */
#ifndef IPT_ICMP_ADMIN_PROHIBITED
#define IPT_ICMP_ADMIN_PROHIBITED	IPT_TCP_RESET + 1
#endif

struct reject_names {
	const char *name;
	const char *alias;
	enum ipt_reject_with with;
	const char *desc;
};

static const struct reject_names reject_table[] = {
	{"icmp-net-unreachable", "net-unreach",
		IPT_ICMP_NET_UNREACHABLE, "ICMP network unreachable"},
	{"icmp-host-unreachable", "host-unreach",
		IPT_ICMP_HOST_UNREACHABLE, "ICMP host unreachable"},
	{"icmp-proto-unreachable", "proto-unreach",
		IPT_ICMP_PROT_UNREACHABLE, "ICMP protocol unreachable"},
	{"icmp-port-unreachable", "port-unreach",
		IPT_ICMP_PORT_UNREACHABLE, "ICMP port unreachable (default)"},
#if 0
	{"echo-reply", "echoreply",
	 IPT_ICMP_ECHOREPLY, "for ICMP echo only: faked ICMP echo reply"},
#endif
	{"icmp-net-prohibited", "net-prohib",
	 IPT_ICMP_NET_PROHIBITED, "ICMP network prohibited"},
	{"icmp-host-prohibited", "host-prohib",
	 IPT_ICMP_HOST_PROHIBITED, "ICMP host prohibited"},
	{"tcp-reset", "tcp-rst",
	 IPT_TCP_RESET, "TCP RST packet"},
	{"icmp-admin-prohibited", "admin-prohib",
	 IPT_ICMP_ADMIN_PROHIBITED, "ICMP administratively prohibited (*)"}
};

static void
print_reject_types(void)
{
	unsigned int i;

	printf("Valid reject types:\n");

	for (i = 0; i < sizeof(reject_table)/sizeof(struct reject_names); i++) {
		printf("    %-25s\t%s\n", reject_table[i].name, reject_table[i].desc);
		printf("    %-25s\talias\n", reject_table[i].alias);
	}
	printf("\n");
}

/* Saves the union ipt_targinfo in parsable form to stdout. */

/* Function which prints out usage message. */
static void REJECT_help(void)
{
	printf(
"REJECT options:\n"
"--reject-with type              drop input packet and send back\n"
"                                a reply packet according to type:\n");

	print_reject_types();

	printf("(*) See man page or read the INCOMPATIBILITES file for compatibility issues.\n");
}

static const struct option REJECT_opts[] = {
	{ "reject-with", 1, NULL, '1' },
	{ }
};

/* Allocate and initialize the target. */
static void REJECT_init(struct xt_entry_target *t)
{
	struct ipt_reject_info *reject = (struct ipt_reject_info *)t->data;

	/* default */
	reject->with = IPT_ICMP_PORT_UNREACHABLE;

}

/* Function which parses command options; returns true if it
   ate an option */
static int REJECT_parse(int c, char **argv, int invert, unsigned int *flags,
                        const void *entry, struct xt_entry_target **target)
{
	struct ipt_reject_info *reject = (struct ipt_reject_info *)(*target)->data;
	unsigned int limit = sizeof(reject_table)/sizeof(struct reject_names);
	unsigned int i;

	switch(c) {
	case '1':
		if (check_inverse(optarg, &invert, NULL, 0))
			exit_error(PARAMETER_PROBLEM,
				   "Unexpected `!' after --reject-with");
		for (i = 0; i < limit; i++) {
			if ((strncasecmp(reject_table[i].name, optarg, strlen(optarg)) == 0)
			    || (strncasecmp(reject_table[i].alias, optarg, strlen(optarg)) == 0)) {
				reject->with = reject_table[i].with;
				return 1;
			}
		}
		/* This due to be dropped late in 2.4 pre-release cycle --RR */
		if (strncasecmp("echo-reply", optarg, strlen(optarg)) == 0
		    || strncasecmp("echoreply", optarg, strlen(optarg)) == 0)
			fprintf(stderr, "--reject-with echo-reply no longer"
				" supported\n");
		exit_error(PARAMETER_PROBLEM, "unknown reject type `%s'",optarg);
	default:
		/* Fall through */
		break;
	}
	return 0;
}

/* Prints out ipt_reject_info. */
static void REJECT_print(const void *ip, const struct xt_entry_target *target,
                         int numeric)
{
	const struct ipt_reject_info *reject
		= (const struct ipt_reject_info *)target->data;
	unsigned int i;

	for (i = 0; i < sizeof(reject_table)/sizeof(struct reject_names); i++) {
		if (reject_table[i].with == reject->with)
			break;
	}
	if(i < sizeof(reject_table)/sizeof(struct reject_names))	// Ben, Coverity Run: 62 (2009-11-09 20:45) CID: 4450
		printf("reject-with %s ", reject_table[i].name);
}

/* Saves ipt_reject in parsable form to stdout. */
static void REJECT_save(const void *ip, const struct xt_entry_target *target)
{
	const struct ipt_reject_info *reject
		= (const struct ipt_reject_info *)target->data;
	unsigned int i;

	for (i = 0; i < sizeof(reject_table)/sizeof(struct reject_names); i++)
		if (reject_table[i].with == reject->with)
			break;
	if(i < sizeof(reject_table)/sizeof(struct reject_names))	// Ben, Coverity Run: 62 (2009-11-09 20:45) CID: 4451
		printf("--reject-with %s ", reject_table[i].name);
}

static struct iptables_target reject_target = {
	.name		= "REJECT",
	.version	= IPTABLES_VERSION,
	.size		= IPT_ALIGN(sizeof(struct ipt_reject_info)),
	.userspacesize	= IPT_ALIGN(sizeof(struct ipt_reject_info)),
	.help		= REJECT_help,
	.init		= REJECT_init,
	.parse		= REJECT_parse,
	.print		= REJECT_print,
	.save		= REJECT_save,
	.extra_opts	= REJECT_opts,
};

void _init(void)
{
	register_target(&reject_target);
}
