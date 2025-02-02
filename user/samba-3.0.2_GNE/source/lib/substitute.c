/* 
   Unix SMB/CIFS implementation.
   string substitution functions
   Copyright (C) Andrew Tridgell 1992-2000
   
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

fstring local_machine="";
fstring remote_arch="UNKNOWN";
userdom_struct current_user_info;
fstring remote_proto="UNKNOWN";

static fstring remote_machine;
static fstring smb_user_name;

/** 
 * Set the 'local' machine name
 * @param local_name the name we are being called
 * @param if this is the 'final' name for us, not be be changed again
 */

void set_local_machine_name(const char* local_name, BOOL perm)
{
	static BOOL already_perm = False;
	fstring tmp_local_machine;

	/*
	 * Windows NT/2k uses "*SMBSERVER" and XP uses "*SMBSERV"
	 * arrggg!!! 
	 */

	if (strequal(local_name, "*SMBSERVER")) 
		return;

	if (strequal(local_name, "*SMBSERV")) 
		return;

	if (already_perm)
		return;

	already_perm = perm;

	fstrcpy(tmp_local_machine,local_name);
	trim_char(tmp_local_machine,' ',' ');
	alpha_strcpy(local_machine,tmp_local_machine,SAFE_NETBIOS_CHARS,sizeof(local_machine)-1);
	strlower_m(local_machine);
}

/** 
 * Set the 'remote' machine name
 * @param remote_name the name our client wants to be called by
 * @param if this is the 'final' name for them, not be be changed again
 */

void set_remote_machine_name(const char* remote_name, BOOL perm)
{
	static BOOL already_perm = False;
	fstring tmp_remote_machine;

	if (already_perm)
		return;

	already_perm = perm;

	fstrcpy(tmp_remote_machine,remote_name);
	trim_char(tmp_remote_machine,' ',' ');
	alpha_strcpy(remote_machine,tmp_remote_machine,SAFE_NETBIOS_CHARS,sizeof(remote_machine)-1);
	strlower_m(remote_machine);
}

const char* get_remote_machine_name(void) 
{
	return remote_machine;
}

const char* get_local_machine_name(void) 
{
	if (!*local_machine) {
		return global_myname();
	}

	return local_machine;
}

/*******************************************************************
 Setup the string used by %U substitution.
********************************************************************/

void sub_set_smb_name(const char *name)
{
	fstring tmp;

	/* don't let anonymous logins override the name */
	if (! *name)
		return;

	fstrcpy(tmp,name);
	trim_char(tmp,' ',' ');
	strlower_m(tmp);
	alpha_strcpy(smb_user_name,tmp,SAFE_NETBIOS_CHARS,sizeof(smb_user_name)-1);
}

/*******************************************************************
 Setup the strings used by substitutions. Called per packet. Ensure
 %U name is set correctly also.
********************************************************************/

void set_current_user_info(const userdom_struct *pcui)
{
	current_user_info = *pcui;
	/* The following is safe as current_user_info.smb_name
	 * has already been sanitised in register_vuid. */
	fstrcpy(smb_user_name, current_user_info.smb_name);
}

/*******************************************************************
 Given a pointer to a %$(NAME) expand it as an environment variable.
 Return the number of characters by which the pointer should be advanced.
 Based on code by Branko Cibej <branko.cibej@hermes.si>
 When this is called p points at the '%' character.
********************************************************************/

static size_t expand_env_var(char *p, int len)
{
	fstring envname;
	char *envval;
	char *q, *r;
	int copylen;

	if (p[1] != '$')
		return 1;

	if (p[2] != '(')
		return 2;

	/*
	 * Look for the terminating ')'.
	 */

	if ((q = strchr_m(p,')')) == NULL) {
		DEBUG(0,("expand_env_var: Unterminated environment variable [%s]\n", p));
		return 2;
	}

	/*
	 * Extract the name from within the %$(NAME) string.
	 */

	r = p+3;
	copylen = MIN((q-r),(sizeof(envname)-1));
	strncpy(envname,r,copylen);
	envname[copylen] = '\0';

	if ((envval = getenv(envname)) == NULL) {
		DEBUG(0,("expand_env_var: Environment variable [%s] not set\n", envname));
		return 2;
	}

	/*
	 * Copy the full %$(NAME) into envname so it
	 * can be replaced.
	 */

	copylen = MIN((q+1-p),(sizeof(envname)-1));
	strncpy(envname,p,copylen);
	envname[copylen] = '\0';
	string_sub(p,envname,envval,len);
	return 0; /* Allow the environment contents to be parsed. */
}

