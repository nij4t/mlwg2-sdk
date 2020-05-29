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

static volatile int sigterm, sighup, sigusr1, sigalarm, num_kids, in_child;

static int set_dns_listeners(struct daemon *daemon, fd_set *set, int maxfd);
static void check_dns_listeners(struct daemon *daemon, fd_set *set, time_t now);
static void sig_handler(int sig);

int main (int argc, char **argv)
{
  struct daemon *daemon;
  int first_loop = 1;
  int bind_fallback = 0;
  time_t resolv_changed = 0;
  time_t now, last = 0;
  struct irec *interfaces;
  struct sigaction sigact;
  sigset_t sigmask;
  struct iname *if_tmp;

  sighup = 1; /* init cache the first time through */
  sigusr1 = 0; /* but don't dump */
  sigterm = 0; /* or die */
#ifdef HAVE_BROKEN_RTC
  sigalarm = 1; /* need regular lease dumps */
#else
  sigalarm = 0; /* or not */
#endif
  num_kids = 0;
  in_child = 0;
 
  sigact.sa_handler = sig_handler;
  sigact.sa_flags = 0;
  sigemptyset(&sigact.sa_mask);
  sigaction(SIGUSR1, &sigact, NULL);
  sigaction(SIGHUP, &sigact, NULL);
  sigaction(SIGTERM, &sigact, NULL);
  sigaction(SIGALRM, &sigact, NULL);
  sigaction(SIGCHLD, &sigact, NULL);

  /* ignore SIGPIPE */
  sigact.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &sigact, NULL);

  /* now block all the signals, they stay that way except
      during the call to pselect */
  sigaddset(&sigact.sa_mask, SIGUSR1);
  sigaddset(&sigact.sa_mask, SIGTERM);
  sigaddset(&sigact.sa_mask, SIGHUP);
  sigaddset(&sigact.sa_mask, SIGALRM);
  sigaddset(&sigact.sa_mask, SIGCHLD);
  sigprocmask(SIG_BLOCK, &sigact.sa_mask, &sigmask); 

  daemon = read_opts(argc, argv);
  
  if (daemon->edns_pktsz < PACKETSZ)
    daemon->edns_pktsz = PACKETSZ;
  daemon->packet_buff_sz = daemon->edns_pktsz > DNSMASQ_PACKETSZ ? 
    daemon->edns_pktsz : DNSMASQ_PACKETSZ;
  daemon->packet = safe_malloc(daemon->packet_buff_sz);
  
  if (!daemon->lease_file)
    {
      if (daemon->dhcp)
	daemon->lease_file = LEASEFILE;
    }
#ifndef HAVE_ISC_READER
  else if (!daemon->dhcp)
    die("ISC dhcpd integration not available: set HAVE_ISC_READER in src/config.h", NULL);
#endif
  
  if (!enumerate_interfaces(daemon, &interfaces, NULL, NULL))
    die("failed to find list of interfaces: %s", NULL);

  if (!(daemon->options & OPT_NOWILD) && 
      !(daemon->listeners = create_wildcard_listeners(daemon->port)))
    {
      bind_fallback = 1;
      daemon->options |= OPT_NOWILD;
    }
    
  if (daemon->options & OPT_NOWILD) 
    {
      daemon->listeners = create_bound_listeners(interfaces, daemon->port);

      for (if_tmp = daemon->if_names; if_tmp; if_tmp = if_tmp->next)
	if (if_tmp->name && !if_tmp->used)
	  die("unknown interface %s", if_tmp->name);
  
      for (if_tmp = daemon->if_addrs; if_tmp; if_tmp = if_tmp->next)
	if (!if_tmp->used)
	  {
	    char *addrbuff = daemon->namebuff;
#ifdef HAVE_IPV6
	    if (if_tmp->addr.sa.sa_family == AF_INET)
	      inet_ntop(AF_INET, &if_tmp->addr.in.sin_addr,
			addrbuff, ADDRSTRLEN);
	    else
	      inet_ntop(AF_INET6, &if_tmp->addr.in6.sin6_addr,
			addrbuff, ADDRSTRLEN);
#else
	    strcpy(addrbuff, inet_ntoa(if_tmp->addr.in.sin_addr));
#endif
	    die("no interface with address %s", addrbuff);
	  }
    }
  
  forward_init(1);
  cache_init(daemon->cachesize, daemon->options & OPT_LOG);

