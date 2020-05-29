#include <ctype.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/sysinfo.h>
#include "base.h"
#include "server.h"
#include "log.h"
#include "buffer.h"
#include "mod_session.h"
#include "plugin.h"
#include <unistd.h>
//#include <shutils.h>
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


/**
 * this is a session for a lighttpd plugin
 *
 * just replaces every occurance of 'session' by your plugin name
 *
 * e.g. in vim:
 *
 *   :%s/session/myhandler/
 *
 */


/* plugin config for all request/connections */

//LIST_HEAD(list);
#define MAX_USER 4

/*
struct previliege_handler verify_pages[] = {
    //{ "login.htm", 0/1 },
#include "power_pages.h"
    { NULL, 0}
};
*/

typedef struct {

    unsigned short session_timeout;
} plugin_config;

typedef struct {
	PLUGIN_DATA;

	plugin_config **config_storage;

	plugin_config conf;
} plugin_data;


/* init the plugin data */
INIT_FUNC(mod_session_init) {
	plugin_data *p;

	p = calloc(1, sizeof(*p));

	return p;
}

/* detroy the plugin data */
FREE_FUNC(mod_session_free) {
	plugin_data *p = p_d;

	UNUSED(srv);

	if (!p) return HANDLER_GO_ON;

	if (p->config_storage) {
		size_t i;

		for (i = 0; i < srv->config_context->used; i++) {
			plugin_config *s = p->config_storage[i];

			if (!s) continue;
			free(s);
		}
		free(p->config_storage);
	}

	free(p);

	return HANDLER_GO_ON;
}

/* handle plugin config and check values */

SETDEFAULTS_FUNC(mod_session_set_defaults) {
	plugin_data *p = p_d;
	size_t i;

	config_values_t cv[] = {
		{ "session.session-timeout",   NULL, T_CONFIG_SHORT, T_CONFIG_SCOPE_CONNECTION },
		{ NULL,                        NULL, T_CONFIG_UNSET, T_CONFIG_SCOPE_UNSET }
	};

	if (!p) return HANDLER_ERROR;

	p->config_storage = calloc(1, srv->config_context->used * sizeof(specific_config *));

	for (i = 0; i < srv->config_context->used; i++) {
		plugin_config *s;

		s = calloc(1, sizeof(plugin_config));
		s->session_timeout = DEFAULT_TIMEOUT;
		cv[0].destination = &(s->session_timeout);
		p->config_storage[i] = s;

		if (0 != config_insert_values_global(srv, ((data_config *)srv->config_context->data[i])->value, cv)) {
			return HANDLER_ERROR;
		}
	}

	return HANDLER_GO_ON;
}

#if 0
#define PATCH(x) \
	p->conf.x = s->x;
static int mod_session_patch_connection(server *srv, connection *con, plugin_data *p) 
{
	size_t i, j;
	plugin_config *s = p->config_storage[0];

	PATCH(session_timeout);

	/* skip the first, the global context */
	for (i = 1; i < srv->config_context->used; i++) {
		data_config *dc = (data_config *)srv->config_context->data[i];
		s = p->config_storage[i];

		/* condition didn't match */
		if (!config_check_cond(srv, con, dc)) continue;

		/* merge config */
		for (j = 0; j < dc->value->used; j++) {
			data_unset *du = dc->value->data[j];

			if (buffer_is_equal_string(du->key, CONST_STR_LEN("session.array"))) {
				PATCH(match);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("session.session-timeout"))) {
				PATCH(session_timeout);
			}		
		}
	}

	return 0;
}
#undef PATCH
#endif

int
login_list_register(server *srv, char *sSessionID, int reason, char *in_ip)
{
	log_error_write(srv, __FILE__, __LINE__, "ssd", "-- login_list_register called -- ", sSessionID,reason);
	struct logoutNode * a_node;
	
	if (!(a_node = (struct logoutNode *) malloc(sizeof(struct logoutNode)))) {
		printf("malloc fail \n");
		return L_ERR;
	}
	memset(a_node, 0, sizeof(struct logoutNode));

	if (sSessionID != NULL) {
		sprintf(a_node->sessionID, "%s", sSessionID);
		a_node->reason = reason;
		sprintf(a_node->new_ip, in_ip);
	} else {
		if (a_node) free(a_node);
		return L_ERR;
	}

	list_add_tail(&a_node->node, &logout_list);
	return L_OK;
}