/*******************************************************************
 Given a pointer to a %$(NAME) in p and the whole string in str
 expand it as an environment variable.
 Return a new allocated and expanded string.
 Based on code by Branko Cibej <branko.cibej@hermes.si>
 When this is called p points at the '%' character.
 May substitute multiple occurrencies of the same env var.
********************************************************************/


static char * realloc_expand_env_var(char *str, char *p)
{
	char *envname;
	char *envval;
	char *q, *r;
	int copylen;

	if (p[0] != '%' || p[1] != '$' || p[2] != '(')
		return str;

	/*
	 * Look for the terminating ')'.
	 */

	if ((q = strchr_m(p,')')) == NULL) {
		DEBUG(0,("expand_env_var: Unterminated environment variable [%s]\n", p));
		return str;
	}

	/*
	 * Extract the name from within the %$(NAME) string.
	 */

	r = p + 3;
	copylen = q - r;
	envname = (char *)malloc(copylen + 1 + 4); /* reserve space for use later add %$() chars */
	if (envname == NULL) return NULL;
	strncpy(envname,r,copylen);
	envname[copylen] = '\0';

	if ((envval = getenv(envname)) == NULL) {
		DEBUG(0,("expand_env_var: Environment variable [%s] not set\n", envname));
		SAFE_FREE(envname);
		return str;
	}

	/*
	 * Copy the full %$(NAME) into envname so it
	 * can be replaced.
	 */

	copylen = q + 1 - p;
	strncpy(envname,p,copylen);
	envname[copylen] = '\0';
	r = realloc_string_sub(str, envname, envval);
	SAFE_FREE(envname);
	if (r == NULL) return NULL;
	return r;
}

/*******************************************************************
 Patch from jkf@soton.ac.uk
 Added this to implement %p (NIS auto-map version of %H)
*******************************************************************/

static char *automount_path(const char *user_name)
{
	static pstring server_path;

	/* use the passwd entry as the default */
	/* this will be the default if WITH_AUTOMOUNT is not used or fails */

	pstrcpy(server_path, get_user_home_dir(user_name));

#if (defined(HAVE_NETGROUP) && defined (WITH_AUTOMOUNT))

	if (lp_nis_home_map()) {
		char *home_path_start;
		char *automount_value = automount_lookup(user_name);

		if(strlen(automount_value) > 0) {
			home_path_start = strchr_m(automount_value,':');
			if (home_path_start != NULL) {
				DEBUG(5, ("NIS lookup succeeded.  Home path is: %s\n",
						home_path_start?(home_path_start+1):""));
				pstrcpy(server_path, home_path_start+1);
			}
		} else {
			/* NIS key lookup failed: default to user home directory from password file */
			DEBUG(5, ("NIS lookup failed. Using Home path from passwd file. Home path is: %s\n", server_path ));
		}
	}
#endif

	DEBUG(4,("Home server path: %s\n", server_path));

	return server_path;
}

/*******************************************************************
 Patch from jkf@soton.ac.uk
 This is Luke's original function with the NIS lookup code
 moved out to a separate function.
*******************************************************************/

static const char *automount_server(const char *user_name)
{
	static pstring server_name;
	const char *local_machine_name = get_local_machine_name(); 

	/* use the local machine name as the default */
	/* this will be the default if WITH_AUTOMOUNT is not used or fails */
	if (local_machine_name && *local_machine_name)
		pstrcpy(server_name, local_machine_name);
	else
		pstrcpy(server_name, global_myname());
	
#if (defined(HAVE_NETGROUP) && defined (WITH_AUTOMOUNT))

	if (lp_nis_home_map()) {
	        int home_server_len;
		char *automount_value = automount_lookup(user_name);
		home_server_len = strcspn(automount_value,":");
		DEBUG(5, ("NIS lookup succeeded.  Home server length: %d\n",home_server_len));
		if (home_server_len > sizeof(pstring))
			home_server_len = sizeof(pstring);
		strncpy(server_name, automount_value, home_server_len);
                server_name[home_server_len] = '\0';
	}
#endif

	DEBUG(4,("Home server: %s\n", server_name));

	return server_name;
}

/****************************************************************************
 Do some standard substitutions in a string.
 len is the length in bytes of the space allowed in string str. If zero means
 don't allow expansions.
****************************************************************************/

