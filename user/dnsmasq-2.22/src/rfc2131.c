/* dnsmasq is Copyright (c) 2000-2005 Simon Kelley

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 dated June, 1991.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
*/

/* Author's email: simon@thekelleys.org.uk */

#include "dnsmasq.h"

#define BOOTREQUEST              1
#define BOOTREPLY                2
#define DHCP_COOKIE              0x63825363

/* The Linux in-kernel DHCP client silently ignores any packet 
   smaller than this. Sigh...........   */
#define MIN_PACKETSZ             300

#define OPTION_PAD               0
#define OPTION_NETMASK           1
#define OPTION_ROUTER            3
#define OPTION_DNSSERVER         6
#define OPTION_HOSTNAME          12
#define OPTION_DOMAINNAME        15
#define OPTION_BROADCAST         28
#define OPTION_VENDOR_CLASS_OPT  43
#define OPTION_REQUESTED_IP      50 
#define OPTION_LEASE_TIME        51
#define OPTION_OVERLOAD          52
#define OPTION_MESSAGE_TYPE      53
#define OPTION_SERVER_IDENTIFIER 54
#define OPTION_REQUESTED_OPTIONS 55
#define OPTION_MESSAGE           56
#define OPTION_MAXMESSAGE        57
#define OPTION_T1                58
#define OPTION_T2                59
#define OPTION_VENDOR_ID         60
#define OPTION_CLIENT_ID         61
#define OPTION_USER_CLASS        77
#define OPTION_SUBNET_SELECT     118
#define OPTION_END               255

#define DHCPDISCOVER             1
#define DHCPOFFER                2
#define DHCPREQUEST              3
#define DHCPDECLINE              4
#define DHCPACK                  5
#define DHCPNAK                  6
#define DHCPRELEASE              7
#define DHCPINFORM               8

#define have_config(config, mask) ((config) && ((config)->flags & (mask))) 

static unsigned char *option_put(unsigned char *p, unsigned char *end, int opt, int len, unsigned int val);
static unsigned char *option_end(unsigned char *p, unsigned char *end, struct dhcp_packet *start);
static unsigned char *option_put_string(unsigned char *p, unsigned char *end, int opt, char *string);
static void bootp_option_put(struct dhcp_packet *mess, 
			     struct dhcp_boot *boot_opts, struct dhcp_netid *netids);
static int option_len(unsigned char *opt);
static void *option_ptr(unsigned char *opt);
static struct in_addr option_addr(unsigned char *opt);
static unsigned int option_uint(unsigned char *opt, int size);
static void log_packet(char *type, struct in_addr *addr, unsigned char *hwaddr, char *interface, char *string);
static unsigned char *option_find(struct dhcp_packet *mess, int size, int opt_type, int minsize);
static unsigned char *do_req_options(struct dhcp_context *context,
				     unsigned char *p, unsigned char *end, 
				     unsigned char *req_options, 
				     struct daemon *daemon,
				     char *hostname,
				     struct dhcp_netid *netid,
				     struct in_addr subnet_addr);

