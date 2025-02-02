/* 
   Unix SMB/Netbios implementation.
   Version 3.0
   MSDfs services for Samba
   Copyright (C) Shirish Kalele 2000

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

extern fstring local_machine;
extern uint32 global_client_caps;

/**********************************************************************
  Parse the pathname  of the form \hostname\service\reqpath
  into the dfs_path structure 
 **********************************************************************/

static BOOL parse_dfs_path(char* pathname, struct dfs_path* pdp)
{
	pstring pathname_local;
	char* p,*temp;

	pstrcpy(pathname_local,pathname);
	p = temp = pathname_local;

	ZERO_STRUCTP(pdp);

	trim_char(temp,'\\','\\');
	DEBUG(10,("temp in parse_dfs_path: .%s. after trimming \\'s\n",temp));

	/* now tokenize */
	/* parse out hostname */
	p = strchr(temp,'\\');
	if(p == NULL)
		return False;
	*p = '\0';
	pstrcpy(pdp->hostname,temp);
	DEBUG(10,("hostname: %s\n",pdp->hostname));

	/* parse out servicename */
	temp = p+1;
	p = strchr(temp,'\\');
	if(p == NULL) {
		pstrcpy(pdp->servicename,temp);
		pdp->reqpath[0] = '\0';
		return True;
	}
	*p = '\0';
	pstrcpy(pdp->servicename,temp);
	DEBUG(10,("servicename: %s\n",pdp->servicename));

	/* rest is reqpath */
	pstrcpy(pdp->reqpath, p+1);
	p = pdp->reqpath;
	while (*p) {
		if (*p == '\\') *p = '/';
		p++;
	}

	DEBUG(10,("rest of the path: %s\n",pdp->reqpath));
	return True;
}

/********************************************************
 Fake up a connection struct for the VFS layer.
 Note this CHANGES CWD !!!! JRA.
*********************************************************/

static BOOL create_conn_struct( connection_struct *conn, int snum, char *path)
{
	ZERO_STRUCTP(conn);
	conn->service = snum;
	conn->connectpath = path;
	pstring_sub(conn->connectpath , "%S", lp_servicename(snum));

	/* needed for smbd_vfs_init() */
	
        if ( (conn->mem_ctx=talloc_init("connection_struct")) == NULL ) {
                DEBUG(0,("talloc_init(connection_struct) failed!\n"));
                return False;
        }
	
	if (!smbd_vfs_init(conn)) {
		DEBUG(0,("create_conn_struct: smbd_vfs_init failed.\n"));
		talloc_destroy( conn->mem_ctx );
		return False;
	}
	if (vfs_ChDir(conn,conn->connectpath) != 0) {
		DEBUG(3,("create_conn_struct: Can't ChDir to new conn path %s. Error was %s\n",
					conn->connectpath, strerror(errno) ));
		talloc_destroy( conn->mem_ctx );
		return False;
	}
	return True;
}


/**********************************************************************
 Parse the contents of a symlink to verify if it is an msdfs referral
 A valid referral is of the form: msdfs:server1\share1,server2\share2
 **********************************************************************/
static BOOL parse_symlink(char* buf,struct referral** preflist, 
				 int* refcount)
{
	pstring temp;
	char* prot;
	char* alt_path[MAX_REFERRAL_COUNT];
	int count=0, i;
	struct referral* reflist;

	pstrcpy(temp,buf);
  
	prot = strtok(temp,":");

	if (!strequal(prot, "msdfs"))
		return False;

	/* No referral list requested. Just yes/no. */
	if (!preflist) 
		return True;

	/* parse out the alternate paths */
	while(((alt_path[count] = strtok(NULL,",")) != NULL) && count<MAX_REFERRAL_COUNT)
		count++;

	DEBUG(10,("parse_symlink: count=%d\n", count));

	reflist = *preflist = (struct referral*) malloc(count * sizeof(struct referral));
	if(reflist == NULL) {
		DEBUG(0,("parse_symlink: Malloc failed!\n"));
		return False;
	}
	
	for(i=0;i<count;i++) {
		/* replace / in the alternate path by a \ */
		char* p = strchr(alt_path[i],'/');
		if(p)
			*p = '\\'; 

		pstrcpy(reflist[i].alternate_path, "\\");
		pstrcat(reflist[i].alternate_path, alt_path[i]);
		reflist[i].proximity = 0;
		reflist[i].ttl = REFERRAL_TTL;
		DEBUG(10, ("parse_symlink: Created alt path: %s\n", reflist[i].alternate_path));
	}

	if(refcount)
		*refcount = count;

	return True;
}
 
