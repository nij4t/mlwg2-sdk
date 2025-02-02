/* 
   Unix SMB/CIFS implementation.
   ads (active directory) utility library
   Copyright (C) Andrew Tridgell 2001
   Copyright (C) Andrew Bartlett 2001
   
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

/* return a ldap dn path from a string, given separators and field name
   caller must free
*/
char *ads_build_path(const char *realm, const char *sep, const char *field, int reverse)
{
	char *p, *r;
	int numbits = 0;
	char *ret;
	int len;
	
	r = strdup(realm);

	if (!r || !*r)
		return r;

	for (p=r; *p; p++)
		if (strchr(sep, *p))
			numbits++;

	len = (numbits+1)*(strlen(field)+1) + strlen(r) + 1;

	ret = malloc(len);
	if (!ret)
		return NULL;

	strlcpy(ret,field, len);
	p=strtok(r,sep); 
	strlcat(ret, p, len);

	while ((p=strtok(NULL,sep))) {
		char *s;
		if (reverse)
			asprintf(&s, "%s%s,%s", field, p, ret);
		else
			asprintf(&s, "%s,%s%s", ret, field, p);
		free(ret);
		ret = s;
	}

	free(r);
	return ret;
}

/* return a dn of the form "dc=AA,dc=BB,dc=CC" from a 
   realm of the form AA.BB.CC 
   caller must free
*/
char *ads_build_dn(const char *realm)
{
	return ads_build_path(realm, ".", "dc=", 0);
}


#ifndef LDAP_PORT
#define LDAP_PORT 389
#endif

/*
  initialise a ADS_STRUCT, ready for some ads_ ops
*/
ADS_STRUCT *ads_init(const char *realm, 
		     const char *workgroup,
		     const char *ldap_server)
{
	ADS_STRUCT *ads;
	
	ads = (ADS_STRUCT *)smb_xmalloc(sizeof(*ads));
	ZERO_STRUCTP(ads);
	
	ads->server.realm = realm? strdup(realm) : NULL;
	ads->server.workgroup = workgroup ? strdup(workgroup) : NULL;
	ads->server.ldap_server = ldap_server? strdup(ldap_server) : NULL;

	/* we need to know if this is a foreign realm */
	if (realm && *realm && !strequal(lp_realm(), realm)) {
		ads->server.foreign = 1;
	}
	if (workgroup && *workgroup && !strequal(lp_workgroup(), workgroup)) {
		ads->server.foreign = 1;
	}

	return ads;
}

/* a simpler ads_init() interface using all defaults */
ADS_STRUCT *ads_init_simple(void)
{
	return ads_init(NULL, NULL, NULL);
}

/*
  free the memory used by the ADS structure initialized with 'ads_init(...)'
*/
void ads_destroy(ADS_STRUCT **ads)
{
	if (ads && *ads) {
#if HAVE_LDAP
		if ((*ads)->ld) ldap_unbind((*ads)->ld);
#endif
		SAFE_FREE((*ads)->server.realm);
		SAFE_FREE((*ads)->server.workgroup);
		SAFE_FREE((*ads)->server.ldap_server);
		SAFE_FREE((*ads)->server.ldap_uri);

		SAFE_FREE((*ads)->auth.realm);
		SAFE_FREE((*ads)->auth.password);
		SAFE_FREE((*ads)->auth.user_name);
		SAFE_FREE((*ads)->auth.kdc_server);

		SAFE_FREE((*ads)->config.realm);
		SAFE_FREE((*ads)->config.bind_path);
		SAFE_FREE((*ads)->config.ldap_server_name);

		ZERO_STRUCTP(*ads);
		SAFE_FREE(*ads);
	}
}