int dhcp_reply(struct daemon *daemon, struct dhcp_context *context, char *iface_name, unsigned int sz, time_t now)
{
  struct dhcp_context *context_tmp;
  unsigned char *opt, *clid = NULL;
  struct dhcp_lease *ltmp, *lease = NULL;
  struct dhcp_vendor *vendor;
  struct dhcp_netid_list *id_list;
  int clid_len = 0, ignore = 0;
  struct dhcp_packet *mess = &daemon->dhcp_packet->data;
  unsigned char *p = mess->options + sizeof(u32); /* skip cookie */
  unsigned char *end = (unsigned char *)(daemon->dhcp_packet + 1);
  char *hostname = NULL;
  char *req_options = NULL;
  char *message = NULL;
  unsigned int time;
  struct dhcp_config *config;
  struct dhcp_netid *netid = NULL;
  struct in_addr addr, subnet_addr;
  unsigned short fuzz = 0;
  unsigned int mess_type = 0;

  subnet_addr.s_addr = 0;

  if (mess->op != BOOTREQUEST)
    return 0;
  
  /* Token ring is supported when we have packet sockets
     to make the HW headers for us. We don't have the code to build
     token ring headers when using BPF. We rely on the fact that
     token ring hwaddrs are the same size as ethernet hwaddrs. */
  
#ifdef HAVE_BPF
  if (mess->htype != ARPHRD_ETHER)	
#else
  if (mess->htype != ARPHRD_ETHER && mess->htype != ARPHRD_IEEE802)
#endif
    {
      syslog(LOG_WARNING, "DHCP request for unsupported hardware type (%d) recieved on %s", 
	     mess->htype, iface_name);
      return 0;
    }	
  
  if (mess->hlen != ETHER_ADDR_LEN)
    return 0;
  
  /* check for DHCP rather than BOOTP */
  if ((opt = option_find(mess, sz, OPTION_MESSAGE_TYPE, 1)))
    {
      mess_type = option_uint(opt, 1);

      /* only insist on a cookie for DHCP. */
      if (*((u32 *)&mess->options) != htonl(DHCP_COOKIE))
	return 0;

      /* Some buggy clients set ciaddr when they shouldn't, so clear that here since
	 it can affect the context-determination code. */
      if ((option_find(mess, sz, OPTION_REQUESTED_IP, INADDRSZ) || mess_type == DHCPDISCOVER))
	mess->ciaddr.s_addr = 0;

      /* Check for RFC3011 subnet selector */
      if ((opt = option_find(mess, sz, OPTION_SUBNET_SELECT, INADDRSZ)))
	subnet_addr = option_addr(opt);
      
      /* If there is no client identifier option, use the hardware address */
      if ((opt = option_find(mess, sz, OPTION_CLIENT_ID, 1)))
	{
	  clid_len = option_len(opt);
	  clid = option_ptr(opt);
	}
      
      /* do we have a lease in store? */
      lease = lease_find_by_client(mess->chaddr, clid, clid_len);

      /* If this request is missing a clid, but we've seen one before, 
	 use it again for option matching etc. */
      if (lease && !clid && lease->clid)
	{
	  clid_len = lease->clid_len;
	  clid = lease->clid;
	}
    }
  
  /* Determine network for this packet. Our caller will have already linked all the 
     contexts which match the addresses of the receiving interface but if the 
     machine has an address already, or came via a relay, or we have a subnet selector, 
     we search again. If we don't have have a giaddr or explicit subnet selector, 
     use the ciaddr. This is necessary because a  machine which got a lease via a 
     relay won't use the relay to renew. */
  if (mess->giaddr.s_addr || subnet_addr.s_addr || mess->ciaddr.s_addr)
    {
      addr = 
	subnet_addr.s_addr ? subnet_addr : 
	(mess->giaddr.s_addr ? mess->giaddr : mess->ciaddr);
      
      for (context = NULL, context_tmp = daemon->dhcp; context_tmp; context_tmp = context_tmp->next)
	if (context_tmp->netmask.s_addr  && 
	    is_same_net(addr, context_tmp->start, context_tmp->netmask) &&
	    is_same_net(addr, context_tmp->end, context_tmp->netmask))
	  {
	    context_tmp->current = context;
	    context = context_tmp;
	  }
    }
  
  if (!context)
    {
      syslog(LOG_WARNING, "no address range available for DHCP request %s %s", 
             subnet_addr.s_addr ? "with subnet selector" : "via",
             subnet_addr.s_addr ? inet_ntoa(subnet_addr) : (mess->giaddr.s_addr ? inet_ntoa(mess->giaddr) : iface_name));
      return 0;
    }
  
  mess->op = BOOTREPLY;
    
  config = find_config(daemon->dhcp_conf, context, clid, clid_len, mess->chaddr, NULL);
  
  if (mess_type == 0)
    {
      /* BOOTP request */
      struct dhcp_netid id;
      char save = mess->file[127];	// Ben, Coverity Run: 62 (2009-11-09 20:45) CID: 4443
      struct in_addr *logaddr = NULL;
      
      if (have_config(config, CONFIG_ADDR))
	{
	  logaddr = &config->addr;
	  mess->yiaddr = config->addr;
	  if (lease_find_by_addr(config->addr))
	    message = "address in use";
	  context = narrow_context(context, config->addr);
	}
      else
	message = "no address configured";

      if (have_config(config, CONFIG_DISABLE))
	message = "disabled";

      end = mess->options + 64; /* BOOTP vend area is only 64 bytes */
            
      if (have_config(config, CONFIG_NAME))
	hostname = config->hostname;
      
      if (context->netid.net && !(context->flags & CONTEXT_FILTER))
	{
	  context->netid.next = netid;
	  netid = &context->netid;
	}
      
      if (have_config(config, CONFIG_NETID))
	{
	  config->netid.next = netid;
	  netid = &config->netid;
	}

      /* Match incoming filename field as a netid. */
      if (mess->file[0])
	{
	  mess->file[127] = 0; /* ensure zero term. */	// Ben, Coverity Run: 62 (2009-11-09 20:45) CID: 4443
	  id.net = mess->file;
	  id.next = netid;
	  netid = &id;
	}
      
      for (id_list = daemon->dhcp_ignore; id_list; id_list = id_list->next)
	if (match_netid(id_list->list, netid))
	  message = "disabled";
      
      p = do_req_options(context, p, end, NULL, daemon, 
			 hostname, netid, subnet_addr);
      /* must do this after do_req_options since it overwrites filename field. */
      mess->siaddr = context->local;
      bootp_option_put(mess, daemon->boot_config, netid);
      p = option_end(p, end, mess);
      log_packet(NULL, logaddr, mess->chaddr, iface_name, message);
      mess->file[128] = save;

      if (message)
	return 0;
      else
	return p - (unsigned char *)mess; 
    }
  
  if (have_config(config, CONFIG_NAME))
    hostname = config->hostname;
  else if ((opt = option_find(mess, sz, OPTION_HOSTNAME, 1)))
    {
      int len = option_len(opt);
      hostname = daemon->dhcp_buff;
      memcpy(hostname, option_ptr(opt), len);
      /* May not be zero terminated */
      hostname[len] = 0;
      /* ensure there are no strange chars in there */
      if (!canonicalise(hostname))
	hostname = NULL;
      else if ((hostname = strip_hostname(daemon, hostname)) && !config)
	{
	  /* Search again now we have a hostname. 
	     Only accept configs without CLID and HWADDR here, (they won't match)
	     to avoid impersonation by name. */
	  struct dhcp_config *new = find_config(daemon->dhcp_conf, context, NULL, 0, mess->chaddr, hostname);
	  if (!have_config(new, CONFIG_CLID) && !have_config(new, CONFIG_HWADDR))
	    config = new;
	}
    }
  
  if (have_config(config, CONFIG_NETID))
    {
      config->netid.next = netid;
      netid = &config->netid;
    }
  
  /* user-class options are, according to RFC3004, supposed to contain
     a set of counted strings. Here we check that this is so (by seeing
     if the counts are consistent with the overall option length) and if
     so zero the counts so that we don't get spurious matches between 
     the vendor string and the counts. If the lengths don't add up, we
     assume that the option is a single string and non RFC3004 compliant 
     and just do the substring match. dhclient provides these broken options. */

  if ((opt = option_find(mess, sz, OPTION_USER_CLASS, 1)))
    {
      unsigned char *ucp = option_ptr(opt);
      int tmp, j;
      for (j = 0; j < option_len(opt); j += ucp[j] + 1);
      if (j == option_len(opt))
	for (j = 0; j < option_len(opt); j = tmp)
	  {
	    tmp = j + ucp[j] + 1;
	    ucp[j] = 0;
	  }
    }
  
  for (vendor = daemon->dhcp_vendors; vendor; vendor = vendor->next)
    if ((opt = option_find(mess, sz, vendor->is_vendor ? OPTION_VENDOR_ID : OPTION_USER_CLASS, 1)))
      {
	int i;
	for (i = 0; i <= (option_len(opt) - vendor->len); i++)
	  if (memcmp(vendor->data, option_ptr(opt)+i, vendor->len) == 0)
	    {
	      vendor->netid.next = netid;
	      netid = &vendor->netid;
	      break;
	    }
      }

  /* if all the netids in the ignore list are present, ignore this client */
  for (id_list = daemon->dhcp_ignore; id_list; id_list = id_list->next)
    if (match_netid(id_list->list, netid))
      ignore = 1;
  
  /* Can have setting to ignore the client ID for a particular MAC address or hostname */
  if (have_config(config, CONFIG_NOCLID))
    clid = NULL;
          
  if ((opt = option_find(mess, sz, OPTION_REQUESTED_OPTIONS, 0)))
    {
      req_options = daemon->dhcp_buff2;
      memcpy(req_options, option_ptr(opt), option_len(opt));
      req_options[option_len(opt)] = OPTION_END;
    }
  
  switch (mess_type)
    {
    case DHCPDECLINE:
      if (!(opt = option_find(mess, sz, OPTION_SERVER_IDENTIFIER, INADDRSZ)) ||
	  (context->local.s_addr != option_addr(opt).s_addr))
	return 0;

      /* sanitise any message. Paranoid? Moi? */
      if ((opt = option_find(mess, sz, OPTION_MESSAGE, 1)))
	{ 
	  char *p = option_ptr(opt), *q = daemon->dhcp_buff;
	  int i;
	  
	  for (i = option_len(opt); i > 0; i--)
	    {
	      char c = *p++;
	      if (isprint(c))
		*q++ = c;
	    }
	  *q++ = 0; /* add terminator */
	  message = daemon->dhcp_buff;
	}
      
      if (!(opt = option_find(mess, sz, OPTION_REQUESTED_IP, INADDRSZ)))
	return 0;
      
      log_packet("DECLINE", option_ptr(opt), mess->chaddr, iface_name, message);
      
      if (lease && lease->addr.s_addr == option_addr(opt).s_addr)
	lease_prune(lease, now);
      
      if (have_config(config, CONFIG_ADDR) && 
	  config->addr.s_addr == option_addr(opt).s_addr)
	{
	  syslog(LOG_WARNING, "disabling DHCP static address %s", inet_ntoa(config->addr));
	  config->flags &= ~CONFIG_ADDR ;
	}
      else
	/* make sure this host gets a different address next time. */
	for (; context; context = context->current)
	  context->addr_epoch++;
      
      return 0;

    case DHCPRELEASE:
      if (!(opt = option_find(mess, sz, OPTION_SERVER_IDENTIFIER, INADDRSZ)) ||
	  (context->local.s_addr != option_addr(opt).s_addr))
	return 0;
      
      if (lease && lease->addr.s_addr == mess->ciaddr.s_addr)
	lease_prune(lease, now);
      else
	message = "unknown lease";

      log_packet("RELEASE", &mess->ciaddr, mess->chaddr, iface_name, message);
	
      return 0;
      
    case DHCPDISCOVER:
      if ((opt = option_find(mess, sz, OPTION_REQUESTED_IP, INADDRSZ)))	 
	addr = option_addr(opt);
      if (ignore || have_config(config, CONFIG_DISABLE))
	message = "ignored";
      else if (have_config(config, CONFIG_ADDR) && 
               (!(ltmp = lease_find_by_addr(config->addr)) || ltmp == lease))
	mess->yiaddr = config->addr;
      else if (lease && address_available(context, lease->addr))
	mess->yiaddr = lease->addr;
      else if (opt && address_available(context, addr) && !lease_find_by_addr(addr) && 
	       !config_find_by_address(daemon->dhcp_conf, addr))
	mess->yiaddr = addr;
      else if (!address_allocate(context, daemon, &mess->yiaddr, mess->chaddr, netid))
	message = "no address available";      
      log_packet("DISCOVER", opt ? &addr : NULL, mess->chaddr, iface_name, message);          
      
      if (message)
	return 0;
      
      context = narrow_context(context, mess->yiaddr);
      if (context->netid.net && !(context->flags & CONTEXT_FILTER))
	{
	  context->netid.next = netid;
	  netid = &context->netid;
	}
       
      time = have_config(config, CONFIG_TIME) ? config->lease_time : context->lease_time;
      if ((opt = option_find(mess, sz, OPTION_LEASE_TIME, 4)))
	{
	  unsigned int req_time = option_uint(opt, 4);
	  if (time == 0xffffffff || (req_time != 0xffffffff && req_time < time))
	    time = req_time;
	}
      else if (lease && lease->expires != 0)
	time = (unsigned int)difftime(lease->expires, now);

      mess->siaddr = context->local;
      bootp_option_put(mess, daemon->boot_config, netid);
      p = option_put(p, end, OPTION_MESSAGE_TYPE, 1, DHCPOFFER);
      p = option_put(p, end, OPTION_SERVER_IDENTIFIER, INADDRSZ, ntohl(context->local.s_addr));
      p = option_put(p, end, OPTION_LEASE_TIME, 4, time);
      /* T1 and T2 are required in DHCPOFFER by HP's wacky Jetdirect client. */
      if (time != 0xffffffff)
	{
	  p = option_put(p, end, OPTION_T1, 4, (time/2));
	  p = option_put(p, end, OPTION_T2, 4, (time*7)/8);
	}
      p = do_req_options(context, p, end, req_options, daemon, 
			 NULL, netid, subnet_addr);
      p = option_end(p, end, mess);
      
      log_packet("OFFER" , &mess->yiaddr, mess->chaddr, iface_name, NULL);
      return p - (unsigned char *)mess;
      
    case DHCPREQUEST:
      if (ignore || have_config(config, CONFIG_DISABLE))
	return 0;
      if ((opt = option_find(mess, sz, OPTION_REQUESTED_IP, INADDRSZ)))
	{
	  /* SELECTING  or INIT_REBOOT */
	  mess->yiaddr = option_addr(opt);
	  
	  if ((opt = option_find(mess, sz, OPTION_SERVER_IDENTIFIER, INADDRSZ)))
	    {
	      /* SELECTING */
	      if (context->local.s_addr != option_addr(opt).s_addr)
		return 0;
	      
	      /* If a lease exists for this host and another address, squash it. */
	      if (lease && lease->addr.s_addr != mess->yiaddr.s_addr)
		{
		  lease_prune(lease, now);
		  lease = NULL;
		}
	    }
	  else
	    {
	      /* INIT-REBOOT */
	      if (!lease && !(daemon->options & OPT_AUTHORITATIVE))
		return 0;
	      
	      if (lease && lease->addr.s_addr != mess->yiaddr.s_addr)
		message = "wrong address";
	    }
	}
      else
	{
	  /* RENEWING or REBINDING */ 
	  /* Must exist a lease for this address */
	  if (!lease || mess->ciaddr.s_addr != lease->addr.s_addr)
	    message = "lease not found";
	  
	  /* desynchronise renewals */
	  fuzz = rand16();
	  mess->yiaddr = mess->ciaddr;
	}
      
      log_packet("REQUEST", &mess->yiaddr, mess->chaddr, iface_name, NULL);
      
      if (!message)
	{
	  struct dhcp_config *addr_config;

	  /* If a machine moves networks whilst it has a lease, we catch that here. */
	  if (!is_same_net(mess->yiaddr, context->start, context->netmask))
	    message = "wrong network";

	  /* Check for renewal of a lease which is outside the allowed range. */
	  else if (!address_available(context, mess->yiaddr) &&
		   (!have_config(config, CONFIG_ADDR) || config->addr.s_addr != mess->yiaddr.s_addr))
	    message = "address not available";

	  /* Check if a new static address has been configured. Be very sure that
	     when the client does DISCOVER, it will get the static address, otherwise
	     an endless protocol loop will ensue. */
	  
	  else if (have_config(config, CONFIG_ADDR) && 
		   config->addr.s_addr != mess->yiaddr.s_addr &&
		   (!(ltmp = lease_find_by_addr(config->addr)) || ltmp == lease))
	    message = "static lease available";

	  /* Check to see if the address is reserved as a static address for another host */
	  else if ((addr_config = config_find_by_address(daemon->dhcp_conf, mess->yiaddr)) && addr_config != config)
	    message = "address reserved";

	  else if ((ltmp = lease_find_by_addr(mess->yiaddr)) && ltmp != lease)
	    message = "address in use";
	  
	  else if (!lease && !(lease = lease_allocate(mess->chaddr, clid, clid_len, mess->yiaddr)))
	    message = "no leases left";
	}
      
      if (message)
	{
	  log_packet("NAK", &mess->yiaddr, mess->chaddr, iface_name, message);
	  
	  mess->siaddr.s_addr = mess->yiaddr.s_addr = mess->ciaddr.s_addr = 0;
	  bootp_option_put(mess, NULL, NULL);
	  p = option_put(p, end, OPTION_MESSAGE_TYPE, 1, DHCPNAK);
	  p = option_put_string(p, end, OPTION_MESSAGE, message);
	  mess->flags |= htons(0x8000); /* broadcast */
	}
      else
	{
	  log_packet("ACK", &mess->yiaddr, mess->chaddr, iface_name, hostname);
      
	  context = narrow_context(context, mess->yiaddr);
	  if (context->netid.net && !(context->flags & CONTEXT_FILTER))
	    {
	      context->netid.next = netid;
	      netid = &context->netid;
	    }
	
	  time = have_config(config, CONFIG_TIME) ? config->lease_time : context->lease_time;
	  if ((opt = option_find(mess, sz, OPTION_LEASE_TIME, 4)))
	    {
	      unsigned int req_time = option_uint(opt, 4);
	      if (time == 0xffffffff || (req_time != 0xffffffff && req_time < time))
		time = req_time;
	    }
	  
	  lease_set_hwaddr(lease, mess->chaddr, clid, clid_len);
	  if (!hostname)
	    hostname = host_from_dns(daemon, mess->yiaddr);
	  if (hostname)
	    lease_set_hostname(lease, hostname, daemon->domain_suffix);
	  lease_set_expires(lease, time == 0xffffffff ? 0 : now + (time_t)time);
	  	  
	  mess->siaddr = context->local;
	  bootp_option_put(mess, daemon->boot_config, netid);
	  p = option_put(p, end, OPTION_MESSAGE_TYPE, 1, DHCPACK);
	  p = option_put(p, end, OPTION_SERVER_IDENTIFIER, INADDRSZ, ntohl(context->local.s_addr));
	  p = option_put(p, end, OPTION_LEASE_TIME, 4, time);
	  if (time != 0xffffffff)
	    {
	      while (fuzz > (time/16))
		fuzz = fuzz/2; 
	      p = option_put(p, end, OPTION_T1, 4, (time/2) - fuzz);
	      p = option_put(p, end, OPTION_T2, 4, ((time * 7)/8) - fuzz);
	    }
	  p = do_req_options(context, p, end, req_options, daemon, 
			     hostname, netid, subnet_addr);
	}

      p = option_end(p, end, mess);
      return p - (unsigned char *)mess; 
      
    case DHCPINFORM:
      if (ignore || have_config(config, CONFIG_DISABLE))
	message = "ignored";
      
      log_packet("INFORM", &mess->ciaddr, mess->chaddr, iface_name, message);
     
      if (message || mess->ciaddr.s_addr == 0)
	return 0;
      
      context = narrow_context(context, mess->ciaddr);
      if (context->netid.net)
	{
	  context->netid.next = netid;
	  netid = &context->netid;
	}
       
      mess->siaddr = context->local;
      bootp_option_put(mess, daemon->boot_config, netid);
      p = option_put(p, end, OPTION_MESSAGE_TYPE, 1, DHCPACK);
      p = option_put(p, end, OPTION_SERVER_IDENTIFIER, INADDRSZ, ntohl(context->local.s_addr));
      if (!hostname)
	hostname = host_from_dns(daemon, mess->yiaddr);
      p = do_req_options(context, p, end, req_options, daemon, 
			 hostname, netid, subnet_addr);
      p = option_end(p, end, mess);
      
      log_packet("ACK", &mess->ciaddr, mess->chaddr, iface_name, hostname);
      return p - (unsigned char *)mess; 
    }
  
  return 0;
}

