/* vi: set sw=4 ts=4: */
/* serverpacket.c
 *
 * Construct and send DHCP server packets
 *
 * Russ Dill <Russ.Dill@asu.edu> July 2001
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "common.h"
#include "dhcpc.h"
#include "dhcpd.h"
#include "options.h"


/* send a packet to giaddr using the kernel ip stack */
static int send_packet_to_relay(struct dhcpMessage *payload)
{
	DEBUG("Forwarding packet to relay");

	return udhcp_send_kernel_packet(payload, server_config.server, SERVER_PORT,
			payload->giaddr, SERVER_PORT);
}


/* send a packet to a specific arp address and ip address by creating our own ip packet */
static int send_packet_to_client(struct dhcpMessage *payload, int force_broadcast)
{
	const uint8_t *chaddr;
	uint32_t ciaddr;

	if (force_broadcast) {
		DEBUG("broadcasting packet to client (NAK)");
		ciaddr = INADDR_BROADCAST;
		chaddr = MAC_BCAST_ADDR;
	} else if (ntohs(payload->flags) & BROADCAST_FLAG) {
		DEBUG("broadcasting packet to client (requested)");
		ciaddr = INADDR_BROADCAST;
		chaddr = MAC_BCAST_ADDR;
	} else if (payload->ciaddr) {
		DEBUG("unicasting packet to client ciaddr");
		ciaddr = payload->ciaddr;
		chaddr = payload->chaddr;
	} else {
		DEBUG("unicasting packet to client yiaddr");
		ciaddr = payload->yiaddr;
		chaddr = payload->chaddr;
	}
	return udhcp_send_raw_packet(payload, server_config.server, SERVER_PORT,
			ciaddr, CLIENT_PORT, chaddr, server_config.ifindex);
}


/* send a dhcp packet, if force broadcast is set, the packet will be broadcast to the client */
static int send_packet(struct dhcpMessage *payload, int force_broadcast)
{
	if (payload->giaddr)
		return send_packet_to_relay(payload);
	return send_packet_to_client(payload, force_broadcast);
}


static void init_packet(struct dhcpMessage *packet, struct dhcpMessage *oldpacket, char type)
{
	udhcp_init_header(packet, type);
	packet->xid = oldpacket->xid;
	memcpy(packet->chaddr, oldpacket->chaddr, 16);
	packet->flags = oldpacket->flags;
	packet->giaddr = oldpacket->giaddr;
	packet->ciaddr = oldpacket->ciaddr;
	add_simple_option(packet->options, DHCP_SERVER_ID, server_config.server);
}


/* add in the bootp options */
static void add_bootp_options(struct dhcpMessage *packet)
{
	packet->siaddr = server_config.siaddr;
	if (server_config.sname)
		strncpy((char*)packet->sname, server_config.sname, sizeof(packet->sname) - 1);
	if (server_config.boot_file)
		strncpy((char*)packet->file, server_config.boot_file, sizeof(packet->file) - 1);
}