int 
find_logoutNupdate(server *srv, char *in_sessionID, int reason, char *in_ip)
{
	log_error_write(srv, __FILE__, __LINE__, "s", "-- find_logoutNupdate called -- ");
	struct logoutNode *curr_node;

	list_for_each_entry(curr_node,&logout_list,node) {
		log_error_write(srv, __FILE__, __LINE__, "ss", "-- find_logoutNupdate called -- ", curr_node->sessionID);

		if(strcmp(curr_node->sessionID, in_sessionID) == 0) {
			log_error_write(srv, __FILE__, __LINE__, "s", "-- find_logoutNupdate entry FOUND!! -- ");
			curr_node->reason = reason;
			log_error_write(srv, __FILE__, __LINE__, "s", "-- find_logoutNupdate reason updated!! -- ");
			strcpy(curr_node->new_ip, in_ip);
			log_error_write(srv, __FILE__, __LINE__, "s", "-- find_logoutNupdate entry ip updated!! -- ");
			return 1;
		}
    	}
	return 0;

}

int
session_list_register(server *srv, char *ip, int logintime, char *username, int in_loginLevel, char *sSessionID)
{
	struct sessionNode * a_node;
	log_error_write(srv, __FILE__, __LINE__, "s", "-- session_list_register called -- ");

	find_ipNdel(srv, ip, sSessionID);

	if (!(a_node = (struct sessionNode *) malloc(sizeof(struct sessionNode)))) {
		printf("malloc fail \n");
		return L_ERR;
	}
	memset(a_node, 0, sizeof(struct sessionNode));

	if (ip != NULL && username != NULL) {
		a_node->ip = (char *) malloc(strlen(ip) + 1);
		strcpy(a_node->ip, ip);
		a_node->username = (char *) malloc(strlen(username) + 1);
		strcpy(a_node->username, username);
		a_node->loginLevel = in_loginLevel;
		sprintf(a_node->sessionID, "%s", sSessionID);
    
	} else {
		if (a_node) free(a_node);
		return L_ERR;
	}
	a_node->loginTime = logintime;

	list_add_tail(&a_node->node, &list);

	return L_OK;
}

int print_logout_list(server *srv)
{
	struct logoutNode *curr_session;
	int i=1;
    
	log_error_write(srv, __FILE__, __LINE__, "s","=============================== ");
	log_error_write(srv, __FILE__, __LINE__, "s","order,      sessionID,      reason,         ip");
	list_for_each_entry(curr_session,&logout_list,node) {
		log_error_write(srv, __FILE__, __LINE__, "dsds",i++, curr_session->sessionID, curr_session->reason, curr_session->new_ip);
	}
	log_error_write(srv, __FILE__, __LINE__, "s", "=============================== ");

	return L_OK;
}

int print_list(server *srv)
{
	struct sessionNode *curr_session;
	int i=1;
    
	log_error_write(srv, __FILE__, __LINE__, "s","=============================== ");
	log_error_write(srv, __FILE__, __LINE__, "s","order,      IP,       username,       sessionID,      logintime,         powerlevel");
	list_for_each_entry(curr_session,&list,node) {
		log_error_write(srv, __FILE__, __LINE__, "dsssdd",i++, curr_session->ip,curr_session->username, curr_session->sessionID, curr_session->loginTime, curr_session->loginLevel);
	}
	log_error_write(srv, __FILE__, __LINE__, "s", "=============================== ");
 
	return L_OK;
}

struct sessionNode *find_session_by_sessionID(server *srv, char *in_sessionID)
{

	log_error_write(srv, __FILE__, __LINE__, "s", "-- in find_session_by_sessionID ");
    
	struct sessionNode *curr_session;

	list_for_each_entry(curr_session,&list,node) {
		log_error_write(srv, __FILE__, __LINE__, "ss", "-- find ", curr_session->ip);
		log_error_write(srv, __FILE__, __LINE__, "ssss", "-- curr_session->sessionID= ", curr_session->sessionID, ", in_sessionID =",in_sessionID);
        
	if(strcmp(curr_session->sessionID, in_sessionID) == 0)
		return curr_session;
	}
	return NULL;
}

