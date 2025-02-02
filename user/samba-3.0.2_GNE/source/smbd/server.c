/* 
   Unix SMB/CIFS implementation.
   Main SMB server routines
   Copyright (C) Andrew Tridgell		1992-1998
   Copyright (C) Martin Pool			2002
   Copyright (C) Jelmer Vernooij		2002-2003
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "includes.h"

// +++ Gemtek - Check Second SSID
#include "gtk_check2ndssid.h"
struct in_addr clientip;
// --- Gemtek - Check Second SSID

int am_parent = 1;

/* the last message the was processed */
int last_message = -1;

/* a useful macro to debug the last message processed */
#define LAST_MESSAGE() smb_fn_name(last_message)

extern pstring user_socket_options;
extern SIG_ATOMIC_T got_sig_term;
extern SIG_ATOMIC_T reload_after_sighup;

#ifdef WITH_DFS
extern int dcelogin_atmost_once;
#endif /* WITH_DFS */

/* really we should have a top level context structure that has the
   client file descriptor as an element. That would require a major rewrite :(

   the following 2 functions are an alternative - they make the file
   descriptor private to smbd
 */
static int server_fd = -1;

int smbd_server_fd(void)
{
	return server_fd;
}

static void smbd_set_server_fd(int fd)
{
	server_fd = fd;
	client_setfd(fd);
}

/****************************************************************************
 Terminate signal.
****************************************************************************/

static void sig_term(void)
{
	got_sig_term = 1;
	sys_select_signal();
}

/****************************************************************************
 Catch a sighup.
****************************************************************************/

static void sig_hup(int sig)
{
	reload_after_sighup = 1;
	sys_select_signal();
}

/****************************************************************************
  Send a SIGTERM to our process group.
*****************************************************************************/

static void  killkids(void)
{
	//if(am_parent) kill(0,SIGTERM);
}

/****************************************************************************
 Process a sam sync message - not sure whether to do this here or
 somewhere else.
****************************************************************************/

static void msg_sam_sync(int UNUSED(msg_type), pid_t UNUSED(pid),
			 void *UNUSED(buf), size_t UNUSED(len))
{
        DEBUG(10, ("** sam sync message received, ignoring\n"));
}

/****************************************************************************
 Process a sam sync replicate message - not sure whether to do this here or
 somewhere else.
****************************************************************************/

static void msg_sam_repl(int msg_type, pid_t pid, void *buf, size_t len)
{
        uint32 low_serial;

        if (len != sizeof(uint32))
                return;

        low_serial = *((uint32 *)buf);

        DEBUG(3, ("received sam replication message, serial = 0x%04x\n",
                  low_serial));
}

/****************************************************************************
 Open the socket communication - inetd.
****************************************************************************/

static BOOL open_sockets_inetd(void)
{
	/* Started from inetd. fd 0 is the socket. */
	/* We will abort gracefully when the client or remote system 
	   goes away */
	smbd_set_server_fd(dup(0));
	
	/* close our standard file descriptors */
	close_low_fds(False); /* Don't close stderr */
	
	set_socket_options(smbd_server_fd(),"SO_KEEPALIVE");
	set_socket_options(smbd_server_fd(), user_socket_options);

	return True;
}

static void msg_exit_server(int msg_type, pid_t src, void *buf, size_t len)
{
	exit_server("Got a SHUTDOWN message");
}


/****************************************************************************
 Have we reached the process limit ?
****************************************************************************/

BOOL allowable_number_of_smbd_processes(void)
{
	int max_processes = lp_max_smbd_processes();

	if (!max_processes)
		return True;

	{
		TDB_CONTEXT *tdb = conn_tdb_ctx();
		int32 val;
		if (!tdb) {
			DEBUG(0,("allowable_number_of_smbd_processes: can't open connection tdb.\n" ));
			return False;
		}

		val = tdb_fetch_int32(tdb, "INFO/total_smbds");
		if (val == -1 && (tdb_error(tdb) != TDB_ERR_NOEXIST)) {
			DEBUG(0,("allowable_number_of_smbd_processes: can't fetch INFO/total_smbds. Error %s\n",
				tdb_errorstr(tdb) ));
			return False;
		}
		if (val > max_processes) {
			DEBUG(0,("allowable_number_of_smbd_processes: number of processes (%d) is over allowed limit (%d)\n",
				val, max_processes ));
			return False;
		}
	}
	return True;
}