#ifdef HAVE_BROKEN_RTC
  if ((daemon->uptime_fd = open(UPTIME, O_RDONLY)) == -1)
    die("cannot open " UPTIME ":%s", NULL);
#endif
 
  now = dnsmasq_time(daemon->uptime_fd);
  
  if (daemon->dhcp)
    {
#if !defined(IP_PKTINFO) && !defined(IP_RECVIF)
      int c;
      struct iname *tmp;
      for (c = 0, tmp = daemon->if_names; tmp; tmp = tmp->next)
	if (!tmp->isloop)
	  c++;
      if (c != 1)
	die("must set exactly one interface on broken systems without IP_RECVIF", NULL);
#endif
      dhcp_init(daemon);
      lease_init(daemon, now);
    }

  /* If query_port is set then create a socket now, before dumping root
     for use to access nameservers without more specific source addresses.
     This allows query_port to be a low port */
  if (daemon->query_port)
    {
      union  mysockaddr addr;
      addr.in.sin_family = AF_INET;
      addr.in.sin_addr.s_addr = INADDR_ANY;
      addr.in.sin_port = htons(daemon->query_port);
#ifdef HAVE_SOCKADDR_SA_LEN
      addr.in.sin_len = sizeof(struct sockaddr_in);
#endif
      allocate_sfd(&addr, &daemon->sfds);
#ifdef HAVE_IPV6
      addr.in6.sin6_family = AF_INET6;
      addr.in6.sin6_addr = in6addr_any;
      addr.in6.sin6_port = htons(daemon->query_port);
      addr.in6.sin6_flowinfo = htonl(0);
#ifdef HAVE_SOCKADDR_SA_LEN
      addr.in6.sin6_len = sizeof(struct sockaddr_in6);
#endif
      allocate_sfd(&addr, &daemon->sfds);
#endif
    }
  
  setbuf(stdout, NULL);

  if (!(daemon->options & OPT_DEBUG))
    {
      FILE *pidfile;
      struct serverfd *serverfdp;
      struct listener *listener;
      struct passwd *ent_pw;
      int i;
        
      /* The following code "daemonizes" the process. 
	 See Stevens section 12.4 */

#ifndef NO_FORK
      if (!(daemon->options & OPT_NO_FORK))
	{
	  if (fork() != 0 )
	    exit(0);
	  
	  setsid();
	  
	  if (fork() != 0)
	    exit(0);
	}
#endif
      
      chdir("/");
      umask(022); /* make pidfile 0644 */
      
      /* write pidfile _after_ forking ! */
      if (daemon->runfile && (pidfile = fopen(daemon->runfile, "w")))
      	{
	  fprintf(pidfile, "%d\n", (int) getpid());
	  fclose(pidfile);
	}
      
      umask(0);

      for (i=0; i<64; i++)
	{
	  for (listener = daemon->listeners; listener; listener = listener->next)
	    if (listener->fd == i || listener->tcpfd == i)
	      break;
	  if (listener)
	    continue;

#ifdef HAVE_BROKEN_RTC	  
	  if (i == daemon->uptime_fd)
	    continue;
#endif

	  if (daemon->dhcp && 
	      (i == daemon->lease_fd || 
	       i == daemon->dhcpfd || 
	       i == daemon->dhcp_raw_fd ||
	       i == daemon->dhcp_icmp_fd))
	    continue;
	  
	  for (serverfdp = daemon->sfds; serverfdp; serverfdp = serverfdp->next)
	    if (serverfdp->fd == i)
	      break;
	  if (serverfdp)
	    continue;

	  close(i);
	}

      /* Change uid and gid for security */
      if (daemon->username && (ent_pw = getpwnam(daemon->username)))
	{
	  gid_t dummy;
	  struct group *gp;
	  /* remove all supplimentary groups */
	  setgroups(0, &dummy);
	  /* change group for /etc/ppp/resolv.conf 
	     otherwise get the group for "nobody" */
	  if ((daemon->groupname && (gp = getgrnam(daemon->groupname))) || 
	      (gp = getgrgid(ent_pw->pw_gid)))
	    setgid(gp->gr_gid); 
	  /* finally drop root */
	  setuid(ent_pw->pw_uid);
	}
    }

  openlog("dnsmasq", 
	  DNSMASQ_LOG_OPT(daemon->options & OPT_DEBUG), 
	  DNSMASQ_LOG_FAC(daemon->options & OPT_DEBUG));
  