/* send a DHCP OFFER to a DHCP DISCOVER */
int send_offer(struct dhcpMessage *oldpacket)
{
	struct dhcpMessage packet;
	struct dhcpOfferedAddr *lease = NULL;
	uint32_t req_align, lease_time_align = server_config.lease;
	uint8_t *req, *lease_time;
	struct option_set *curr;
	struct in_addr addr;

	uint32_t static_lease_ip;

	init_packet(&packet, oldpacket, DHCPOFFER);

	static_lease_ip = getIpByMac(server_config.static_leases, oldpacket->chaddr);

#ifdef GT_NOT_LEASE_IP
	/* Jayo 2009/08/27  : Judge IP is not lease ip or not */
	if(is_not_lease_ip(ntohl(static_lease_ip)))
		static_lease_ip = 0;
#endif

	/* ADDME: if static, short circuit */
	if (!static_lease_ip) {
		/* the client is in our lease/offered table */
		lease = find_lease_by_chaddr(oldpacket->chaddr);
		if (lease
#ifdef GT_NOT_LEASE_IP
		 && !is_not_lease_ip(ntohl(lease->yiaddr))
#endif
		) {
			if (!lease_expired(lease))
				lease_time_align = lease->expires - time(0);
			packet.yiaddr = lease->yiaddr;
		/* Or the client has a requested ip */
		} else if ((req = get_option(oldpacket, DHCP_REQUESTED_IP))
		 /* Don't look here (ugly hackish thing to do) */
		 && memcpy(&req_align, req, 4)
#ifdef GT_NOT_LEASE_IP
		 && !is_not_lease_ip(ntohl(req_align))
#endif
		 /* and the ip is in the lease range */
		 && ntohl(req_align) >= server_config.start_ip
		 && ntohl(req_align) <= server_config.end_ip
		 && !static_lease_ip /* Check that its not a static lease */
		 /* and is not already taken/offered */
		 && (!(lease = find_lease_by_yiaddr(req_align))
			/* or its taken, but expired */ /* ADDME: or maybe in here */
			|| lease_expired(lease))
		) {
			packet.yiaddr = req_align; /* FIXME: oh my, is there a host using this IP? */
			/* otherwise, find a free IP */
		} else {
			/* Is it a static lease? (No, because find_address skips static lease) */
			packet.yiaddr = find_address(0);
			/* try for an expired lease */
			if (!packet.yiaddr)
				packet.yiaddr = find_address(1);
		}

		if (!packet.yiaddr) {
#ifdef GT_NEW_LOG
			addLog(_LOGINFO, "Warning, no IP addresses to give -- OFFER abandoned") ; /* log */
#else
			bb_error_msg("no IP addresses to give - OFFER abandoned");
#endif
			return -1;
		}
		if (!add_lease(get_option(oldpacket, DHCP_HOST_NAME), packet.chaddr, packet.yiaddr, server_config.offer_time, NULL)) { //Jimmy
#ifdef GT_NEW_LOG
			addLog(_LOGINFO, "Warning, lease pool is full -- OFFER abandoned") ; /* log */
#else
			bb_error_msg("lease pool is full - OFFER abandoned");
#endif
			return -1;
		}
		lease_time = get_option(oldpacket, DHCP_LEASE_TIME);
		if (lease_time) {
			memcpy(&lease_time_align, lease_time, 4);
			lease_time_align = ntohl(lease_time_align);
			if (lease_time_align > server_config.lease)
				lease_time_align = server_config.lease;
		}

		/* Make sure we aren't just using the lease time from the previous offer */
		if (lease_time_align < server_config.min_lease)
			lease_time_align = server_config.lease;
		/* ADDME: end of short circuit */
	} else {
		/* It is a static lease... use it */
		packet.yiaddr = static_lease_ip;
	}

	add_simple_option(packet.options, DHCP_LEASE_TIME, htonl(lease_time_align));

	curr = server_config.options;
	while (curr) {
		if (curr->data[OPT_CODE] != DHCP_LEASE_TIME)
			add_option_string(packet.options, curr->data);
		curr = curr->next;
	}

	add_bootp_options(&packet);

	addr.s_addr = packet.yiaddr;
#ifdef GT_NEW_LOG
	{
        char log_info[128] = {'\0'};
        snprintf(log_info, sizeof(log_info), "Sending OFFER of %s", inet_ntoa(addr));
        addLog(_LOGINFO, log_info) ; /* log */
	}
#else
	bb_info_msg("Sending OFFER of %s", inet_ntoa(addr));
#endif
	return send_packet(&packet, 0);
}


int send_NAK(struct dhcpMessage *oldpacket)
{
	struct dhcpMessage packet;

	init_packet(&packet, oldpacket, DHCPNAK);

#ifdef GT_NEW_LOG
	addLog(_LOGINFO, "Sending NAK...") ; /* log */
#else
	DEBUG("Sending NAK");
#endif
	return send_packet(&packet, 1);
}