void standard_sub_basic(const char *smb_name, char *str,size_t len)
{
	char *p, *s;
	fstring pidstr;
	struct passwd *pass;
	const char *local_machine_name = get_local_machine_name();

	for (s=str; (p=strchr_m(s, '%'));s=p) {
		fstring tmp_str;

		int l = (int)len - (int)(p-str);

		if (l < 0)
			l = 0;
		
		switch (*(p+1)) {
		case 'U' : 
			fstrcpy(tmp_str, smb_name);
			strlower_m(tmp_str);
			string_sub(p,"%U",tmp_str,l);
			break;
		case 'G' :
			fstrcpy(tmp_str, smb_name);
			if ((pass = Get_Pwnam(tmp_str))!=NULL) {
				string_sub(p,"%G",gidtoname(pass->pw_gid),l);
			} else {
				p += 2;
			}
			break;
		case 'D' :
			fstrcpy(tmp_str, current_user_info.domain);
			strupper_m(tmp_str);
			string_sub(p,"%D", tmp_str,l);
			break;
		case 'I' :
			string_sub(p,"%I", client_addr(),l);
			break;
		case 'i' :
			string_sub(p,"%i", client_socket_addr(),l);
			break;
		case 'L' : 
			if (local_machine_name && *local_machine_name)
				string_sub(p,"%L", local_machine_name,l); 
			else {
				pstring temp_name;

				pstrcpy(temp_name, global_myname());
				strlower_m(temp_name);
				string_sub(p,"%L", temp_name,l); 
			}
			break;
		case 'M' :
			string_sub(p,"%M", client_name(),l);
			break;
		case 'R' :
			string_sub(p,"%R", remote_proto,l);
			break;
		case 'T' :
			string_sub(p,"%T", timestring(False),l);
			break;
		case 'a' :
			string_sub(p,"%a", remote_arch,l);
			break;
		case 'd' :
			slprintf(pidstr,sizeof(pidstr)-1, "%d",(int)sys_getpid());
			string_sub(p,"%d", pidstr,l);
			break;
		case 'h' :
			string_sub(p,"%h", myhostname(),l);
			break;
		case 'm' :
			string_sub(p,"%m", get_remote_machine_name(),l);
			break;
		case 'v' :
			string_sub(p,"%v", SAMBA_VERSION_STRING,l);
			break;
		case '$' :
			p += expand_env_var(p,l);
			break; /* Expand environment variables */
		case '\0': 
			p++; 
			break; /* don't run off the end of the string */
			
		default: p+=2; 
			break;
		}
	}
}

static void standard_sub_advanced(int snum, const char *user, 
				  const char *connectpath, gid_t gid, 
				  const char *smb_name, char *str, size_t len)
{
	char *p, *s, *home;

	for (s=str; (p=strchr_m(s, '%'));s=p) {
		int l = (int)len - (int)(p-str);
	
		if (l < 0)
			l = 0;
	
		switch (*(p+1)) {
		case 'N' :
			string_sub(p,"%N", automount_server(user),l);
			break;
		case 'H':
			if ((home = get_user_home_dir(user)))
				string_sub(p,"%H",home, l);
			else
				p += 2;
			break;
		case 'P': 
			string_sub(p,"%P", connectpath, l); 
			break;
		case 'S': 
			string_sub(p,"%S", lp_servicename(snum), l); 
			break;
		case 'g': 
			string_sub(p,"%g", gidtoname(gid), l); 
			break;
		case 'u': 
			string_sub(p,"%u", user, l); 
			break;
			
			/* Patch from jkf@soton.ac.uk Left the %N (NIS
			 * server name) in standard_sub_basic as it is
			 * a feature for logon servers, hence uses the
			 * username.  The %p (NIS server path) code is
			 * here as it is used instead of the default
			 * "path =" string in [homes] and so needs the
			 * service name, not the username.  */
		case 'p': 
			string_sub(p,"%p", automount_path(lp_servicename(snum)), l); 
			break;
		case '\0': 
			p++; 
			break; /* don't run off the end of the string */
			
		default: p+=2; 
			break;
		}
	}

	standard_sub_basic(smb_name, str, len);
}

/****************************************************************************
 Do some standard substitutions in a string.
 This function will return an allocated string that have to be freed.
****************************************************************************/

char *talloc_sub_basic(TALLOC_CTX *mem_ctx, const char *smb_name, const char *str)
{
	char *a, *t;
       	a = alloc_sub_basic(smb_name, str);
	if (!a) return NULL;
	t = talloc_strdup(mem_ctx, a);
	SAFE_FREE(a);
	return t;
}