static void log_packet(char *type, struct in_addr *addr, unsigned char *hwaddr, char *interface, char *string)
{
  syslog(LOG_INFO, "%s%s(%s)%s%s %.2x:%.2x:%.2x:%.2x:%.2x:%.2x%s%s",
	 type ? "DHCP" : "BOOTP",
	 type ? type : "",
	 interface, 
	 addr ? " " : "",
	 addr ? inet_ntoa(*addr) : "",
	 hwaddr[0], hwaddr[1], hwaddr[2], hwaddr[3], hwaddr[4], hwaddr[5],
	 string ? " " : "",
	 string ? string : "");
}

static int option_len(unsigned char *opt)
{
  return opt[1];
}

static void *option_ptr(unsigned char *opt)
{
  return &opt[2];
}

static struct in_addr option_addr(unsigned char *opt)
{
  /* this worries about unaligned data in the option. */
  /* struct in_addr is network byte order */
  struct in_addr ret;

  memcpy(&ret, option_ptr(opt), INADDRSZ);

  return ret;
}

static unsigned int option_uint(unsigned char *opt, int size)
{
  /* this worries about unaligned data and byte order */
  unsigned int ret = 0;
  int i;
  unsigned char *p = option_ptr(opt);
  
  for (i = 0; i < size; i++)
    ret = (ret << 8) | *p++;

  return ret;
}