static int verify_password_expired(char request_uri[])
{

	if(strstr(request_uri, ".htm")) {
		if ( !strstr(request_uri, "index.htm") && !strstr(request_uri, "getting_start.htm") 
		&& !strstr(request_uri, "password_aging.htm") && !strstr(request_uri, "login.htm")  ) {
			if (access("lighttpd.expired", R_OK) == 0)
				return 0;
		}
	}

	return 1;
}

struct sessionNode *find_session_by_username(server *srv, char *in_username)
{
    
	struct sessionNode *curr_session;

	list_for_each_entry(curr_session,&list,node) {
		log_error_write(srv, __FILE__, __LINE__, "ss", "-- find session by username", curr_session->username);
		log_error_write(srv, __FILE__, __LINE__, "ssss", "-- curr_session->username= ", curr_session->username, ", in_username =",in_username);
        
		if(strcmp(curr_session->username, in_username) == 0)
                	return curr_session;
	}
	return NULL;
}

static int check_powerLevel(server *srv, char *in_sessionID)
{
	struct sessionNode *curr_session;

	list_for_each_entry(curr_session,&list,node) {
		if(strcmp(curr_session->sessionID, in_sessionID) == 0)
			return curr_session->loginLevel;
	}
    
	return -1;

}

int get_total_curr_users(server *srv)
{
	struct sessionNode *curr_node;
	struct list_head *position, *q;
	int total_curr_users = 0;
    
	list_for_each_safe(position, q, &list) {
		curr_node = NULL;
		curr_node=list_entry(position, struct sessionNode, node);
		if (curr_node != NULL)
			total_curr_users ++;
	}
	log_error_write(srv, __FILE__, __LINE__, "sd", "-- total current users", total_curr_users);
    
	return total_curr_users;

}

int find_ipNdel(server *srv, char *in_ip, char *in_sessionid)
{
	struct sessionNode *curr_node;
	struct list_head *position, *q;
    
	list_for_each_safe(position, q, &list) {
        
		curr_node = NULL;
		curr_node=list_entry(position, struct sessionNode, node);
        
		if (curr_node != NULL && strcmp(curr_node->ip, in_ip) == 0 && strcmp(curr_node->sessionID, in_sessionid) == 0) {
			list_del(position);
			free(curr_node);
			return 1;
		}
	}
    
	return 0;
}

int find_sessionNdel(server *srv, char *in_sessionid)
{
	struct sessionNode *curr_node;
	struct list_head *position, *q;
    
	list_for_each_safe(position, q, &list) {
		curr_node = NULL;
		curr_node=list_entry(position, struct sessionNode, node);
		if (curr_node != NULL && strcmp(curr_node->sessionID, in_sessionid) == 0) {
			list_del(position);
			free(curr_node);
			system("rm /tmp/httpd_session");
			return 1;
		}
	}
    
	return 0;
}

static int check_exception_uri(char file_name[])
{
	
	if (strstr(file_name, "login.cgi") || strstr(file_name, "ajax.cgi") 
	|| strstr(file_name, "getCurrentStatus.htm") || strstr(file_name, "getWPSStatus.htm"))
		return 1;
	else
		return 0;
}

int find_nameNdel(server *srv, char *in_userName)
{
	struct sessionNode *curr_node;
	struct list_head *position, *q;
	int hit=0;
    
	list_for_each_safe(position, q, &list) {
		curr_node = NULL;
		curr_node=list_entry(position, struct sessionNode, node);
        
		if (curr_node != NULL && strcmp(curr_node->username, in_userName) == 0) {
			list_del(position);
			free(curr_node);
			hit = 1;
		}
	}
    
	if (hit == 1)
		return 1;
	else 
		return 0;
}

static int do_user_account_modify_handle(server *srv, char *in_session)
{
    
	struct sessionNode *curr_node;
	int ret = 0;
    
	curr_node = find_session_by_sessionID(srv,in_session);
    
	if (user_modify == 1) {
		char *modified_user = NULL;
		modified_user = file2str("/tmp/modified.user");
		
		if (find_nameNdel(srv, modified_user) == 1) {
			log_error_write(srv, __FILE__, __LINE__, "s", "-- delete the modified user!!!! --");
			if (modified_user != NULL) free(modified_user);
			//reset it!
			user_modify = 0;
			if (curr_node->loginLevel == 2) //if modify is made by admin, can't redirect to main
				return 0;
			else
				return 1;
		} 
		else {
			log_error_write(srv, __FILE__, __LINE__, "s", "-- delete the modified user NOT found!! --");
			if (modified_user != NULL) free(modified_user);
			//reset it!
			user_modify = 0;
			return 0;
		}
	}
    
	return ret;
}