/*
  if (daemon->cachesize != 0)
    syslog(LOG_DEBUG, "started, version %s cachesize %d", VERSION, daemon->cachesize);		// Ben 960822
  else
    syslog(LOG_DEBUG, "started, version %s cache disabled", VERSION);				// Ben 960822
  */
  if (bind_fallback)
    syslog(LOG_WARNING, "setting --bind-interfaces option because of OS limitations");
  
  if (!(daemon->options & OPT_NOWILD)) 
    for (if_tmp = daemon->if_names; if_tmp; if_tmp = if_tmp->next)
      if (if_tmp->name && !if_tmp->used)
	syslog(LOG_WARNING, "warning: interface %s does not currently exist", if_tmp->name);
  
  if (daemon->dhcp)
    {
      struct dhcp_context *dhcp_tmp;

#ifdef HAVE_RTNETLINK
      /* Must do this after daemonizing so that the pid is right */
      daemon->netlinkfd =  netlink_init();
#endif
      
      for (dhcp_tmp = daemon->dhcp; dhcp_tmp; dhcp_tmp = dhcp_tmp->next)
	{
	  prettyprint_time(daemon->dhcp_buff2, dhcp_tmp->lease_time);
	  strcpy(daemon->dhcp_buff, inet_ntoa(dhcp_tmp->start));
	  syslog(LOG_INFO, 
		 (dhcp_tmp->flags & CONTEXT_STATIC) ? 
		 "DHCP, static leases only on %.0s%s, lease time %s" :
		 "DHCP, IP range %s -- %s, lease time %s",
		 daemon->dhcp_buff, inet_ntoa(dhcp_tmp->end), daemon->dhcp_buff2);
	}
 
#ifdef HAVE_BROKEN_RTC
      daemon->min_leasetime = daemon->min_leasetime/3;
      if (daemon->min_leasetime > (60 * 60 * 24))
	daemon->min_leasetime = 60 * 60 * 24;
      if (daemon->min_leasetime < 60)
	daemon->min_leasetime = 60;
      prettyprint_time(daemon->dhcp_buff2, daemon->min_leasetime);
      syslog(LOG_INFO, "DHCP, %s will be written every %s", daemon->lease_file, daemon->dhcp_buff2);
#endif
    }
