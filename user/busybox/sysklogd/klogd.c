/* vi: set sw=4 ts=4: */
/*
 * Mini klogd implementation for busybox
 *
 * Copyright (C) 2001 by Gennady Feldman <gfeldman@gena01.com>.
 * Changes: Made this a standalone busybox module which uses standalone
 *					syslog() client interface.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Copyright (C) 2000 by Karl M. Hegbloom <karlheg@debian.org>
 *
 * "circular buffer" Copyright (C) 2000 by Gennady Feldman <gfeldman@gena01.com>
 *
 * Maintainer: Gennady Feldman <gfeldman@gena01.com> as of Mar 12, 2001
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */

#include "libbb.h"
#include <syslog.h>
#include <sys/klog.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#define WIFI_UNASO_ARP 1

static void klogd_signal(int sig)
{
	/* FYI: cmd 7 is equivalent to setting console_loglevel to 7
	 * via klogctl(8, NULL, 7). */
	klogctl(7, NULL, 0); /* "7 -- Enable printk's to console" */
	klogctl(0, NULL, 0); /* "0 -- Close the log. Currently a NOP" */
	syslog(LOG_NOTICE, "klogd: exiting");
	kill_myself_with_sig(sig);
}

#ifdef WIFI_UNASO_ARP
/* Charlie add 2009.3.25 */
static void deleteArpRule(char *ip)
{
	struct arpreq {
           struct sockaddr   arp_pa;     /* protocol address */
           struct sockaddr   arp_ha;     /* hardware address */
           int               arp_flags;  /* flags */
           struct sockaddr   arp_netmask;
           char              arp_dev[16];
       };
        struct arpreq arpreq;
        struct sockaddr_in *si;
        struct in_addr ina;
        int s=0,result=0;

        s = socket(AF_INET, SOCK_DGRAM, 0);
        bzero(&arpreq, sizeof(arpreq));
        arpreq.arp_pa.sa_family = AF_INET;
        si = (struct sockaddr_in *) &arpreq.arp_pa;
        memset(si, 0, sizeof(struct sockaddr_in));
        si->sin_family = AF_INET;
        ina.s_addr = inet_addr(ip);
        arpreq.arp_flags = ATF_COM | ATF_PERM;
        memcpy(&si->sin_addr, (char *)&ina, sizeof(struct in_addr));
        if (s!=0)
        {
                result=ioctl(s, SIOCDARP, (caddr_t)&arpreq);
                close(s);
        }
}

static void checkLogToDeleteArp(char *wmsg)
{
    char logmsg[4][256];
    char arptbl[6][128];
    char buffer[256];
    FILE *arp_flag=NULL;
	int i=0,get_flag=0;
    
    memset(logmsg,0x00,sizeof(logmsg));
    memset(arptbl,0x00,sizeof(arptbl));
    memset(buffer,0x00,sizeof(buffer));

    sscanf(wmsg,"%s %s %s %s", logmsg[0], logmsg[1], logmsg[2] ,logmsg[3]);
	if (strcmp(logmsg[2],"disassociated:"))
		return;

	for (i=0;i<17;i++)
	{
		if (logmsg[3][i]>=97 && logmsg[3][i]<=122)
			logmsg[3][i]-=32;
	}
	system("cp /proc/net/arp /tmp/pnarp");
    arp_flag = fopen("/tmp/pnarp","r");
    if (arp_flag==NULL)
        return;
    while (fgets(buffer,sizeof(buffer),arp_flag))
    {
        if (strstr(buffer,logmsg[3])!=NULL)
        {
			get_flag=1;
            break;
        }
        else
            memset(buffer,0x00,sizeof(buffer));
    }
    fclose(arp_flag);
	if (get_flag)
	{
		sscanf(buffer,"%s %s %s %s %s %s",arptbl[0],arptbl[1],arptbl[2],arptbl[3],arptbl[4],arptbl[5]);
		deleteArpRule(arptbl[0]);
	}
}
#endif

#define log_buffer bb_common_bufsiz1
enum {
	KLOGD_LOGBUF_SIZE = sizeof(log_buffer),
	OPT_LEVEL      = (1 << 0),
	OPT_FOREGROUND = (1 << 1),
};