/****************************************************************************
 Open the socket communication.
****************************************************************************/

static BOOL open_sockets_smbd(BOOL is_daemon, BOOL interactive, const char *smb_ports)
{
	int num_interfaces = iface_count();
	int num_sockets = 0;
	int fd_listenset[FD_SETSIZE];
	fd_set listen_set;
	int s;
	int i;
	char *ports;

	if (!is_daemon) {
		return open_sockets_inetd();
	}

		
#ifdef HAVE_ATEXIT
	{
		static int atexit_set;
		if(atexit_set == 0) {
			atexit_set=1;
			atexit(killkids);
		}
	}
#endif

	/* Stop zombies */
	CatchChild();
				
	FD_ZERO(&listen_set);

	/* use a reasonable default set of ports - listing on 445 and 139 */
	if (!smb_ports) {
		ports = lp_smb_ports();
		if (!ports || !*ports) {
			ports = smb_xstrdup(SMB_PORTS);
		} else {
			ports = smb_xstrdup(ports);
		}
	} else {
		ports = smb_xstrdup(smb_ports);
	}

	if (lp_interfaces() && lp_bind_interfaces_only()) {
		/* We have been given an interfaces line, and been 
		   told to only bind to those interfaces. Create a
		   socket per interface and bind to only these.
		*/
		
		/* Now open a listen socket for each of the
		   interfaces. */
		for(i = 0; i < num_interfaces; i++) {
			struct in_addr *ifip = iface_n_ip(i);
			fstring tok;
			const char *ptr;

			if(ifip == NULL) {
				DEBUG(0,("open_sockets_smbd: interface %d has NULL IP address !\n", i));
				continue;
			}

			for (ptr=ports; next_token(&ptr, tok, NULL, sizeof(tok)); ) {
				unsigned port = atoi(tok);
				if (port == 0) continue;
				s = fd_listenset[num_sockets] = open_socket_in(SOCK_STREAM, port, 0, ifip->s_addr, True);
				if(s == -1)
					return False;

				/* ready to listen */
				set_socket_options(s,"SO_KEEPALIVE"); 
				set_socket_options(s,user_socket_options);
      
				if (listen(s, SMBD_LISTEN_BACKLOG) == -1) {
					DEBUG(0,("listen: %s\n",strerror(errno)));
					close(s);
					return False;
				}
				FD_SET(s,&listen_set);

				num_sockets++;
				if (num_sockets >= FD_SETSIZE) {
					DEBUG(0,("open_sockets_smbd: Too many sockets to bind to\n"));
					return False;
				}
			}
		}
	} else {
		/* Just bind to 0.0.0.0 - accept connections
		   from anywhere. */

		fstring tok;
		const char *ptr;

		num_interfaces = 1;
		
		for (ptr=ports; next_token(&ptr, tok, NULL, sizeof(tok)); ) {
			unsigned port = atoi(tok);
			if (port == 0) continue;
			/* open an incoming socket */
			s = open_socket_in(SOCK_STREAM, port, 0,
					   interpret_addr(lp_socket_address()),True);
			if (s == -1)
				return(False);
		
			/* ready to listen */
			set_socket_options(s,"SO_KEEPALIVE"); 
			set_socket_options(s,user_socket_options);
			
			if (listen(s, SMBD_LISTEN_BACKLOG) == -1) {
				DEBUG(0,("open_sockets_smbd: listen: %s\n",
					 strerror(errno)));
				close(s);
				return False;
			}

			fd_listenset[num_sockets] = s;
			FD_SET(s,&listen_set);

			num_sockets++;

			if (num_sockets >= FD_SETSIZE) {
				DEBUG(0,("open_sockets_smbd: Too many sockets to bind to\n"));
				return False;
			}
		}
	} 

	SAFE_FREE(ports);

        /* Listen to messages */

        message_register(MSG_SMB_SAM_SYNC, msg_sam_sync);
        message_register(MSG_SMB_SAM_REPL, msg_sam_repl);
        message_register(MSG_SHUTDOWN, msg_exit_server);

	/* now accept incoming connections - forking a new process
	   for each incoming connection */
	DEBUG(2,("waiting for a connection\n"));
	while (1) {
		fd_set lfds;
		int num;
		
		/* Free up temporary memory from the main smbd. */
		lp_talloc_free();

		/* Ensure we respond to PING and DEBUG messages from the main smbd. */
		message_dispatch();

		memcpy((char *)&lfds, (char *)&listen_set, 
		       sizeof(listen_set));
		
		num = sys_select(FD_SETSIZE,&lfds,NULL,NULL,NULL);
		
		if (num == -1 && errno == EINTR) {
			if (got_sig_term) {
				exit_server("Caught TERM signal");
			}

			/* check for sighup processing */
			if (reload_after_sighup) {
				change_to_root_user();
				DEBUG(1,("Reloading services after SIGHUP\n"));
				reload_services(False);
				reload_after_sighup = 0;
			}

			continue;
		}
		
		/* check if we need to reload services */
		check_reload(time(NULL));

		/* Find the sockets that are read-ready -
		   accept on these. */
		for( ; num > 0; num--) {
			struct sockaddr addr;
			socklen_t in_addrlen = sizeof(addr);
			// +++ Gemtek - Check Second SSID
			struct sockaddr_in *ptr;
			ptr = &addr;
			// --- Gemtek - Check Second SSID 

			s = -1;
			for(i = 0; i < num_sockets; i++) {
				if(FD_ISSET(fd_listenset[i],&lfds)) {
					s = fd_listenset[i];
					/* Clear this so we don't look
					   at it again. */
					FD_CLR(fd_listenset[i],&lfds);
					break;
				}
			}

			smbd_set_server_fd(accept(s,&addr,&in_addrlen));
					
			  if (smbd_server_fd() == -1 && errno == EINTR)
				  continue;
			
			  if (smbd_server_fd() == -1) {
				  DEBUG(0,("open_sockets_smbd: accept: %s\n",
					   strerror(errno)));
				  continue;
			  }

			  if (smbd_server_fd() != -1 && interactive)
				  return True;
			
			  if (allowable_number_of_smbd_processes() && smbd_server_fd() != -1 && sys_fork()==0) {
				  /* Child code ... */
				  CatchChildLeaveStatus();
				  /* close the listening socket(s) */
				  for(i = 0; i < num_sockets; i++)
					  close(fd_listenset[i]);
				
				  /* close our standard file
				     descriptors */
				  close_low_fds(False);
				  am_parent = 0;
				
				  set_socket_options(smbd_server_fd(),"SO_KEEPALIVE");
				  set_socket_options(smbd_server_fd(),user_socket_options);
				
				  /* this is needed so that we get decent entries
				     in smbstatus for port 445 connects */
				  set_remote_machine_name(get_peer_addr(smbd_server_fd()), False);
				
				  /* Reset global variables in util.c so
				     that client substitutions will be
				     done correctly in the process.  */
				  reset_globals_after_fork();

				  /* tdb needs special fork handling */
				  if (tdb_reopen_all() == -1) {
					  DEBUG(0,("tdb_reopen_all failed.\n"));
					  return False;
				  }

				  return True; 
			  }
			  /* The parent doesn't need this socket */
			  close(smbd_server_fd()); 

			  /* Sun May 6 18:56:14 2001 ackley@cs.unm.edu:
				  Clear the closed fd info out of server_fd --
				  and more importantly, out of client_fd in
				  util_sock.c, to avoid a possible
				  getpeername failure if we reopen the logs
				  and use %I in the filename.
			  */

			  smbd_set_server_fd(-1);

			  /* Force parent to check log size after
			   * spawning child.  Fix from
			   * klausr@ITAP.Physik.Uni-Stuttgart.De.  The
			   * parent smbd will log to logserver.smb.  It
			   * writes only two messages for each child
			   * started/finished. But each child writes,
			   * say, 50 messages also in logserver.smb,
			   * begining with the debug_count of the
			   * parent, before the child opens its own log
			   * file logserver.client. In a worst case
			   * scenario the size of logserver.smb would be
			   * checked after about 50*50=2500 messages
			   * (ca. 100kb).
			   * */
			  force_check_log_size();
 
		} /* end for num */
	} /* end while 1 */

/* NOTREACHED	return True; */
}