/*
  if (!(daemon->options & OPT_DEBUG) && (getuid() == 0 || geteuid() == 0))
    syslog(LOG_DEBUG, "running as root");					// Ben 960822
  */
  check_servers(daemon, interfaces);
  
  while (sigterm == 0)
    {
      fd_set rset;
      
      if (sighup)
	{
	  cache_reload(daemon->options, daemon->namebuff, daemon->domain_suffix, daemon->addn_hosts);
	  if (daemon->dhcp)
	    {
	      if (daemon->options & OPT_ETHERS)
		dhcp_read_ethers(daemon);
	      dhcp_update_configs(daemon->dhcp_conf);
	      lease_update_from_configs(daemon->dhcp_conf, daemon->domain_suffix); 
	      lease_update_file(0, now); 
	      lease_update_dns(daemon);
	    }
	  if (daemon->resolv_files && (daemon->options & OPT_NO_POLL))
	    {
	      reload_servers(daemon->resolv_files->name, daemon);
	      check_servers(daemon, interfaces);
	    }
	  sighup = 0;
	}
      
      if (sigusr1)
	{
	  dump_cache(daemon);
	  sigusr1 = 0;
	}
      
      if (sigalarm)
	{
	  if (daemon->dhcp)
	    {
	      lease_update_file(1, now);
#ifdef HAVE_BROKEN_RTC
	      alarm(daemon->min_leasetime);
#endif
	    } 
	  sigalarm  = 0;
	}
      
      FD_ZERO(&rset);
      
      if (!first_loop)
	{
	  int maxfd = set_dns_listeners(daemon, &rset, 0);
	  	  
	  if (daemon->dhcp)
	    {
	      FD_SET(daemon->dhcpfd, &rset);
	      if (daemon->dhcpfd > maxfd)
		maxfd = daemon->dhcpfd;
	    }

#ifdef HAVE_PSELECT
	  if (pselect(maxfd+1, &rset, NULL, NULL, NULL, &sigmask) < 0)
	    FD_ZERO(&rset); /* rset otherwise undefined after error */ 
#else
	  {
	    sigset_t save_mask;
	    sigprocmask(SIG_SETMASK, &sigmask, &save_mask);
	    if (select(maxfd+1, &rset, NULL, NULL, NULL) < 0)
	      FD_ZERO(&rset); /* rset otherwise undefined after error */ 
	    sigprocmask(SIG_SETMASK, &save_mask, NULL);
	  }
#endif
	}

      first_loop = 0;
      now = dnsmasq_time(daemon->uptime_fd);

      /* Check for changes to resolv files once per second max. */
      if (last == 0 || difftime(now, last) > 1.0)
	{
	  last = now;

#ifdef HAVE_ISC_READER
	  if (daemon->lease_file && !daemon->dhcp)
	    load_dhcp(daemon, now);
#endif

	  if (!(daemon->options & OPT_NO_POLL))
	    {
	      struct resolvc *res = daemon->resolv_files, *latest = NULL;
	      struct stat statbuf;
	      time_t last_change = 0;
	      /* There may be more than one possible file. 
		 Go through and find the one which changed _last_.
		 Warn of any which can't be read. */
	      while (res)
		{
		  if (stat(res->name, &statbuf) == -1)
		    {
		      if (!res->logged)
			syslog(LOG_WARNING, "failed to access %s: %m", res->name);
		      res->logged = 1;
		    }
		  else
		    {
		      res->logged = 0;
		      if (difftime(statbuf.st_mtime, last_change) > 0.0)
			{
			  last_change = statbuf.st_mtime;
			  latest = res;
			}
		    }
		  res = res->next;
		}
	  
	      if (latest && difftime(last_change, resolv_changed) > 0.0)
		{
		  resolv_changed = last_change;
		  reload_servers(latest->name, daemon);
		  check_servers(daemon, interfaces);
		}
	    }
	}
		
      check_dns_listeners(daemon, &rset, now);
      
      if (daemon->dhcp && FD_ISSET(daemon->dhcpfd, &rset))
	dhcp_packet(daemon, now);
    }
  
//  syslog(LOG_DEBUG, "exiting on receipt of SIGTERM");						// Ben 960822

  if (daemon->dhcp)
    { 
#ifdef HAVE_BROKEN_RTC
      lease_update_file(1, now);
#endif
      close(daemon->lease_fd);
    }
  
  return 0;
}

static void sig_handler(int sig)
{
  if (sig == SIGTERM)
    sigterm = 1;
  else if (sig == SIGHUP)
    sighup = 1;
  else if (sig == SIGUSR1)
    sigusr1 = 1;
  else if (sig == SIGALRM)
    {
      /* alarm is used to kill children after a fixed time. */
      if (in_child)
	exit(0);
      else
	sigalarm = 1;
    }
  else if (sig == SIGCHLD)
    {
      /* See Stevens 5.10 */
      
      while (waitpid(-1, NULL, WNOHANG) > 0)
	num_kids--;
    }
}