//static char* cache_export_cookie_params(server *srv, connection *con)
void cache_export_cookie_params(server *srv, connection *con, char session_out[]) 
{
	
	data_unset *d;
	//char session_out[128];
	buffer *session_id;
	session_id = buffer_init();
	UNUSED(srv);

	memset(session_out, 0, sizeof(session_out));

	if (NULL != (d = array_get_element(con->request.headers, "Cookie"))) {
		data_string *ds = (data_string *)d;
		size_t key = 0, value = 0;
		size_t is_key = 1, is_sid = 0;
		size_t i;

		/* found COOKIE */
		if (!DATA_IS_STRING(d)) {
			buffer_free(session_id);
			return;
		}
		if (ds->value->used == 0) {
			buffer_free(session_id);
			return;
		}
		if (ds->value->ptr[0] == '\0' ||
		    ds->value->ptr[0] == '=' ||
		    ds->value->ptr[0] == ';') {
			buffer_free(session_id);
			return;
		}
		//log_error_write(srv, __FILE__, __LINE__, "sb", "-- Cookie = ", ds->value);

		buffer_reset(session_id);
		for (i = 0; i < ds->value->used; i++) {
			switch(ds->value->ptr[i]) {
			case '=':
				if (is_key) {
					if (0 == strncmp(ds->value->ptr + key, "HSESSIONID", i - key)) {
						/* found PHP-session-id-key */
						is_sid = 1;
					}
					value = i + 1;
					is_key = 0;
				}
				break;
			case ';':
				if (is_sid) {
					buffer_copy_string_len(session_id, ds->value->ptr + value, i - value);
				}

				is_sid = 0;
				key = i + 1;
				value = 0;
				is_key = 1;
				break;
			case ' ':
				if (is_key == 1 && key == i) key = i + 1;
				if (is_key == 0 && value == i) value = i + 1;
				break;
			case '\0':
				if (is_sid) {
					buffer_copy_string_len(session_id, ds->value->ptr + value, i - value);
				}
				/* fin */
				break;
			}
		}
	}
	/*
	else
		log_error_write(srv, __FILE__, __LINE__, "s", "-- No session cookie found!!--");
	*/
	if (session_id->ptr != NULL) {
		sprintf(session_out, "%s", session_id->ptr);
		//sprintf(session_out, "%s", (char*)session_id);
		buffer_free(session_id);    
		return;
		//return session_out;     
	} 
	else {
		buffer_free(session_id);
		return;    
		//return NULL;
	}
}

static int
cleanup_idle_user(server *srv, int timeout_sec)
{
	struct sysinfo info;
	char tmp[128];
	struct sessionNode *curr_node;
	struct list_head *position, *q;
	int hit=0;
	bzero(&info, sizeof(sysinfo));
	sysinfo(&info);

	list_for_each_safe(position, q, &list) {
        
		curr_node = NULL;
		curr_node=list_entry(position, struct sessionNode, node);
       	 
		if (curr_node != NULL && ((info.uptime - curr_node->loginTime) > timeout_sec)) {
		//if(curr_node != NULL) {
			memset(tmp, 0x0, sizeof(tmp));
			sprintf(tmp, "%d", info.uptime - curr_node->loginTime);

			if (find_logoutNupdate(srv, curr_node->sessionID, 1, tmp) ==0) {
				login_list_register(srv, curr_node->sessionID, 1, tmp);
			}

			list_del(position);
			free(curr_node);
			hit = 1;
			//return 1;
		}
	}
   	
	 
	if (hit == 1)
        	return 1;
	else
		return 0;
}