/**********************************************************************
 Returns true if the unix path is a valid msdfs symlink
 **********************************************************************/

BOOL is_msdfs_link(connection_struct* conn, char * path,
		   struct referral** reflistp, int* refcnt,
		   SMB_STRUCT_STAT *sbufp)
{
	SMB_STRUCT_STAT st;
	pstring referral;
	int referral_len = 0;

	if (!path || !conn)
		return False;

	if (sbufp == NULL)
		sbufp = &st;

	if (SMB_VFS_LSTAT(conn, path, sbufp) != 0) {
		DEBUG(5,("is_msdfs_link: %s does not exist.\n",path));
		return False;
	}
  
	if (S_ISLNK(sbufp->st_mode)) {
		/* open the link and read it */
		referral_len = SMB_VFS_READLINK(conn, path, referral, 
						      sizeof(pstring));
		if (referral_len == -1) {
			DEBUG(0,("is_msdfs_link: Error reading msdfs link %s: %s\n", path, strerror(errno)));
			return False;
		}

		referral[referral_len] = '\0';
		DEBUG(5,("is_msdfs_link: %s -> %s\n",path,referral));
		if (parse_symlink(referral, reflistp, refcnt))
			return True;
	}
	return False;
}

/*****************************************************************
 Used by other functions to decide if a dfs path is remote,
and to get the list of referred locations for that remote path.
 
findfirst_flag: For findfirsts, dfs links themselves are not
redirected, but paths beyond the links are. For normal smb calls,
even dfs links need to be redirected.

self_referralp: clients expect a dfs referral for the same share when
they request referrals for dfs roots on a server. 

consumedcntp: how much of the dfs path is being redirected. the client
should try the remaining path on the redirected server.
		
*****************************************************************/

static BOOL resolve_dfs_path(pstring dfspath, struct dfs_path* dp, 
		      connection_struct* conn,
		      BOOL findfirst_flag,
		      struct referral** reflistpp, int* refcntp,
		      BOOL* self_referralp, int* consumedcntp)
{
	pstring localpath;
	int consumed_level = 1;
	char *p;
	BOOL bad_path = False;
	SMB_STRUCT_STAT sbuf;
	pstring reqpath;

	if (!dp || !conn) {
		DEBUG(1,("resolve_dfs_path: NULL dfs_path* or NULL connection_struct*!\n"));
		return False;
	}

	if (dp->reqpath[0] == '\0') {
		if (self_referralp) {
			DEBUG(6,("resolve_dfs_path: self-referral. returning False\n"));
			*self_referralp = True;
		}
		return False;
	}

	DEBUG(10,("resolve_dfs_path: Conn path = %s req_path = %s\n", conn->connectpath, dp->reqpath));

	unix_convert(dp->reqpath,conn,0,&bad_path,&sbuf);
	/* JRA... should we strlower the last component here.... ? */
	pstrcpy(localpath, dp->reqpath);

	/* check if need to redirect */
	if (is_msdfs_link(conn, localpath, reflistpp, refcntp, NULL)) {
		if (findfirst_flag) {
			DEBUG(6,("resolve_dfs_path (FindFirst) No redirection "
				 "for dfs link %s.\n", dfspath));
			return False;
		} else {		
			DEBUG(6,("resolve_dfs_path: %s resolves to a valid Dfs link.\n",
				 dfspath));
			if (consumedcntp) 
				*consumedcntp = strlen(dfspath);
			return True;
		}
	} 

	/* redirect if any component in the path is a link */
	pstrcpy(reqpath, dp->reqpath);
	p = strrchr(reqpath, '/');
	while (p) {
		*p = '\0';
		pstrcpy(localpath, reqpath);
		if (is_msdfs_link(conn, localpath, reflistpp, refcntp, NULL)) {
			DEBUG(4, ("resolve_dfs_path: Redirecting %s because parent %s is dfs link\n", dfspath, localpath));

			/* To find the path consumed, we truncate the original
			   DFS pathname passed to use to remove the last
			   component. The length of the resulting string is
			   the path consumed 
			*/
			if (consumedcntp) {
				char *q;
				pstring buf;
				pstrcpy(buf, dfspath);
				trim_char(buf, '\0', '\\');
				for (; consumed_level; consumed_level--) {
					q = strrchr(buf, '\\');
					if (q)
						*q = 0;
				}
				*consumedcntp = strlen(buf);
				DEBUG(10, ("resolve_dfs_path: Path consumed: %s (%d)\n", buf, *consumedcntp));
			}
			
			return True;
		}
		p = strrchr(reqpath, '/');
		consumed_level++;
	}
	
	return False;
}

