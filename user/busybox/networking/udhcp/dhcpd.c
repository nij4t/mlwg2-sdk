/* vi: set sw=4 ts=4: */
/* dhcpd.c
 *
 * udhcp Server
 * Copyright (C) 1999 Matthew Ramsay <matthewr@moreton.com.au>
 *			Chris Trew <ctrew@moreton.com.au>
 *
 * Rewrite by Russ Dill <Russ.Dill@asu.edu> July 2001
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <syslog.h>
#include "common.h"
#include "dhcpc.h"
#include "dhcpd.h"
#include "options.h"

/* globals */
struct dhcpOfferedAddr *leases;
/* struct server_config_t server_config is in bb_common_bufsiz1 */


int udhcpd_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int udhcpd_main(int argc UNUSED_PARAM, char **argv)
{
	fd_set rfds;
	struct timeval tv;
	int server_socket = -1, bytes, retval, max_sock;
	struct dhcpMessage packet;
	uint8_t *state, *server_id, *requested;
	uint32_t server_id_align, requested_align, static_lease_ip;
	unsigned timeout_end;
	unsigned num_ips;
	unsigned opt;
	struct option_set *option;
	struct dhcpOfferedAddr *lease, static_lease;
	USE_FEATURE_UDHCP_PORT(char *str_P;)

#if ENABLE_FEATURE_UDHCP_PORT
	SERVER_PORT = 67;
	CLIENT_PORT = 68;
#endif

	opt = getopt32(argv, "fS" USE_FEATURE_UDHCP_PORT("P:", &str_P));
	argv += optind;

	if (!(opt & 1)) { /* no -f */
		bb_daemonize_or_rexec(0, argv);
		logmode &= ~LOGMODE_STDIO;
	}

	if (opt & 2) { /* -S */
		openlog(applet_name, LOG_PID, LOG_LOCAL0);
		logmode |= LOGMODE_SYSLOG;
	}
#if ENABLE_FEATURE_UDHCP_PORT
	if (opt & 4) { /* -P */
		SERVER_PORT = xatou16(str_P);
		CLIENT_PORT = SERVER_PORT + 1;
	}
#endif
	/* Would rather not do read_config before daemonization -
	 * otherwise NOMMU machines will parse config twice */
	read_config(argv[0] ? argv[0] : DHCPD_CONF_FILE);

	/* Make sure fd 0,1,2 are open */
	bb_sanitize_stdio();
	/* Equivalent of doing a fflush after every \n */
	setlinebuf(stdout);

	/* Create pidfile */
	write_pidfile(server_config.pidfile);
	/* if (!..) bb_perror_msg("cannot create pidfile %s", pidfile); */

#ifdef GT_NEW_LOG
	setLogLevel(_LOGDBG, "DHCPS");  /* set log level to debug */
	addLog(_LOGINFO, "udhcpd (v"BB_VER") started") ; /* log */
#else
	bb_info_msg("%s (v"BB_VER") started", applet_name);
#endif

	option = find_option(server_config.options, DHCP_LEASE_TIME);
	server_config.lease = LEASE_TIME;
	if (option) {
		memcpy(&server_config.lease, option->data + 2, 4);
		server_config.lease = ntohl(server_config.lease);
	}

	/* Sanity check */
	num_ips = server_config.end_ip - server_config.start_ip + 1;
	if (server_config.max_leases > num_ips) {
#ifdef GT_NEW_LOG
		{
			char log_info[128] = {'\0'};
			snprintf(log_info, sizeof(log_info), "max_leases=%u is too big, setting to %u",
            	(unsigned)server_config.max_leases, num_ips);
			addLog(_LOGINFO, log_info);
		}
#else
		bb_error_msg("max_leases=%u is too big, setting to %u",
			(unsigned)server_config.max_leases, num_ips);
#endif
		server_config.max_leases = num_ips;
	}

	leases = xzalloc(server_config.max_leases * sizeof(*leases));
	read_leases(server_config.lease_file);

	if (read_interface(server_config.interface, &server_config.ifindex,
			   &server_config.server, server_config.arp)) {
		retval = 1;
		goto ret;
	}

	/* Setup the signal pipe */
	udhcp_sp_setup();

	timeout_end = monotonic_sec() + server_config.auto_time;
	while (1) { /* loop until universe collapses */

		if (server_socket < 0) {
			server_socket = listen_socket(/*INADDR_ANY,*/ SERVER_PORT,
					server_config.interface);
		}

		max_sock = udhcp_sp_fd_set(&rfds, server_socket);
		if (server_config.auto_time) {
			tv.tv_sec = timeout_end - monotonic_sec();
			tv.tv_usec = 0;
		}
		retval = 0;
		if (!server_config.auto_time || tv.tv_sec > 0) {
			retval = select(max_sock + 1, &rfds, NULL, NULL,
					server_config.auto_time ? &tv : NULL);
		}
		if (retval == 0) {
			write_leases();
			timeout_end = monotonic_sec() + server_config.auto_time;
			continue;
		}
		if (retval < 0 && errno != EINTR) {
			DEBUG("error on select");
			continue;
		}

		switch (udhcp_sp_read(&rfds)) {
		case SIGUSR1:
#ifdef GT_NEW_LOG
			addLog(_LOGINFO,"Received a SIGUSR1.") ; /* log */
#else
			bb_info_msg("Received a SIGUSR1");
#endif
			write_leases();
			/* why not just reset the timeout, eh */
			timeout_end = monotonic_sec() + server_config.auto_time;
			continue;
		case SIGTERM:
#ifdef GT_NEW_LOG
			addLog(_LOGINFO,"Received a SIGTERM.") ; /* log */
			addLog(_LOGINFO,"Exiting: DHCP Server Stopped...") ; /* log */
#else
			bb_info_msg("Received a SIGTERM");
#endif
			goto ret0;
		case 0: break;		/* no signal */
		default: continue;	/* signal or error (probably EINTR) */
		}

		bytes = udhcp_recv_kernel_packet(&packet, server_socket); /* this waits for a packet - idle */
		if (bytes < 0) {
			if (bytes == -1 && errno != EINTR) {
				DEBUG("error on read, %s, reopening socket", strerror(errno));
				close(server_socket);
				server_socket = -1;
			}
			continue;
		}

		state = get_option(&packet, DHCP_MESSAGE_TYPE);
		if (state == NULL) {
			bb_error_msg("cannot get option from packet, ignoring");
			continue;
		}

		/* Look for a static lease */
		static_lease_ip = getIpByMac(server_config.static_leases, &packet.chaddr);

		if (static_lease_ip
#ifdef GT_NOT_LEASE_IP
		/* Jayo 2009/08/27 : Judge IP is not lease ip or not*/
		&& !is_not_lease_ip(ntohl(static_lease_ip))
#endif
		) {
			bb_info_msg("Found static lease: %x", static_lease_ip);

			memcpy(&static_lease.chaddr, &packet.chaddr, 16);
			static_lease.yiaddr = static_lease_ip;
			static_lease.expires = 0;

			lease = &static_lease;
		} else {
			lease = find_lease_by_chaddr(packet.chaddr);
		}

		switch (state[0]) {
		case DHCPDISCOVER:
#ifdef GT_NEW_LOG
			addLog(_LOGINFO,"Received DISCOVER...") ; /* log */
#else
			DEBUG("Received DISCOVER");
#endif

			if (send_offer(&packet) < 0) {
				bb_error_msg("send OFFER failed");
			}
			break;
		case DHCPREQUEST:
#ifdef GT_NEW_LOG
			addLog(_LOGINFO,"Received REQUEST...") ; /* log */
#else
			DEBUG("received REQUEST");
#endif

			requested = get_option(&packet, DHCP_REQUESTED_IP);
			server_id = get_option(&packet, DHCP_SERVER_ID);

			if (requested) memcpy(&requested_align, requested, 4);
			if (server_id) memcpy(&server_id_align, server_id, 4);

			if (lease) {
				if (server_id) {
					/* SELECTING State */
					DEBUG("server_id = %08x", ntohl(server_id_align));
					if (server_id_align == server_config.server && requested
					 && requested_align == lease->yiaddr
					) {
						send_ACK(&packet, lease->yiaddr);
					}
				} else if (requested) {
					/* INIT-REBOOT State */
					if (lease->yiaddr == requested_align)
						send_ACK(&packet, lease->yiaddr);
					else
						send_NAK(&packet);
				} else if (lease->yiaddr == packet.ciaddr) {
					/* RENEWING or REBINDING State */
					send_ACK(&packet, lease->yiaddr);
				} else { /* don't know what to do!!!! */
					send_NAK(&packet);
				}

			/* what to do if we have no record of the client */
			} else if (server_id) {
				/* SELECTING State */

			} else if (requested) {
				/* INIT-REBOOT State */
				lease = find_lease_by_yiaddr(requested_align);
				if (lease) {
					if (lease_expired(lease)) {
						/* probably best if we drop this lease */
						memset(lease->chaddr, 0, 16);
					/* make some contention for this address */
					} else
						send_NAK(&packet);
				} else {
					uint32_t r = ntohl(requested_align);
					if (r < server_config.start_ip
				         || r > server_config.end_ip
					) {
						send_NAK(&packet);
					}
					/* else remain silent */
					/* Jayo 2009/09/18: avoid the problem that dhcp client's request time is too long */
					else
						send_NAK(&packet);
					/* Jayo 2009/09/18 End */
				}

			} else {
				/* RENEWING or REBINDING State */
			}
			break;
		case DHCPDECLINE:
#ifdef GT_NEW_LOG
			addLog(_LOGINFO,"Received DECLINE...") ; /* log */
#else
			DEBUG("Received DECLINE");
#endif
			if (lease) {
				memset(lease->chaddr, 0, 16);
				lease->expires = time(0) + server_config.decline_time;
			}
			break;
		case DHCPRELEASE:
#ifdef GT_NEW_LOG
			addLog(_LOGINFO,"Received RELEASE...") ; /* log */
#else
			DEBUG("Received RELEASE");
#endif
			if (lease)
			{
				lease->expires = time(0);
				/* Jayo 2009/10/29: update lease file when receiving RELEASE packet */
				write_leases();
			}
			break;
		case DHCPINFORM:
#ifdef GT_NEW_LOG
			addLog(_LOGINFO,"Received INFORM...") ; /* log */
#else
			DEBUG("Received INFORM");
#endif
			send_inform(&packet);
			break;
		default:
			bb_info_msg("Unsupported DHCP message (%02x) - ignoring", state[0]);
		}
	}
 ret0:
	retval = 0;
 ret:
	/*if (server_config.pidfile) - server_config.pidfile is never NULL */
		remove_pidfile(server_config.pidfile);
#ifdef GT_NEW_LOG
	addLog(_LOGINFO,"Exiting: DHCP Server Stopped...") ; /* log */
#endif
	return retval;
}