static void bootp_option_put(struct dhcp_packet *mess, 
			     struct dhcp_boot *boot_opts, struct dhcp_netid *netids)
{
  struct dhcp_boot *tmp;
  
  for (tmp = boot_opts; tmp; tmp = tmp->next)
    if (match_netid(tmp->netid, netids))
      break;
  if (!tmp)
    /* No match, look for one without a netid */
    for (tmp = boot_opts; tmp; tmp = tmp->next)
      if (!tmp->netid)
	break;

  /* Do this _after_ the matching above, since in 
     BOOTP mode, one if the things we match is the filename. */
  
  memset(mess->sname, 0, sizeof(mess->sname));
  memset(mess->file, 0, sizeof(mess->file));
  
  if (tmp)
    {
      if (tmp->sname)
	strncpy(mess->sname, tmp->sname, sizeof(mess->sname)-1);
      if (tmp->file)
	strncpy(mess->file, tmp->file, sizeof(mess->file)-1);
      if (tmp->next_server.s_addr)
	mess->siaddr = tmp->next_server;
    }
}

static unsigned char *option_put(unsigned char *p, unsigned char *end, int opt, int len, unsigned int val)
{
  int i;
  
  /* always keep one octet space for the END option. */ 
  if (p + len + 3 < end)
    {
      *(p++) = opt;
      *(p++) = len;
      
      for (i = 0; i < len; i++)
	*(p++) = val >> (8 * (len - (i + 1)));
    }
  return p;
}