/*****************************************************************
  Decides if a dfs pathname should be redirected or not.
  If not, the pathname is converted to a tcon-relative local unix path
*****************************************************************/

BOOL dfs_redirect(pstring pathname, connection_struct* conn,
		  BOOL findfirst_flag)
{
	struct dfs_path dp;
	
	if (!conn || !pathname)
		return False;

	parse_dfs_path(pathname, &dp);

	/* if dfs pathname for a non-dfs share, convert to tcon-relative
	   path and return false */
	if (!lp_msdfs_root(SNUM(conn))) {
		pstrcpy(pathname, dp.reqpath);
		return False;
	}
	
	if (!strequal(dp.servicename, lp_servicename(SNUM(conn)) )) 
		return False;

	if (resolve_dfs_path(pathname, &dp, conn, findfirst_flag,
			     NULL, NULL, NULL, NULL)) {
		DEBUG(3,("dfs_redirect: Redirecting %s\n", pathname));
		return True;
	} else {
		DEBUG(3,("dfs_redirect: Not redirecting %s.\n", pathname));
		
		/* Form non-dfs tcon-relative path */
		pstrcpy(pathname, dp.reqpath);
		DEBUG(3,("dfs_redirect: Path converted to non-dfs path %s\n",
			 pathname));
		return False;
	}

	/* never reached */
}

/**********************************************************************
 Gets valid referrals for a dfs path and fills up the
 junction_map structure
 **********************************************************************/

BOOL get_referred_path(char *pathname, struct junction_map* jucn,
		       int* consumedcntp, BOOL* self_referralp)
{
	struct dfs_path dp;

	struct connection_struct conns;
	struct connection_struct* conn = &conns;
	pstring conn_path;
	int snum;
	BOOL ret = False;

	BOOL self_referral = False;

	if (!pathname || !jucn)
		return False;

	if (self_referralp)
		*self_referralp = False;
	else
		self_referralp = &self_referral;

	parse_dfs_path(pathname, &dp);

	/* Verify hostname in path */
	if (local_machine && (!strequal(local_machine, dp.hostname))) {
		/* Hostname mismatch, check if one of our IP addresses */
		if (!ismyip(*interpret_addr2(dp.hostname))) {
			DEBUG(3, ("get_referred_path: Invalid hostname %s in path %s\n",
				dp.hostname, pathname));
			return False;
		}
	}

	pstrcpy(jucn->service_name, dp.servicename);
	pstrcpy(jucn->volume_name, dp.reqpath);

	/* Verify the share is a dfs root */
	snum = lp_servicenumber(jucn->service_name);
	if(snum < 0) {
		if ((snum = find_service(jucn->service_name)) < 0)
			return False;
	}
	
	pstrcpy(conn_path, lp_pathname(snum));
	if (!create_conn_struct(conn, snum, conn_path))
		return False;

	if (!lp_msdfs_root(SNUM(conn))) {
		DEBUG(3,("get_referred_path: .%s. in dfs path %s is not a dfs root.\n",
			 dp.servicename, pathname));
		goto out;
	}

	if (*lp_msdfs_proxy(snum) != '\0') {
		struct referral* ref;
		jucn->referral_count = 1;
		if ((ref = (struct referral*) malloc(sizeof(struct referral))) == NULL) {
			DEBUG(0, ("malloc failed for referral\n"));
			goto out;
		}

		pstrcpy(ref->alternate_path, lp_msdfs_proxy(snum));
		if (dp.reqpath[0] != '\0')
			pstrcat(ref->alternate_path, dp.reqpath);
		ref->proximity = 0;
		ref->ttl = REFERRAL_TTL;
		jucn->referral_list = ref;
		if (consumedcntp)
			*consumedcntp = strlen(pathname);
		ret = True;
		goto out;
	}

	/* If not remote & not a self referral, return False */
	if (!resolve_dfs_path(pathname, &dp, conn, False, 
			      &jucn->referral_list, &jucn->referral_count,
			      self_referralp, consumedcntp)) {
		if (!*self_referralp) {
			DEBUG(3,("get_referred_path: No valid referrals for path %s\n", pathname));
			goto out;
		}
	}
	
	/* if self_referral, fill up the junction map */
	if (*self_referralp) {
		struct referral* ref;
		jucn->referral_count = 1;
		if((ref = (struct referral*) malloc(sizeof(struct referral))) == NULL) {
			DEBUG(0,("malloc failed for referral\n"));
			goto out;
		}
      
		pstrcpy(ref->alternate_path,pathname);
		ref->proximity = 0;
		ref->ttl = REFERRAL_TTL;
		jucn->referral_list = ref;
		if (consumedcntp)
			*consumedcntp = strlen(pathname);
	}
	
	ret = True;
out:
	talloc_destroy( conn->mem_ctx );
	
	return ret;
}