char *alloc_sub_basic(const char *smb_name, const char *str)
{
	char *b, *p, *s, *t, *r, *a_string;
	fstring pidstr;
	struct passwd *pass;
	const char *local_machine_name = get_local_machine_name();

	/* workaround to prevent a crash while lookinf at bug #687 */
	
	if ( !str ) {
		DEBUG(0,("alloc_sub_basic: NULL source string!  This should not happen\n"));
		return NULL;
	}
	
	a_string = strdup(str);
	if (a_string == NULL) {
		DEBUG(0, ("alloc_sub_specified: Out of memory!\n"));
		return NULL;
	}
	
	for (b = s = a_string; (p = strchr_m(s, '%')); s = a_string + (p - b)) {

		r = NULL;
		b = t = a_string;
		
		switch (*(p+1)) {
		case 'U' : 
			r = strdup_lower(smb_name);
			if (r == NULL) goto error;
			t = realloc_string_sub(t, "%U", r);
			break;
		case 'G' :
			r = strdup(smb_name);
			if (r == NULL) goto error;
			if ((pass = Get_Pwnam(r))!=NULL) {
				t = realloc_string_sub(t, "%G", gidtoname(pass->pw_gid));
			} 
			break;
		case 'D' :
			r = strdup_upper(current_user_info.domain);
			if (r == NULL) goto error;
			t = realloc_string_sub(t, "%D", r);
			break;
		case 'I' :
			t = realloc_string_sub(t, "%I", client_addr());
			break;
		case 'L' : 
			if (local_machine_name && *local_machine_name)
				t = realloc_string_sub(t, "%L", local_machine_name); 
			else
				t = realloc_string_sub(t, "%L", global_myname()); 
			break;
		case 'N':
			t = realloc_string_sub(t, "%N", automount_server(smb_name));
			break;
		case 'M' :
			t = realloc_string_sub(t, "%M", client_name());
			break;
		case 'R' :
			t = realloc_string_sub(t, "%R", remote_proto);
			break;
		case 'T' :
			t = realloc_string_sub(t, "%T", timestring(False));
			break;
		case 'a' :
			t = realloc_string_sub(t, "%a", remote_arch);
			break;
		case 'd' :
			slprintf(pidstr,sizeof(pidstr)-1, "%d",(int)sys_getpid());
			t = realloc_string_sub(t, "%d", pidstr);
			break;
		case 'h' :
			t = realloc_string_sub(t, "%h", myhostname());
			break;
		case 'm' :
			t = realloc_string_sub(t, "%m", remote_machine);
			break;
		case 'v' :
			t = realloc_string_sub(t, "%v", SAMBA_VERSION_STRING);
			break;
		case '$' :
			t = realloc_expand_env_var(t, p); /* Expand environment variables */
			break;
			
		default: 
			break;
		}

		p++;
		SAFE_FREE(r);
		if (t == NULL) goto error;
		a_string = t;
	}

	return a_string;
error:
	SAFE_FREE(a_string);
	return NULL;
}

/****************************************************************************
 Do some specific substitutions in a string.
 This function will return an allocated string that have to be freed.
****************************************************************************/

char *talloc_sub_specified(TALLOC_CTX *mem_ctx,
			const char *input_string,
			const char *username,
			const char *domain,
			uid_t uid,
			gid_t gid)
{
	char *a, *t;
       	a = alloc_sub_specified(input_string, username, domain, uid, gid);
	if (!a) return NULL;
	t = talloc_strdup(mem_ctx, a);
	SAFE_FREE(a);
	return t;
}

char *alloc_sub_specified(const char *input_string,
			const char *username,
			const char *domain,
			uid_t uid,
			gid_t gid)
{
	char *a_string, *ret_string;
	char *b, *p, *s, *t;

	a_string = strdup(input_string);
	if (a_string == NULL) {
		DEBUG(0, ("alloc_sub_specified: Out of memory!\n"));
		return NULL;
	}
	
	for (b = s = a_string; (p = strchr_m(s, '%')); s = a_string + (p - b)) {
		
		b = t = a_string;
		
		switch (*(p+1)) {
		case 'U' : 
			t = realloc_string_sub(t, "%U", username);
			break;
		case 'u' : 
			t = realloc_string_sub(t, "%u", username);
			break;
		case 'G' :
			if (gid != -1) {
				t = realloc_string_sub(t, "%G", gidtoname(gid));
			} else {
				t = realloc_string_sub(t, "%G", "NO_GROUP");
			}
			break;
		case 'g' :
			if (gid != -1) {
				t = realloc_string_sub(t, "%g", gidtoname(gid));
			} else {
				t = realloc_string_sub(t, "%g", "NO_GROUP");
			}
			break;
		case 'D' :
			t = realloc_string_sub(t, "%D", domain);
			break;
		case 'N' : 
			t = realloc_string_sub(t, "%N", automount_server(username)); 
			break;
		default: 
			break;
		}

		p++;
		if (t == NULL) {
			SAFE_FREE(a_string);
			return NULL;
		}
		a_string = t;
	}

	ret_string = alloc_sub_basic(username, a_string);
	SAFE_FREE(a_string);
	return ret_string;
}