static int set_dns_listeners(struct daemon *daemon, fd_set *set, int maxfd)
{
  struct serverfd *serverfdp;
  struct listener *listener;

  for (serverfdp = daemon->sfds; serverfdp; serverfdp = serverfdp->next)
    {
      FD_SET(serverfdp->fd, set);
      if (serverfdp->fd > maxfd)
	maxfd = serverfdp->fd;
    }
	  
  for (listener = daemon->listeners; listener; listener = listener->next)
    {
      FD_SET(listener->fd, set);
      if (listener->fd > maxfd)
	maxfd = listener->fd;
      FD_SET(listener->tcpfd, set);
      if (listener->tcpfd > maxfd)
	maxfd = listener->tcpfd;
    }

  return maxfd;
}

static void check_dns_listeners(struct daemon *daemon, fd_set *set, time_t now)
{
  struct serverfd *serverfdp;
  struct listener *listener;	  

   for (serverfdp = daemon->sfds; serverfdp; serverfdp = serverfdp->next)
     if (FD_ISSET(serverfdp->fd, set))
       reply_query(serverfdp, daemon, now);

   for (listener = daemon->listeners; listener; listener = listener->next)
     {
       if (FD_ISSET(listener->fd, set))
	 receive_query(listener, daemon, now);
       
       if (FD_ISSET(listener->tcpfd, set))
	 {
	   int confd;
	   struct in_addr netmask, dst_addr_4;
	   
	   while((confd = accept(listener->tcpfd, NULL, NULL)) == -1 && errno == EINTR);
	      
	   if (confd != -1)
	     {
	       union mysockaddr tcp_addr;
	       socklen_t tcp_len = sizeof(union mysockaddr);
	   	       
	       /* Check for allowed interfaces when binding the wildcard address:
		  we do this by looking for an interface with the same address as 
		  the local address of the TCP connection, then looking to see if that's
		  an allowed interface. As a side effect, we get the netmask of the
		  interface too, for localisation. */

	       if ((num_kids >= MAX_PROCS) ||
		   (!(daemon->options & OPT_NOWILD) &&
		    (getsockname(confd, (struct sockaddr *)&tcp_addr, &tcp_len) == -1 ||
		     !enumerate_interfaces(daemon, NULL, &tcp_addr, &netmask)))) 
		 close(confd);
#ifndef NO_FORK
	       else if (!(daemon->options & OPT_DEBUG) && fork())
		 {
		   num_kids++;
		   close(confd);
		 }
#endif
	       else
		 {
		   char *buff;
		   struct server *s; 
		   int flags;
		   
		   /* Arrange for SIGALARM after CHILD_LIFETIME seconds to
		      terminate the process. */
		   if (!(daemon->options & OPT_DEBUG))
		     {
		       sigset_t mask;
		       sigemptyset(&mask);
		       sigaddset(&mask, SIGALRM);
		       sigprocmask(SIG_UNBLOCK, &mask, NULL);
		       alarm(CHILD_LIFETIME);
		       in_child = 1;
		     }
		   
		   /* start with no upstream connections. */
		   for (s = daemon->servers; s; s = s->next)
		     s->tcpfd = -1; 
		   
		   /* The connected socket inherits non-blocking
		      attribute from the listening socket. 
		      Reset that here. */
		   if ((flags = fcntl(confd, F_GETFL, 0)) != -1)
		     fcntl(confd, F_SETFL, flags & ~O_NONBLOCK);
		   
		   if (listener->family == AF_INET)
		     {
		       if (daemon->options & OPT_NOWILD)
			 {
			   netmask = listener->iface->netmask;
			   dst_addr_4 = listener->iface->addr.in.sin_addr;
			 }
		       else
			 /* netmask already set by enumerate_interfaces */
			 dst_addr_4 = tcp_addr.in.sin_addr;
		     }
		   else
		     dst_addr_4.s_addr = 0;
		   
		   buff = tcp_request(daemon, confd, now, dst_addr_4, netmask);
		   
		   if (!(daemon->options & OPT_DEBUG))
		     exit(0);
		      
		   close(confd);
		   if (buff)
		     free(buff);
		   for (s = daemon->servers; s; s = s->next)
		     if (s->tcpfd != -1)
		       close(s->tcpfd);
		 }
	     }
	 }
     }
}