static int setup_ver2_dfs_referral(char* pathname, char** ppdata, 
				   struct junction_map* junction,
				   int consumedcnt,
				   BOOL self_referral)
{
	char* pdata = *ppdata;

	unsigned char uni_requestedpath[1024];
	int uni_reqpathoffset1,uni_reqpathoffset2;
	int uni_curroffset;
	int requestedpathlen=0;
	int offset;
	int reply_size = 0;
	int i=0;

	DEBUG(10,("setting up version2 referral\nRequested path:\n"));

	requestedpathlen = rpcstr_push(uni_requestedpath, pathname, -1,
				       STR_TERMINATE);

	dump_data(10, (const char *) uni_requestedpath,requestedpathlen);

	DEBUG(10,("ref count = %u\n",junction->referral_count));

	uni_reqpathoffset1 = REFERRAL_HEADER_SIZE + 
			VERSION2_REFERRAL_SIZE * junction->referral_count;

	uni_reqpathoffset2 = uni_reqpathoffset1 + requestedpathlen;

	uni_curroffset = uni_reqpathoffset2 + requestedpathlen;

	reply_size = REFERRAL_HEADER_SIZE + VERSION2_REFERRAL_SIZE*junction->referral_count +
					2 * requestedpathlen;
	DEBUG(10,("reply_size: %u\n",reply_size));

	/* add up the unicode lengths of all the referral paths */
	for(i=0;i<junction->referral_count;i++) {
		DEBUG(10,("referral %u : %s\n",i,junction->referral_list[i].alternate_path));
		reply_size += (strlen(junction->referral_list[i].alternate_path)+1)*2;
	}

	DEBUG(10,("reply_size = %u\n",reply_size));
	/* add the unexplained 0x16 bytes */
	reply_size += 0x16;

	pdata = Realloc(pdata,reply_size);
	if(pdata == NULL) {
		DEBUG(0,("malloc failed for Realloc!\n"));
		return -1;
	} else
		*ppdata = pdata;

	/* copy in the dfs requested paths.. required for offset calculations */
	memcpy(pdata+uni_reqpathoffset1,uni_requestedpath,requestedpathlen);
	memcpy(pdata+uni_reqpathoffset2,uni_requestedpath,requestedpathlen);

	/* create the header */
	SSVAL(pdata,0,consumedcnt * 2); /* path consumed */
	SSVAL(pdata,2,junction->referral_count); /* number of referral in this pkt */
	if(self_referral)
		SIVAL(pdata,4,DFSREF_REFERRAL_SERVER | DFSREF_STORAGE_SERVER); 
	else
		SIVAL(pdata,4,DFSREF_STORAGE_SERVER);

	offset = 8;
	/* add the referral elements */
	for(i=0;i<junction->referral_count;i++) {
		struct referral* ref = &(junction->referral_list[i]);
		int unilen;

		SSVAL(pdata,offset,2); /* version 2 */
		SSVAL(pdata,offset+2,VERSION2_REFERRAL_SIZE);
		if(self_referral)
			SSVAL(pdata,offset+4,1);
		else
			SSVAL(pdata,offset+4,0);
		SSVAL(pdata,offset+6,0); /* ref_flags :use path_consumed bytes? */
		SIVAL(pdata,offset+8,ref->proximity);
		SIVAL(pdata,offset+12,ref->ttl);

		SSVAL(pdata,offset+16,uni_reqpathoffset1-offset);
		SSVAL(pdata,offset+18,uni_reqpathoffset2-offset);
		/* copy referred path into current offset */
		unilen = rpcstr_push(pdata+uni_curroffset, ref->alternate_path,
				     -1, STR_UNICODE);

		SSVAL(pdata,offset+20,uni_curroffset-offset);

		uni_curroffset += unilen;
		offset += VERSION2_REFERRAL_SIZE;
	}
	/* add in the unexplained 22 (0x16) bytes at the end */
	memset(pdata+uni_curroffset,'\0',0x16);
	return reply_size;
}