char *talloc_sub_advanced(TALLOC_CTX *mem_ctx,
			int snum,
			const char *user,
			const char *connectpath,
			gid_t gid,
			const char *smb_name,
			const char *str)
{
	char *a, *t;
       	a = alloc_sub_advanced(snum, user, connectpath, gid, smb_name, str);
	if (!a) return NULL;
	t = talloc_strdup(mem_ctx, a);
	SAFE_FREE(a);
	return t;
}

char *alloc_sub_advanced(int snum, const char *user, 
				  const char *connectpath, gid_t gid, 
				  const char *smb_name, const char *str)
{
	char *a_string, *ret_string;
	char *b, *p, *s, *t, *h;

	a_string = strdup(str);
	if (a_string == NULL) {
		DEBUG(0, ("alloc_sub_specified: Out of memory!\n"));
		return NULL;
	}
	
	for (b = s = a_string; (p = strchr_m(s, '%')); s = a_string + (p - b)) {
		
		b = t = a_string;
		
		switch (*(p+1)) {
		case 'N' :
			t = realloc_string_sub(t, "%N", automount_server(user));
			break;
		case 'H':
			if ((h = get_user_home_dir(user)))
				t = realloc_string_sub(t, "%H", h);
			break;
		case 'P': 
			t = realloc_string_sub(t, "%P", connectpath); 
			break;
		case 'S': 
			t = realloc_string_sub(t, "%S", lp_servicename(snum)); 
			break;
		case 'g': 
			t = realloc_string_sub(t, "%g", gidtoname(gid)); 
			break;
		case 'u': 
			t = realloc_string_sub(t, "%u", user); 
			break;
			
			/* Patch from jkf@soton.ac.uk Left the %N (NIS
			 * server name) in standard_sub_basic as it is
			 * a feature for logon servers, hence uses the
			 * username.  The %p (NIS server path) code is
			 * here as it is used instead of the default
			 * "path =" string in [homes] and so needs the
			 * service name, not the username.  */
		case 'p': 
			t = realloc_string_sub(t, "%p", automount_path(lp_servicename(snum))); 
			break;
			
		default: 
			break;
		}

		p++;
		if (t == NULL) {
			SAFE_FREE(a_string);
			return NULL;
		}
		a_string = t;
	}

	ret_string = alloc_sub_basic(smb_name, a_string);
	SAFE_FREE(a_string);
	return ret_string;
}

/****************************************************************************
 Do some standard substitutions in a string.
****************************************************************************/

void standard_sub_conn(connection_struct *conn, char *str, size_t len)
{
	standard_sub_advanced(SNUM(conn), conn->user, conn->connectpath,
			conn->gid, smb_user_name, str, len);
}

char *talloc_sub_conn(TALLOC_CTX *mem_ctx, connection_struct *conn, const char *str)
{
	return talloc_sub_advanced(mem_ctx, SNUM(conn), conn->user,
			conn->connectpath, conn->gid,
			smb_user_name, str);
}

char *alloc_sub_conn(connection_struct *conn, const char *str)
{
	return alloc_sub_advanced(SNUM(conn), conn->user, conn->connectpath,
			conn->gid, smb_user_name, str);
}

/****************************************************************************
 Like standard_sub but by snum.
****************************************************************************/

void standard_sub_snum(int snum, char *str, size_t len)
{
	extern struct current_user current_user;
	static uid_t cached_uid = -1;
	static fstring cached_user;
	/* calling uidtoname() on every substitute would be too expensive, so
	   we cache the result here as nearly every call is for the same uid */

	if (cached_uid != current_user.uid) {
		fstrcpy(cached_user, uidtoname(current_user.uid));
		cached_uid = current_user.uid;
	}

	standard_sub_advanced(snum, cached_user, "", -1,
			      smb_user_name, str, len);
}