/****************************************************************************
 Reload the services file.
**************************************************************************/

BOOL reload_services(BOOL test)
{
	BOOL ret;
	
	if (lp_loaded()) {
		pstring fname;
		pstrcpy(fname,lp_configfile());
		if (file_exist(fname, NULL) &&
		    !strcsequal(fname, dyn_CONFIGFILE)) {
			pstrcpy(dyn_CONFIGFILE, fname);
			test = False;
		}
	}

	reopen_logs();

	if (test && !lp_file_list_changed())
		return(True);

	lp_killunused(conn_snum_used);
	
	ret = lp_load(dyn_CONFIGFILE, False, False, True);

	load_printers();

	/* perhaps the config filename is now set */
	if (!test)
		reload_services(True);

	reopen_logs();

	load_interfaces();

	{
		if (smbd_server_fd() != -1) {      
			set_socket_options(smbd_server_fd(),"SO_KEEPALIVE");
			set_socket_options(smbd_server_fd(), user_socket_options);
		}
	}

	mangle_reset_cache();
	reset_stat_cache();

	/* this forces service parameters to be flushed */
	set_current_service(NULL,True);

	return(ret);
}

#if DUMP_CORE
/*******************************************************************
prepare to dump a core file - carefully!
********************************************************************/
static BOOL dump_core(void)
{
	char *p;
	pstring dname;
	
	pstrcpy(dname,lp_logfile());
	if ((p=strrchr_m(dname,'/'))) *p=0;
	pstrcat(dname,"/corefiles");
	mkdir(dname,0700);
	sys_chown(dname,getuid(),getgid());
	chmod(dname,0700);
	if (chdir(dname)) return(False);
	umask(~(0700));

#ifdef HAVE_GETRLIMIT
#ifdef RLIMIT_CORE
	{
		struct rlimit rlp;
		getrlimit(RLIMIT_CORE, &rlp);
		rlp.rlim_cur = MAX(4*1024*1024,rlp.rlim_cur);
		setrlimit(RLIMIT_CORE, &rlp);
		getrlimit(RLIMIT_CORE, &rlp);
		DEBUG(3,("Core limits now %d %d\n",
			 (int)rlp.rlim_cur,(int)rlp.rlim_max));
	}
#endif
#endif


	DEBUG(0,("Dumping core in %s\n", dname));
	abort();
	return(True);
}
#endif