static unsigned char *option_end(unsigned char *p, unsigned char *end, struct dhcp_packet *start)
{
  *(p++) = OPTION_END;
  while ((p < end) && (p - ((unsigned char *)start) < MIN_PACKETSZ))
    *p++ = 0;
  
  return p;
}

static unsigned char *option_put_string(unsigned char *p, unsigned char *end, int opt, char *string)
{
  size_t len = strlen(string);

  if (p + len + 3 < end)
    {
      *(p++) = opt;
      *(p++) = len;
      memcpy(p, string, len);
      p += len;
    }

  return p;
}
 
static unsigned char *option_find1(unsigned char *p, unsigned char *end, int opt, int *overload)
{
  if (!p)
    return NULL;
  
  while (*p != OPTION_END) 
    {
      if (p >= end)
	return 0; /* malformed packet */
      else if (*p == OPTION_PAD)
	p++;
      else if (*p == OPTION_OVERLOAD)
	{
	  if (p >= end - 3)
	    return 0; /* malformed packet */
	  if (overload) 
	    *overload = *(p+2);
	  p += 3;
	}
      else 
	{ 
	  int opt_len;;
	  if (p >= end - 2)
	    return 0; /* malformed packet */
	  opt_len = option_len(p);
	  if (p >= end - (2 + opt_len))
	    return 0; /* malformed packet */
	  if (*p == opt)
	    return p;
	  p += opt_len + 2;
	}
    }
  
  return NULL;
}
 