static int
do_session_auth(server *srv, connection *con, char *sSessionID, int timeout_sec)
{
    char tmp_buf[1024];
    memset(tmp_buf, 0, sizeof(tmp_buf));

    sprintf(tmp_buf, "%s", con->uri.path->ptr);
    if(strlen(tmp_buf) < 1)
	return 1;

    if (strstr(tmp_buf, "pre_out.htm")) {   
        log_error_write(srv, __FILE__, __LINE__, "s","-- log out!");
        find_sessionNdel(srv, sSessionID);
        find_logoutNupdate(srv, sSessionID, 3, "NA");
        return 0;
    }

    if (strcmp(tmp_buf, "/") != 0) { //avoid not-check "/".
        if ((strstr(tmp_buf, ".htm") == NULL && strstr(tmp_buf, "cgi_main.cgi") == NULL) ) { 
	// only check for htm pages and cgis, but not login.cgi
            return 1;
        }
    }

    
    log_error_write(srv, __FILE__, __LINE__, "s","------ verify password expired!!!!!!");
    if (verify_password_expired(tmp_buf) != 1) {
        log_error_write(srv, __FILE__, __LINE__, "s","-- password expired!!");
        find_sessionNdel(srv, sSessionID);
        return 0;
    }
    
    struct sessionNode *curr_session=NULL;

    curr_session = find_session_by_sessionID(srv,sSessionID);

    if (strlen(sSessionID) > 0 && curr_session != NULL) {
	struct sysinfo info;
	time_t now_secs;
	char tmp_ip[8];

	memset(tmp_ip, 0, sizeof(tmp_ip));
        /* get system uptime, for count timeout*/
        bzero(&info, sizeof(sysinfo));
        sysinfo(&info);

        //check Timeout
        now_secs = info.uptime;

	log_error_write(srv, __FILE__, __LINE__, "sd","-- timeout_sec is  ", timeout_sec);
	log_error_write(srv, __FILE__, __LINE__, "sdsd","-- time out ", now_secs, "-", curr_session->loginTime);
	
        //do not check timeout for login.htm
        if ( curr_session->loginTime >0 && timeout_sec != 0 && (now_secs - curr_session->loginTime) > timeout_sec 
	&& strstr(tmp_buf, "login.htm") == NULL) {

		//log_error_write(srv, __FILE__, __LINE__, "sdsd","-- time out ", now_secs, "-", curr_session->loginTime);
		sprintf(tmp_ip, "%d", now_secs - curr_session->loginTime);
		if (find_logoutNupdate(srv, sSessionID, 1, tmp_ip) ==0) {
			login_list_register(srv, sSessionID, 1, tmp_ip);
		}
		find_sessionNdel(srv, sSessionID);
		log_error_write(srv, __FILE__, __LINE__, "s","-- timeout !");
		return 0; //timeout
        }
	else {
		//log_error_write(srv, __FILE__, __LINE__, "s","-- no timeout !!!");

		curr_session->loginTime = now_secs;

		return 1;
        }
    } 
    else {
	/*
        if (strlen(sSessionID) > 0)
            log_error_write(srv, __FILE__, __LINE__, "s","-- (strlen(sSessionID) > 0) ");

        if (curr_session == NULL)
            log_error_write(srv, __FILE__, __LINE__, "s","-- (find_session_by_sessionID(sSessionID) == NULL)");
	*/
        return 0;
    }

}