static int setup_ver3_dfs_referral(char* pathname, char** ppdata, 
				   struct junction_map* junction,
				   int consumedcnt,
				   BOOL self_referral)
{
	char* pdata = *ppdata;

	unsigned char uni_reqpath[1024];
	int uni_reqpathoffset1, uni_reqpathoffset2;
	int uni_curroffset;
	int reply_size = 0;

	int reqpathlen = 0;
	int offset,i=0;
	
	DEBUG(10,("setting up version3 referral\n"));

	reqpathlen = rpcstr_push(uni_reqpath, pathname, -1, STR_TERMINATE);
	
	dump_data(10, (char *) uni_reqpath,reqpathlen);

	uni_reqpathoffset1 = REFERRAL_HEADER_SIZE + VERSION3_REFERRAL_SIZE * junction->referral_count;
	uni_reqpathoffset2 = uni_reqpathoffset1 + reqpathlen;
	reply_size = uni_curroffset = uni_reqpathoffset2 + reqpathlen;

	for(i=0;i<junction->referral_count;i++) {
		DEBUG(10,("referral %u : %s\n",i,junction->referral_list[i].alternate_path));
		reply_size += (strlen(junction->referral_list[i].alternate_path)+1)*2;
	}

	pdata = Realloc(pdata,reply_size);
	if(pdata == NULL) {
		DEBUG(0,("version3 referral setup: malloc failed for Realloc!\n"));
		return -1;
	} else
		*ppdata = pdata;

	/* create the header */
	SSVAL(pdata,0,consumedcnt * 2); /* path consumed */
	SSVAL(pdata,2,junction->referral_count); /* number of referral */
	if(self_referral)
		SIVAL(pdata,4,DFSREF_REFERRAL_SERVER | DFSREF_STORAGE_SERVER); 
	else
		SIVAL(pdata,4,DFSREF_STORAGE_SERVER);
	
	/* copy in the reqpaths */
	memcpy(pdata+uni_reqpathoffset1,uni_reqpath,reqpathlen);
	memcpy(pdata+uni_reqpathoffset2,uni_reqpath,reqpathlen);
	
	offset = 8;
	for(i=0;i<junction->referral_count;i++) {
		struct referral* ref = &(junction->referral_list[i]);
		int unilen;

		SSVAL(pdata,offset,3); /* version 3 */
		SSVAL(pdata,offset+2,VERSION3_REFERRAL_SIZE);
		if(self_referral)
			SSVAL(pdata,offset+4,1);
		else
			SSVAL(pdata,offset+4,0);

		SSVAL(pdata,offset+6,0); /* ref_flags :use path_consumed bytes? */
		SIVAL(pdata,offset+8,ref->ttl);
	    
		SSVAL(pdata,offset+12,uni_reqpathoffset1-offset);
		SSVAL(pdata,offset+14,uni_reqpathoffset2-offset);
		/* copy referred path into current offset */
		unilen = rpcstr_push(pdata+uni_curroffset,ref->alternate_path,
				     -1, STR_UNICODE | STR_TERMINATE);
		SSVAL(pdata,offset+16,uni_curroffset-offset);
		/* copy 0x10 bytes of 00's in the ServiceSite GUID */
		memset(pdata+offset+18,'\0',16);

		uni_curroffset += unilen;
		offset += VERSION3_REFERRAL_SIZE;
	}
	return reply_size;
}

/******************************************************************
 * Set up the Dfs referral for the dfs pathname
 ******************************************************************/