static unsigned char *option_find(struct dhcp_packet *mess, int size, int opt_type, int minsize)
{
  int overload = 0; 
  unsigned char *ret;
  
  /* skip over DHCP cookie; */
  ret = option_find1(&mess->options[0] + sizeof(u32), ((unsigned char *)mess) + size, opt_type, &overload);
  
  if (!ret && (overload & 1)) 
    ret = option_find1(&mess->file[0], &mess->file[128], opt_type, &overload);

  if (!ret && (overload & 2))
    ret = option_find1(&mess->sname[0], &mess->file[64], opt_type, &overload);
  
  /* Check the option field is big enough */
  if (ret && (option_len(ret) < minsize))
    ret = NULL;
  
  return ret;
}

static int in_list(unsigned char *list, int opt)
{
  int i;
  
  /* If no requested options, send everything, not nothing. */
  if (!list)
    return 1;
  
  for (i = 0; list[i] != OPTION_END; i++)
    if (opt == list[i])
      return 1;

  return 0;
}

static struct dhcp_opt *option_find2(struct dhcp_netid *netid, struct dhcp_opt *opts, int opt)
{
  struct dhcp_opt *tmp;  
  for (tmp = opts; tmp; tmp = tmp->next)
    if (tmp->opt == opt)
      {
	if (netid)
	  {
	    if (match_netid(tmp->netid, netid))
	      return tmp;
	  }
	else if (!tmp->netid)
	  return tmp;
      }
	      