int klogd_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int klogd_main(int argc UNUSED_PARAM, char **argv)
{
	int i = 0;
	char *start;
	int opt;

	opt = getopt32(argv, "c:n", &start);
	if (opt & OPT_LEVEL) {
		/* Valid levels are between 1 and 8 */
		i = xatou_range(start, 1, 8);
	}
	if (!(opt & OPT_FOREGROUND)) {
		bb_daemonize_or_rexec(DAEMON_CHDIR_ROOT, argv);
	}

	openlog("kernel", 0, LOG_KERN);

	bb_signals(0
		+ (1 << SIGINT)
		+ (1 << SIGTERM)
		, klogd_signal);
	signal(SIGHUP, SIG_IGN);

	/* "Open the log. Currently a NOP" */
	klogctl(1, NULL, 0);

	/* "printk() prints a message on the console only if it has a loglevel
	 * less than console_loglevel". Here we set console_loglevel = i. */
	if (i)
		klogctl(8, NULL, i);

	syslog(LOG_NOTICE, "klogd started: %s", bb_banner);

	/* Note: this code does not detect incomplete messages
	 * (messages not ending with '\n' or just when kernel
	 * generates too many messages for us to keep up)
	 * and will split them in two separate lines */
	while (1) {
		int n;
		int priority;

		/* "2 -- Read from the log." */
		n = klogctl(2, log_buffer, KLOGD_LOGBUF_SIZE - 1);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			syslog(LOG_ERR, "klogd: error %d in klogctl(2): %m",
					errno);
			break;
		}
		log_buffer[n] = '\n';
		i = 0;
		while (i < n) {
			priority = LOG_INFO;
			start = &log_buffer[i];
			if (log_buffer[i] == '<') {
				i++;
				// kernel never ganerates multi-digit prios
				//priority = 0;
				//while (log_buffer[i] >= '0' && log_buffer[i] <= '9') {
				//	priority = priority * 10 + (log_buffer[i] - '0');
				//	i++;
				//}
				if (isdigit(log_buffer[i])) {
					priority = (log_buffer[i] - '0');
					i++;
				}
				if (log_buffer[i] == '>')
					i++;
				start = &log_buffer[i];
			}
			while (log_buffer[i] != '\n')
				i++;
			log_buffer[i] = '\0';
			{	/* Filter log */
					if( strstr(start,"IPFILTER")!= NULL )//LAN->WAN DROP REJECT ACCEPT
					{
						openlog("FILTER", LOG_ODELAY, LOG_USER);
						start = start + strlen("IPFILTER");
						syslog(LOG_INFO, "%s", start);
                        openlog("kernel", LOG_NDELAY, LOG_USER);
					}					
					else if( strstr(start,"FIREW")!= NULL )//NBT PING IDENT ACCESS
					{
						openlog("FIREWALL", LOG_ODELAY, LOG_USER);
						start = start + strlen("FIREW");
						syslog(LOG_INFO, "%s", start);
                        openlog("kernel", LOG_NDELAY, LOG_USER);
					}
					else if( strstr(start,"NAT")!= NULL )//Port forward
					{
						openlog("NAT", LOG_ODELAY, LOG_USER);
						start = start + strlen("NAT");
						syslog(LOG_INFO, "%s", start);
                        openlog("kernel", LOG_NDELAY, LOG_USER);
					}
					else if( strstr(start,"WIRELESS:")!= NULL )//WIRELESS
                    {
                        openlog("WIRELESS", LOG_ODELAY, LOG_USER);
                        start = start + strlen("WIRELESS:");
                        syslog(LOG_INFO, "%s", start);
                        openlog("kernel", LOG_NDELAY, LOG_USER);
						checkLogToDeleteArp(start);
                    }
					else if( strstr(start,"AUTH:")!= NULL )//WLAN AUTH
                    {
                        openlog("AUTH", LOG_ODELAY, LOG_USER);
                        start = start + strlen("AUTH:");
                        syslog(LOG_INFO, "%s", start);
                        openlog("kernel", LOG_NDELAY, LOG_USER);
                    }

			}/* Chris End */

			//syslog(priority, "%s", start);
			i++;
		}
	}

	return EXIT_FAILURE;
}