int icmp_ping(struct daemon *daemon, struct in_addr addr)
{
  /* Try and get an ICMP echo from a machine.
     Note that we can't create the raw socket each time
     we do this, since that needs root. Therefore the socket has to hang
     around all the time. Since most of the time we won't read the 
     socket, it will accumulate buffers full of ICMP messages,
     wasting memory. To avoid that we set the receive buffer
     length to zero except when we're actively pinging. */

  /* Note that whilst in the three second wait, we check for 
     (and service) events on the DNS sockets, (so doing that
     better not use any resources our caller has in use...)
     but we remain deaf to signals or further DHCP packets. */

  struct sockaddr_in saddr;
  struct { 
    struct ip ip;
    struct icmp icmp;
  } packet;
  unsigned short id = rand16();
  unsigned int i, j;
  int opt = 2000, gotreply = 0;
  time_t start, now;
  
  saddr.sin_family = AF_INET;
  saddr.sin_port = 0;
  saddr.sin_addr = addr;
#ifdef HAVE_SOCKADDR_SA_LEN
  saddr.sin_len = sizeof(struct sockaddr_in);
#endif
  
  memset(&packet.icmp, 0, sizeof(packet.icmp));
  packet.icmp.icmp_type = ICMP_ECHO;
  packet.icmp.icmp_id = id;
  for (j = 0, i = 0; i < sizeof(struct icmp) / 2; i++)
    j += ((u16 *)&packet.icmp)[i];
  while (j>>16)
    j = (j & 0xffff) + (j >> 16);  
  packet.icmp.icmp_cksum = (j == 0xffff) ? j : ~j;
  
  setsockopt(daemon->dhcp_icmp_fd, SOL_SOCKET, SO_RCVBUF, &opt, sizeof(opt));

  while (sendto(daemon->dhcp_icmp_fd, (char *)&packet.icmp, sizeof(struct icmp), 0, 
		(struct sockaddr *)&saddr, sizeof(saddr)) == -1 &&
	 retry_send());
  
  for (now = start = dnsmasq_time(daemon->uptime_fd); difftime(now, start) < 3.0;)
    {
      struct timeval tv;
      fd_set rset;
      struct sockaddr_in faddr;
      int maxfd, len = sizeof(faddr);
      
      tv.tv_usec = 250000;
      tv.tv_sec = 0; 
      
      FD_ZERO(&rset);
      FD_SET(daemon->dhcp_icmp_fd, &rset);
      maxfd = set_dns_listeners(daemon, &rset, daemon->dhcp_icmp_fd);
		
      if (select(maxfd+1, &rset, NULL, NULL, &tv) < 0)
	FD_ZERO(&rset);
      
      now = dnsmasq_time(daemon->uptime_fd);
      check_dns_listeners(daemon, &rset, now);
      
      if (FD_ISSET(daemon->dhcp_icmp_fd, &rset) &&
	  recvfrom(daemon->dhcp_icmp_fd, &packet, sizeof(packet), 0,
		   (struct sockaddr *)&faddr, &len) == sizeof(packet) &&
	  saddr.sin_addr.s_addr == faddr.sin_addr.s_addr &&
	  packet.icmp.icmp_type == ICMP_ECHOREPLY &&
	  packet.icmp.icmp_seq == 0 &&
	  packet.icmp.icmp_id == id)
	{
	  gotreply = 1;
	  break;
	}
    }
  
  opt = 1;
  setsockopt(daemon->dhcp_icmp_fd, SOL_SOCKET, SO_RCVBUF, &opt, sizeof(opt));
  
  return gotreply;
}

 