int send_ACK(struct dhcpMessage *oldpacket, uint32_t yiaddr)
{
	struct dhcpMessage packet;
	struct option_set *curr;
	uint8_t *lease_time;
	uint32_t lease_time_align = server_config.lease;
	uint32_t static_lease_ip;
	struct in_addr addr;
    // 20070328 Jimmy add below: for dump dhcp option12 & 60
    uint8_t vendor_class_identifier[255];
    uint8_t *vendor_ptr;
    unsigned int vendor_len;
    // 20070328 Jimmy add above

	init_packet(&packet, oldpacket, DHCPACK);
	packet.yiaddr = yiaddr;

	lease_time = get_option(oldpacket, DHCP_LEASE_TIME);
	if (lease_time) {
		memcpy(&lease_time_align, lease_time, 4);
		lease_time_align = ntohl(lease_time_align);
		if (lease_time_align > server_config.lease)
			lease_time_align = server_config.lease;
		else if (lease_time_align < server_config.min_lease)
			lease_time_align = server_config.lease;
	}

	add_simple_option(packet.options, DHCP_LEASE_TIME, htonl(lease_time_align));

	curr = server_config.options;
	while (curr) {
		if (curr->data[OPT_CODE] != DHCP_LEASE_TIME)
			add_option_string(packet.options, curr->data);
		curr = curr->next;
	}

	add_bootp_options(&packet);

	addr.s_addr = packet.yiaddr;

#ifdef GT_NEW_LOG
	{
		char log_info[128] = {'\0'};
		snprintf(log_info, sizeof(log_info),"Sending ACK to %s", inet_ntoa(addr));
		addLog(_LOGINFO, log_info) ; /* log */
	}
#else
	bb_info_msg("Sending ACK to %s", inet_ntoa(addr));
#endif

	if (send_packet(&packet, 0) < 0)
		return -1;

    // 20070328 Jimmy modify below: for dump dhcp option12 & 60

    memset(vendor_class_identifier, 0x00, sizeof(vendor_class_identifier));
    vendor_ptr = get_option(oldpacket, DHCP_VENDOR);

    /* Modified by Ben 960413 -> */
    if(vendor_ptr != NULL) {
            printf("#####vendor = %s\n", vendor_ptr);
            vendor_len = *(vendor_ptr-1);
            printf("#####vendor_len = %d\n", vendor_len);
            strncpy(vendor_class_identifier, vendor_ptr, vendor_len);
            printf("#####vendor_string = %s\n", vendor_class_identifier);
    }
    else {
            strcpy(vendor_class_identifier, "UNKNOWN");
            printf("#####vendor_string = %s\n", vendor_class_identifier);

    }
    /*              <- Ben 960413 */

	add_lease(get_option(oldpacket, DHCP_HOST_NAME), packet.chaddr, packet.yiaddr, lease_time_align, vendor_class_identifier);
	if (ENABLE_FEATURE_UDHCPD_WRITE_LEASES_EARLY) {
		/* rewrite the file with leases at every new acceptance */
		static_lease_ip = getIpByMac(server_config.static_leases, &packet.chaddr);
		if(!static_lease_ip)
		  write_leases();//Not include static IP
		write_leases_forClientMonitor();//Include static and DHCP IP
	}
    // 20070328 Jimmy modify above

	return 0;
}


int send_inform(struct dhcpMessage *oldpacket)
{
	struct dhcpMessage packet;
	struct option_set *curr;

	init_packet(&packet, oldpacket, DHCPACK);

	curr = server_config.options;
	while (curr) {
		if (curr->data[OPT_CODE] != DHCP_LEASE_TIME)
			add_option_string(packet.options, curr->data);
		curr = curr->next;
	}

	add_bootp_options(&packet);

#ifdef GT_NEW_LOG
	{
        char log_info[128] = {'\0'};
		struct in_addr addr;
		addr.s_addr = packet.yiaddr;
        snprintf(log_info, sizeof(log_info), "Sending INFORM to %s", inet_ntoa(addr));
        addLog(_LOGINFO, log_info) ; /* log */
	}
#endif

	return send_packet(&packet, 0);
}
