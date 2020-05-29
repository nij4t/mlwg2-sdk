/* vi: set sw=4 ts=4: */
/*
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */

#include "common.h"
#include "dhcpd.h"
#include "nvram_lib.h"  //for 'nvram_set' function

int dumpleases_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int dumpleases_main(int argc UNUSED_PARAM, char **argv)
{
	int fd;
	int i;
	unsigned opt;
	time_t expires;
	const char *file = LEASES_FILE;
	const char *file_forClientMonitor = "//tmp//udhcpd.leases_forClientMonitor";
	struct dhcpOfferedAddr lease;
	struct in_addr addr;
	
	time_t curr = time(0);

	enum {
		OPT_a	= 0x1,	// -a
		OPT_r	= 0x2,	// -r
		OPT_f	= 0x4,	// -f
	};

#if ENABLE_GETOPT_LONG
	static const char dumpleases_longopts[] ALIGN1 =
		"absolute\0"  No_argument       "a"
		"remaining\0" No_argument       "r"
		"file\0"      Required_argument "f"
		;

	applet_long_options = dumpleases_longopts;
#endif
	opt_complementary = "=0:a--r:r--a";
	opt = getopt32(argv, "arf:", &file);
  if( !strcmp(nvram_safe_get("ClientMonitor"),"1") )
  	fd = xopen(file_forClientMonitor, O_RDONLY);
  else
	  fd = xopen(file, O_RDONLY);

	/* Jayo 2009/08/31 : change output format to old one */
	//printf("Hostname         Mac Address       IP-Address      Expires %s\n", (opt & OPT_a) ? "at" : "in");
	printf("Mac Address       IP-Address      Expires %s\n", (opt & OPT_a) ? "at" : "in");
	/*     "0123456789abcdef 00:00:00:00:00:00 255.255.255.255 Wed Jun 30 21:49:08 1993" */
	while (full_read(fd, &lease, sizeof(lease)) == sizeof(lease)) {
		//printf("%-16s ", lease.hostname);
		/* Jayo 2009/08/31 End */
		printf(":%02x"+1, lease.chaddr[0]);
		for (i = 1; i < 6; i++) {
			printf(":%02x", lease.chaddr[i]);
		}
		addr.s_addr = lease.yiaddr;
		printf(" %-15s ", inet_ntoa(addr));
		expires = ntohl(lease.expires);

#if 0        
        // 20070402 Jimmy add below : for dump expire seconds
        printf("expire=%lu", expires);
        printf(" ");
        // 20070402 Jimmy add above
#endif
		/* Jayo 2009/09/07 : for DHCP lease period */
		printf("expire=");

		if (!(opt & OPT_a)) { /* no -a */
//			if (!expires)
			if (expires < curr)
			{
				//puts("expired");
				printf("expired ");
			}
			else {
				/* Jayo 2009/09/07 : for DHCP lease period */
				expires -= curr;
				unsigned h, m;
				h = expires / (60*60); expires %= (60*60);
				m = expires / 60; expires %= 60;
				printf("%02u:%02u:%02u", h, m, (unsigned)expires);
				/* Jayo 2009/09/07 End */

#if 0
				unsigned d, h, m;
				d = expires / (24*60*60); expires %= (24*60*60);
				h = expires / (60*60); expires %= (60*60);
				m = expires / 60; expires %= 60;
				if (d) printf("%u days ", d);
				printf("%02u:%02u:%02u\n", h, m, (unsigned)expires);

                // 20070402 Jimmy add below : for dump expire time
                if (expires > 60*60*24) {
                        printf("%ld days, ", expires / (60*60*24));
                        expires %= 60*60*24;
                }
                if (expires > 60*60) {
                        printf("%ld hours, ", expires / (60*60));
                        expires %= 60*60;
                }
                if (expires > 60) {
                        printf("%ld minutes, ", expires / 60);
                        expires %= 60;
                }
                printf("%ld seconds", expires);         // Jimmy
                // 20070402 Jimmy add above
#endif

			}
		} else /* -a */
			fputs(ctime(&expires), stdout);

        // 20070328 Jimmy add below
        printf(" ");
        printf("host=%s", lease.hostname);
        printf(" ");
        printf("vendor=%s", lease.vendor);
        printf("\n");
        // 20070328 Jimmy add above


	}
	/* close(fd); */

	return 0;
}