  return netid ? option_find2(NULL, opts, opt) : NULL;
}

static unsigned char *do_opt(struct dhcp_opt *opt, unsigned char *p, unsigned char *end,  struct in_addr local)
{
  if (p + opt->len + 3 >= end)
    return p;
  
  *(p++) = opt->opt;
  *(p++) = opt->len;
  
  if (opt->len == 0)
    return p;
      
  if (opt->is_addr && !opt->vendor_class)
    {
      int j;
      struct in_addr *a = (struct in_addr *)opt->val;
      for (j = 0; j < opt->len; j+=INADDRSZ, a++)
	{
	  /* zero means "self" (but not in vendorclass options.) */
	  if (a->s_addr == 0)
	    memcpy(p, &local, INADDRSZ);
	  else
	    memcpy(p, a, INADDRSZ);
	  p += INADDRSZ;
	}
    }
  else
    {
      memcpy(p, opt->val, opt->len);
      p += opt->len;
    }

  return p;
}  

static unsigned char *do_req_options(struct dhcp_context *context,
				     unsigned char *p, unsigned char *end, 
				     unsigned char *req_options,
				     struct daemon *daemon,
				     char *hostname,
				     struct dhcp_netid *netid,
				     struct in_addr subnet_addr)
{
  struct dhcp_opt *opt, *config_opts = daemon->dhcp_opts;
  char *vendor_class = NULL;