int setup_dfs_referral(connection_struct *orig_conn, char *pathname, int max_referral_level, char** ppdata)
{
	struct junction_map junction;
	int consumedcnt;
	BOOL self_referral = False;
	pstring buf;
	int reply_size = 0;
	char *pathnamep = pathname;

	ZERO_STRUCT(junction);

	/* get the junction entry */
	if (!pathnamep)
		return -1;

	/* Trim pathname sent by client so it begins with only one backslash.
	   Two backslashes confuse some dfs clients
	 */
	while (pathnamep[0] == '\\' && pathnamep[1] == '\\')
		pathnamep++;

	pstrcpy(buf, pathnamep);
	/* The following call can change cwd. */
	if (!get_referred_path(buf, &junction, &consumedcnt, &self_referral)) {
		vfs_ChDir(orig_conn,orig_conn->connectpath);
		return -1;
	}
	vfs_ChDir(orig_conn,orig_conn->connectpath);
	
	if (!self_referral) {
		pathnamep[consumedcnt] = '\0';

		if( DEBUGLVL( 3 ) ) {
			int i=0;
			dbgtext("setup_dfs_referral: Path %s to alternate path(s):",pathnamep);
			for(i=0;i<junction.referral_count;i++)
				dbgtext(" %s",junction.referral_list[i].alternate_path);
			dbgtext(".\n");
		}
	}

	/* create the referral depeding on version */
	DEBUG(10,("max_referral_level :%d\n",max_referral_level));
	if(max_referral_level<2 || max_referral_level>3)
		max_referral_level = 2;

	switch(max_referral_level) {
	case 2:
		reply_size = setup_ver2_dfs_referral(pathnamep, ppdata, &junction, 
						     consumedcnt, self_referral);
		SAFE_FREE(junction.referral_list);
		break;
	case 3:
		reply_size = setup_ver3_dfs_referral(pathnamep, ppdata, &junction, 
						     consumedcnt, self_referral);
		SAFE_FREE(junction.referral_list);
		break;
	default:
		DEBUG(0,("setup_dfs_referral: Invalid dfs referral version: %d\n", max_referral_level));
		return -1;
	}
      
	DEBUG(10,("DFS Referral pdata:\n"));
	dump_data(10,*ppdata,reply_size);
	return reply_size;
}

/**********************************************************************
 The following functions are called by the NETDFS RPC pipe functions
 **********************************************************************/

/**********************************************************************
 Creates a junction structure from a Dfs pathname
 **********************************************************************/
BOOL create_junction(char* pathname, struct junction_map* jucn)
{
        struct dfs_path dp;
 
        parse_dfs_path(pathname,&dp);

        /* check if path is dfs : validate first token */
        if (local_machine && (!strequal(local_machine,dp.hostname))) {
	    
	   /* Hostname mismatch, check if one of our IP addresses */
	   if (!ismyip(*interpret_addr2(dp.hostname))) {
                DEBUG(4,("create_junction: Invalid hostname %s in dfs path %s\n",
			 dp.hostname, pathname));
                return False;
	   }
        }

        /* Check for a non-DFS share */
        if(!lp_msdfs_root(lp_servicenumber(dp.servicename))) {
                DEBUG(4,("create_junction: %s is not an msdfs root.\n", 
			 dp.servicename));
                return False;
        }

        pstrcpy(jucn->service_name,dp.servicename);
        pstrcpy(jucn->volume_name,dp.reqpath);
        return True;
}

/**********************************************************************
 Forms a valid Unix pathname from the junction 
 **********************************************************************/

static BOOL junction_to_local_path(struct junction_map* jucn, char* path,
				   int max_pathlen, connection_struct *conn)
{
	int snum;
	pstring conn_path;

	if(!path || !jucn)
		return False;

	snum = lp_servicenumber(jucn->service_name);
	if(snum < 0)
		return False;

	safe_strcpy(path, lp_pathname(snum), max_pathlen-1);
	safe_strcat(path, "/", max_pathlen-1);
	safe_strcat(path, jucn->volume_name, max_pathlen-1);

	pstrcpy(conn_path, lp_pathname(snum));
	if (!create_conn_struct(conn, snum, conn_path))
		return False;

	return True;
}