/****************************************************************************
 Exit the server.
****************************************************************************/

void exit_server(const char *reason)
{
	static int firsttime=1;
	extern char *last_inbuf;
	extern struct auth_context *negprot_global_auth_context;

	if (!firsttime)
		exit(0);
	firsttime = 0;

	change_to_root_user();
	DEBUG(2,("Closing connections\n"));

	if (negprot_global_auth_context) {
		(negprot_global_auth_context->free)(&negprot_global_auth_context);
	}

	conn_close_all();

	invalidate_all_vuids();

	print_notify_send_messages(3); /* 3 second timeout. */

	/* run all registered exit events */
	smb_run_exit_events();

	/* delete our entry in the connections database. */
	yield_connection(NULL,"");

	respond_to_all_remaining_local_messages();
	decrement_smbd_process_count();

#ifdef WITH_DFS
	if (dcelogin_atmost_once) {
		dfs_unlogin();
	}
#endif

	if (!reason) {   
		int oldlevel = DEBUGLEVEL;
		DEBUGLEVEL = 10;
		DEBUG(0,("Last message was %s\n",smb_fn_name(last_message)));
		if (last_inbuf)
			show_msg(last_inbuf);
		DEBUGLEVEL = oldlevel;
		DEBUG(0,("===============================================================\n"));
#if DUMP_CORE
		if (dump_core()) return;
#endif
	}    

	locking_end();
	printing_end();

	DEBUG(3,("Server exit (%s)\n", (reason ? reason : "")));
	exit(0);
}