  if (in_list(req_options, OPTION_MAXMESSAGE))
    p = option_put(p, end, OPTION_MAXMESSAGE, 2, end - (unsigned char *)daemon->dhcp_packet);
  
  /* rfc3011 says this doesn't need to be in the requested options list. */
  if (subnet_addr.s_addr)
    p = option_put(p, end, OPTION_SUBNET_SELECT, INADDRSZ, ntohl(subnet_addr.s_addr));

  if (in_list(req_options, OPTION_NETMASK) &&
      !option_find2(netid, config_opts, OPTION_NETMASK))
    p = option_put(p, end, OPTION_NETMASK, INADDRSZ, ntohl(context->netmask.s_addr));
  
  /* May not have a "guessed" broadcast address if we got no packets via a relay
     from this net yet (ie just unicast renewals after a restart */
  if (context->broadcast.s_addr &&
      in_list(req_options, OPTION_BROADCAST) &&
      !option_find2(netid, config_opts, OPTION_BROADCAST))
    p = option_put(p, end, OPTION_BROADCAST, INADDRSZ, ntohl(context->broadcast.s_addr));
  
  /* Same comments as broadcast apply, and also may not be able to get a sensible
     default when using subnet select.  User must configure by steam in that case. */
  if (context->router.s_addr &&
      in_list(req_options, OPTION_ROUTER) &&
      !option_find2(netid, config_opts, OPTION_ROUTER))
    p = option_put(p, end, OPTION_ROUTER, INADDRSZ, ntohl(context->router.s_addr));

  if (in_list(req_options, OPTION_DNSSERVER) &&
      !option_find2(netid, config_opts, OPTION_DNSSERVER))
    p = option_put(p, end, OPTION_DNSSERVER, INADDRSZ, ntohl(context->local.s_addr));
  
  if (daemon->domain_suffix && in_list(req_options, OPTION_DOMAINNAME) && 
      !option_find2(netid, config_opts, OPTION_DOMAINNAME))
    p = option_put_string(p, end, OPTION_DOMAINNAME, daemon->domain_suffix);
 
  /* Note that we ignore attempts to set the hostname using 
     --dhcp-option=12,<name> */
  if (hostname && in_list(req_options, OPTION_HOSTNAME))
    p = option_put_string(p, end, OPTION_HOSTNAME, hostname);
  
  for (opt=config_opts; opt; opt = opt->next)
    {
      if (opt->opt == OPTION_HOSTNAME ||
	  opt->opt == OPTION_MAXMESSAGE ||
	  !in_list(req_options, opt->opt) ||
	  opt != option_find2(netid, config_opts, opt->opt))
	continue;
      
      /* For the options we have default values on
	 dhc-option=<optionno> means "don't include this option"
	 not "include a zero-length option" */
      if (opt->len == 0 && 
	  (opt->opt == OPTION_NETMASK ||
	   opt->opt == OPTION_BROADCAST ||
	   opt->opt == OPTION_ROUTER ||
	   opt->opt == OPTION_DNSSERVER))
	continue;

      /* opt->val has terminating zero */
      if (opt->opt == OPTION_VENDOR_ID)
	vendor_class = opt->val; 
      else
	p = do_opt(opt, p, end, context->local);
    }  

  if (in_list(req_options, OPTION_VENDOR_ID))
    {      
      for (opt = daemon->vendor_opts; opt; opt = opt->next)
	if (!opt->netid || match_netid(opt->netid, netid))
	  {
	    if (vendor_class && strcmp(vendor_class, opt->vendor_class) != 0)
	      syslog(LOG_WARNING, "More than one vendor class matches, using %s", vendor_class);
	    else
	      vendor_class = opt->vendor_class;
	  }

      if (vendor_class)
	{
	  p = option_put_string(p, end, OPTION_VENDOR_ID, vendor_class);
	  
	  if (in_list(req_options, OPTION_VENDOR_CLASS_OPT))
	    {
	      unsigned char *plen, *oend = end;

	      /* encapsulated options can only be 256 bytes,
		 even of the packet is larger */
	      if (p + 256 < end)
		oend = p + 256;

	      if (p + 3 >= oend)
		return p;
	      
	      *(p++) = OPTION_VENDOR_CLASS_OPT;
	      plen = p++; /* fill in later */
	      
	      for (opt = daemon->vendor_opts; opt; opt = opt->next)
		if ((!opt->netid || match_netid(opt->netid, netid)) && 
		    strcmp(vendor_class, opt->vendor_class) == 0)
		  p = do_opt(opt, p, oend, context->local);
	      
	      *plen = p - plen - 1;
	    }
	}
    }
  
  return p;
}