BOOL create_msdfs_link(struct junction_map* jucn, BOOL exists)
{
	pstring path;
	pstring msdfs_link;
	connection_struct conns;
 	connection_struct *conn = &conns;
	int i=0;
	BOOL insert_comma = False;
	BOOL ret = False;

	if(!junction_to_local_path(jucn, path, sizeof(path), conn))
		return False;
  
	/* form the msdfs_link contents */
	pstrcpy(msdfs_link, "msdfs:");
	for(i=0; i<jucn->referral_count; i++) {
		char* refpath = jucn->referral_list[i].alternate_path;
      
		trim_char(refpath, '\\', '\\');
		if(*refpath == '\0') {
			if (i == 0)
				insert_comma = False;
			continue;
		}
		if (i > 0 && insert_comma)
			pstrcat(msdfs_link, ",");

		pstrcat(msdfs_link, refpath);
		if (!insert_comma)
			insert_comma = True;
		
	}

	DEBUG(5,("create_msdfs_link: Creating new msdfs link: %s -> %s\n", path, msdfs_link));

	if(exists)
		if(SMB_VFS_UNLINK(conn,path)!=0)
			goto out;

	if(SMB_VFS_SYMLINK(conn, msdfs_link, path) < 0) {
		DEBUG(1,("create_msdfs_link: symlink failed %s -> %s\nError: %s\n", 
				path, msdfs_link, strerror(errno)));
		goto out;
	}
	
	
	ret = True;
	
out:
	talloc_destroy( conn->mem_ctx );
	return ret;
}

BOOL remove_msdfs_link(struct junction_map* jucn)
{
	pstring path;
	connection_struct conns;
 	connection_struct *conn = &conns;
	BOOL ret = False;

	if( junction_to_local_path(jucn, path, sizeof(path), conn) ) {
		if( SMB_VFS_UNLINK(conn, path) == 0 )
			ret = True;

		talloc_destroy( conn->mem_ctx );
	}
	
	return ret;
}

static BOOL form_junctions(int snum, struct junction_map* jucn, int* jn_count)
{
	int cnt = *jn_count;
	DIR *dirp;
	char* dname;
	pstring connect_path;
	char* service_name = lp_servicename(snum);
	connection_struct conns;
	connection_struct *conn = &conns;
	struct referral *ref = NULL;
	BOOL ret = False;
 
	pstrcpy(connect_path,lp_pathname(snum));

	if(*connect_path == '\0')
		return False;

	/*
	 * Fake up a connection struct for the VFS layer.
	 */

	if (!create_conn_struct(conn, snum, connect_path))
		return False;

	/* form a junction for the msdfs root - convention 
	   DO NOT REMOVE THIS: NT clients will not work with us
	   if this is not present
	*/ 
	pstrcpy(jucn[cnt].service_name, service_name);
	jucn[cnt].volume_name[0] = '\0';
	jucn[cnt].referral_count = 1;

	ref = jucn[cnt].referral_list
		= (struct referral*) malloc(sizeof(struct referral));
	if (jucn[cnt].referral_list == NULL) {
		DEBUG(0, ("Malloc failed!\n"));
		goto out;
	}

	ref->proximity = 0;
	ref->ttl = REFERRAL_TTL;
	if (*lp_msdfs_proxy(snum) != '\0') {
		pstrcpy(ref->alternate_path, lp_msdfs_proxy(snum));
		*jn_count = ++cnt;
		ret = True;
		goto out;
	}
		
	slprintf(ref->alternate_path, sizeof(pstring)-1,
		 "\\\\%s\\%s", local_machine, service_name);
	cnt++;
	
	/* Now enumerate all dfs links */
	dirp = SMB_VFS_OPENDIR(conn, ".");
	if(!dirp)
		goto out;

	while((dname = vfs_readdirname(conn, dirp)) != NULL) {
		if (is_msdfs_link(conn, dname, &(jucn[cnt].referral_list),
				  &(jucn[cnt].referral_count), NULL)) {
			pstrcpy(jucn[cnt].service_name, service_name);
			pstrcpy(jucn[cnt].volume_name, dname);
			cnt++;
		}
	}
	
	SMB_VFS_CLOSEDIR(conn,dirp);
	*jn_count = cnt;
out:
	talloc_destroy(conn->mem_ctx);
	return ret;
}

int enum_msdfs_links(struct junction_map* jucn)
{
	int i=0;
	int jn_count = 0;

	if(!lp_host_msdfs())
		return 0;

	for(i=0;i < lp_numservices();i++) {
		if(lp_msdfs_root(i)) 
			form_junctions(i,jucn,&jn_count);
	}
	return jn_count;
}