/****************************************************************************
 Initialise connect, service and file structs.
****************************************************************************/

static BOOL init_structs(void )
{
	/*
	 * Set the machine NETBIOS name if not already
	 * set from the config file.
	 */

	if (!init_names())
		return False;

	conn_init();

	file_init();

	/* for RPC pipes */
	init_rpc_pipe_hnd();

	init_dptrs();

	secrets_init();

	return True;
}

/****************************************************************************
 main program.
****************************************************************************/

/* Declare prototype for build_options() to avoid having to run it through
   mkproto.h.  Mixing $(builddir) and $(srcdir) source files in the current
   prototype generation system is too complicated. */

void build_options(BOOL screen);

 int main(int argc,const char *argv[])
{
	/* shall I run as a daemon */
	static BOOL is_daemon = False;
	static BOOL interactive = False;
	static BOOL Fork = True;
	static BOOL log_stdout = False;
	static char *ports = NULL;
	int opt;
	poptContext pc;

	struct poptOption long_options[] = {
		POPT_AUTOHELP
	{"daemon", 'D', POPT_ARG_VAL, &is_daemon, True, "Become a daemon (default)" },
	{"interactive", 'i', POPT_ARG_VAL, &interactive, True, "Run interactive (not a daemon)"},
	{"foreground", 'F', POPT_ARG_VAL, &Fork, False, "Run daemon in foreground (for daemontools & etc)" },
	{"log-stdout", 'S', POPT_ARG_VAL, &log_stdout, True, "Log to stdout" },
	{"build-options", 'b', POPT_ARG_NONE, NULL, 'b', "Print build options" },
	{"port", 'p', POPT_ARG_STRING, &ports, 0, "Listen on the specified ports"},
	POPT_COMMON_SAMBA
	{ NULL }
	};

#ifdef HAVE_SET_AUTH_PARAMETERS
	set_auth_parameters(argc,argv);
#endif

	pc = poptGetContext("smbd", argc, argv, long_options, 0);
	
	while((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt)  {
		case 'b':
			build_options(True); /* Display output to screen as well as debug */ 
			exit(0);
			break;
		}
	}

	poptFreeContext(pc);

#ifdef HAVE_SETLUID
	/* needed for SecureWare on SCO */
	setluid(0);
#endif

	sec_init();

	load_case_tables();

	set_remote_machine_name("smbd", False);

	if (interactive) {
		Fork = False;
		log_stdout = True;
	}

	if (log_stdout && Fork) {
		DEBUG(0,("ERROR: Can't log to stdout (-S) unless daemon is in foreground (-F) or interactive (-i)\n"));
		exit(1);
	}

	setup_logging(argv[0],log_stdout);

	/* we want to re-seed early to prevent time delays causing
           client problems at a later date. (tridge) */
	generate_random_buffer(NULL, 0, False);

	/* make absolutely sure we run as root - to handle cases where people
	   are crazy enough to have it setuid */

	gain_root_privilege();
	gain_root_group_privilege();

	fault_setup((void (*)(void *))exit_server);
	CatchSignal(SIGTERM , SIGNAL_CAST sig_term);
	CatchSignal(SIGHUP,SIGNAL_CAST sig_hup);
	
	/* we are never interested in SIGPIPE */
	BlockSignals(True,SIGPIPE);

#if defined(SIGFPE)
	/* we are never interested in SIGFPE */
	BlockSignals(True,SIGFPE);
#endif

#if defined(SIGUSR2)
	/* We are no longer interested in USR2 */
	BlockSignals(True,SIGUSR2);
#endif

	/* POSIX demands that signals are inherited. If the invoking process has
	 * these signals masked, we will have problems, as we won't recieve them. */
	BlockSignals(False, SIGHUP);
	BlockSignals(False, SIGUSR1);
	BlockSignals(False, SIGTERM);

	/* we want total control over the permissions on created files,
	   so set our umask to 0 */
	umask(0);

	init_sec_ctx();

	reopen_logs();

	DEBUG(0,( "smbd version %s started.\n", SAMBA_VERSION_STRING));
	DEBUGADD(0,( "Copyright Andrew Tridgell and the Samba Team 1992-2004\n"));

	DEBUG(2,("uid=%d gid=%d euid=%d egid=%d\n",
		 (int)getuid(),(int)getgid(),(int)geteuid(),(int)getegid()));

	/* Output the build options to the debug log */ 
	build_options(False);

	if (sizeof(uint16) < 2 || sizeof(uint32) < 4) {
		DEBUG(0,("ERROR: Samba is not configured correctly for the word size on your machine\n"));
		exit(1);
	}

	/*
	 * Do this before reload_services.
	 */

	if (!reload_services(False))
		return(-1);	

	init_structs();

#ifdef WITH_PROFILE
	if (!profile_setup(False)) {
		DEBUG(0,("ERROR: failed to setup profiling\n"));
		return -1;
	}
#endif

	DEBUG(3,( "loaded services\n"));

	if (!is_daemon && !is_a_socket(0)) {
		if (!interactive)
			DEBUG(0,("standard input is not a socket, assuming -D option\n"));

		/*
		 * Setting is_daemon here prevents us from eventually calling
		 * the open_sockets_inetd()
		 */

		is_daemon = True;
	}

	if (is_daemon && !interactive) {
		DEBUG( 3, ( "Becoming a daemon.\n" ) );
		become_daemon(Fork);
	}

#if HAVE_SETPGID
	/*
	 * If we're interactive we want to set our own process group for
	 * signal management.
	 */
	if (interactive)
		setpgid( (pid_t)0, (pid_t)0);
#endif

	if (!directory_exist(lp_lockdir(), NULL))
		mkdir(lp_lockdir(), 0755);

	if (is_daemon)
		pidfile_create("smbd");

	if (!message_init())
		exit(1);

	if (!print_backend_init())
		exit(1);

	/* Setup the main smbd so that we can get messages. */
	claim_connection(NULL,"",0,True,FLAG_MSG_GENERAL|FLAG_MSG_SMBD);

	/* 
	   DO NOT ENABLE THIS TILL YOU COPE WITH KILLING THESE TASKS AND INETD
	   THIS *killed* LOTS OF BUILD FARM MACHINES. IT CREATED HUNDREDS OF 
	   smbd PROCESSES THAT NEVER DIE
	   start_background_queue(); 
	*/

	if (!open_sockets_smbd(is_daemon, interactive, ports))
		exit(1);

	/*
	 * everything after this point is run after the fork()
	 */ 

	namecache_enable();

	if (!locking_init(0))
		exit(1);

	if (!share_info_db_init())
		exit(1);

	if (!init_registry())
		exit(1);

	/* Initialise the password backed before the global_sam_sid
	   to ensure that we fetch from ldap before we make a domain sid up */

	if(!initialize_password_db(False))
		exit(1);

	if(!get_global_sam_sid()) {
		DEBUG(0,("ERROR: Samba cannot create a SAM SID.\n"));
		exit(1);
	}

	static_init_rpc;

	init_modules();

	/* possibly reload the services file. */
	reload_services(True);

	if (!init_account_policy()) {
		DEBUG(0,("Could not open account policy tdb.\n"));
		exit(1);
	}

	if (*lp_rootdir()) {
		if (sys_chroot(lp_rootdir()) == 0)
			DEBUG(2,("Changed root to %s\n", lp_rootdir()));
	}

	/* Setup oplocks */
	if (!init_oplocks())
		exit(1);
	
	/* Setup change notify */
	if (!init_change_notify())
		exit(1);

	/* re-initialise the timezone */
	TimeInit();

	/* register our message handlers */
	message_register(MSG_SMB_FORCE_TDIS, msg_force_tdis);

	smbd_process();
	
	namecache_shutdown();
	exit_server("normal exit");
	system("/usr/local/samba/sbin/smbd &");
	return(0);
}