URIHANDLER_FUNC(mod_session_uri_handler) 
{

	plugin_data *p = p_d;
	data_string *ds;
	int session_timeout_sec;
	size_t k, i;
	char session_from_header[128];
	//char *session_from_header = NULL;


	UNUSED(srv);
    
	if (con->mode != DIRECT) return HANDLER_GO_ON;
	if (con->uri.path->used == 0) return HANDLER_GO_ON;
   
//	char *buf = NULL;
//	buf = file2str("/tmp/lighttpd.session_timeout");
//	if(buf) {
//		session_timeout_sec = atoi(buf);
//		free(buf);
//	}
//	else
		session_timeout_sec = DEFAULT_TIMEOUT;

    
	memset(session_from_header, 0, sizeof(session_from_header));
	//session_from_header = cache_export_cookie_params(srv, con);
	cache_export_cookie_params(srv, con, session_from_header);  	 
  
	/*
	if(strlen(session_from_header) == 0) {
		check_session = 0;
		return HANDLER_GO_ON;
	}
	*/

	char tmp_uri[128];
	memset(tmp_uri, 0, sizeof(tmp_uri));
	sprintf(tmp_uri, "%s", con->uri.path->ptr);	

	if(check_exception_uri(tmp_uri) == 0 && check_session != 1) {
		if (do_session_auth(srv, con, session_from_header, session_timeout_sec) == 0) {
			//buffer_copy_string(con->uri.path, "/login.htm"); 
			check_session = 0;
			return HANDLER_GO_ON;
		}
	}
	
	if(check_session == 1) {
		session_handler_t session_handler;
		int iNeedSession=0;
		struct sessionNode *curr_session;
		struct sessionNode *find_session;
		char *sSessionData=NULL;
		char sLine[128], *pNext;
		char *pWord=NULL;
		char loginIP[128];
		
		memset(loginIP, 0x0, sizeof(loginIP));
		sprintf(loginIP, "%s",inet_ntop_cache_get_ip(srv, &(con->dst_addr)));

		if (access(HTTPD_SESSION_PATH, R_OK) == 0) {
			sSessionData = file2str(HTTPD_SESSION_PATH);
			if(sSessionData) {
				foreachby(sLine, sSessionData, "\n", pNext) {
					if((pWord = strstr(sLine, "status=")) != NULL) { 
						pWord += 7;
						session_handler.status = atoi(pWord);
						log_error_write(srv, __FILE__, __LINE__, "sd", "status = ", session_handler.status);
					}
					else if((pWord = strstr(sLine, "login_time=")) != NULL) {
						pWord += 11;
						session_handler.login_time = atoi(pWord);
						log_error_write(srv, __FILE__, __LINE__, "sd", "login_time = ", 
						session_handler.login_time);
					}
					else if((pWord = strstr(sLine, "sessionid=")) != NULL) {  
						pWord += 10;
						strcpy(session_handler.sessionid, pWord);
						log_error_write(srv, __FILE__, __LINE__, "ss", "sessionid = ", 
						session_handler.sessionid);
					}
					else if((pWord = strstr(sLine, "username=")) != NULL) { 
						pWord += 9;
						strcpy(session_handler.username, pWord);
						log_error_write(srv, __FILE__, __LINE__, "ss", "username = ", 
						session_handler.username);
					}
					else if((pWord = strstr(sLine, "level=")) != NULL) { 
						pWord += 6;
						session_handler.loginLevel = atoi(pWord);
						log_error_write(srv, __FILE__, __LINE__, "sd", "level = ", 
						session_handler.loginLevel);
					}
				}	
				free(sSessionData);

				if (strlen(session_from_header) == 0) {
					if (NULL == (ds = (data_string *)array_get_unused_element(con->response.headers, 
					TYPE_STRING))) {
						ds = data_response_init();
					}
					buffer_copy_string_len(ds->key, CONST_STR_LEN("Set-Cookie"));
					buffer_append_string_len(ds->value, CONST_STR_LEN("HSESSIONID"));
					buffer_append_string_len(ds->value, CONST_STR_LEN("="));
					buffer_append_string(ds->value, session_handler.sessionid);
					array_insert_unique(con->response.headers, (data_unset *)ds);   
					check_session = 0;

					log_error_write(srv, __FILE__, __LINE__, "ss", "-- Set header=", session_handler.sessionid);
					log_error_write(srv, __FILE__, __LINE__, "s", "set-Cookie");
				}
				
    				/* check for max users */ 
    				if (get_total_curr_users(srv) == MAX_USER) {
        				cleanup_idle_user(srv, session_timeout_sec);
        				if (find_logoutNupdate(srv, session_handler.sessionid, 5, "NA") ==0) {
            					login_list_register(srv, session_handler.sessionid, 5, "NA");
        				}
        				buffer_copy_string(con->uri.path, "/login.htm"); 
        				check_session = 0;
        				return HANDLER_GO_ON;
    				}
				
				/*  IMPORTANT: Only "admin" will be checked for duplicated login, other user will not need!
				*  1.1 if username is in link list
				*      1.1.1 if sessionid is the same
				*       do nothing
				*      1.1.2 sessionid is different -> update sessionid (kick out!)
				* 1.2 username is not in the list
				*      1.2.1 if session exist -> use switch computer -> update username
				*      1.2.2 if session does exist -> no user no sessionid -> add a new entry
				*/

				if (session_handler.loginLevel != 2) {
					log_error_write(srv, __FILE__, __LINE__, "s", "not admin login");
					curr_session = NULL;
					curr_session = find_session_by_sessionID(srv,session_handler.sessionid);
					if (curr_session == NULL) { //no username && no sessionid found, register!
						if (find_logoutNupdate(srv, session_handler.sessionid, 0, "NA") ==0) {
							login_list_register(srv, session_handler.sessionid, 0, "NA");
						}
						if (session_list_register(srv, loginIP, session_handler.login_time, 
						session_handler.username, session_handler.loginLevel, 
						session_handler.sessionid) == L_ERR) {
							log_error_write(srv, __FILE__, __LINE__, "s", "session register FAIL!!! ");
						}
						log_error_write(srv, __FILE__, __LINE__, "s", "register_success");
					} 
					else {
						log_error_write(srv, __FILE__, __LINE__, "s", "Before update user ");
						if (curr_session->username) free (curr_session->username);
							log_error_write(srv, __FILE__, __LINE__, "s", "update user I ");
						curr_session->username = (char *) malloc(strlen(session_handler.username) + 1);
						log_error_write(srv, __FILE__, __LINE__, "s", "update user II ");
						strcpy(curr_session->username, session_handler.username);
						log_error_write(srv, __FILE__, __LINE__, "s", "after update user ");
					}
					check_session = 0;
					return HANDLER_GO_ON;
    				}

				curr_session = NULL;  
				curr_session = find_session_by_username(srv,session_handler.username);    
				if (curr_session != NULL) {
					/* user:session pair doesn't match, update! */
        				if (strcmp(session_handler.sessionid, curr_session->sessionID) != 0) { 
            				/*first check there has any data entry for this un-matched sessionID, find out and del it!*/
            					
						log_error_write(srv, __FILE__, __LINE__, "s", "kick out!");
						find_sessionNdel(srv, curr_session->sessionID);
						find_logoutNupdate(srv, curr_session->sessionID, 2, "NA");
											
						if (find_logoutNupdate(srv, session_handler.sessionid, 0, "NA") ==0){
							login_list_register(srv, session_handler.sessionid, 0, "NA");
						}
						if (session_list_register(srv, loginIP, session_handler.login_time,
						session_handler.username, session_handler.loginLevel,
						session_handler.sessionid) == L_ERR)
							log_error_write(srv, __FILE__, __LINE__, "s", "session register FAIL!!! ");
					}
					else { 
						//username : sessionid match ! do nothing !
						log_error_write(srv, __FILE__, __LINE__, "s", 
						"username:sessionid match !nothing happen!");
						curr_session->loginTime = session_handler.login_time;
						curr_session->loginLevel = session_handler.loginLevel;
					}
				}
				else {
					curr_session = NULL;
					curr_session = find_session_by_sessionID(srv,session_handler.sessionid);
					if (curr_session == NULL) { //no username && no sessionid found, register!
						if (find_logoutNupdate(srv, session_handler.sessionid, 0, "NA") ==0){
							login_list_register(srv, session_handler.sessionid, 0, "NA");
						}
						if (session_list_register(srv, loginIP, session_handler.login_time, 
						session_handler.username, session_handler.loginLevel, 
						session_handler.sessionid) == L_ERR)
							log_error_write(srv, __FILE__, __LINE__, "s", "session register FAIL!!! ");

						log_error_write(srv, __FILE__, __LINE__, "s", "register_success");
					} 
					else {//no sessionid but has username, update the sessionid:username pair
						log_error_write(srv, __FILE__, __LINE__, "s", "session  data updated - II");
						if (find_logoutNupdate(srv, curr_session->sessionID, 2, loginIP)==0)
							login_list_register(srv, curr_session->sessionID, 2, loginIP);
						strcpy(curr_session->sessionID, session_handler.sessionid);
						curr_session->loginTime = session_handler.login_time;
						curr_session->loginLevel = session_handler.loginLevel;
					}
				}
			}
		}
	}

	check_session = 0;
	return HANDLER_GO_ON;
	
}

/* this function is called at dlopen() time and inits the callbacks */

int mod_session_plugin_init(plugin *p) 
{
	p->version     = LIGHTTPD_VERSION_ID;
	p->name        = buffer_init_string("session");

	p->init        = mod_session_init;
	p->handle_uri_clean  = mod_session_uri_handler;
	p->set_defaults  = mod_session_set_defaults;
	p->cleanup     = mod_session_free;

	p->data        = NULL;

	return 0;
}
