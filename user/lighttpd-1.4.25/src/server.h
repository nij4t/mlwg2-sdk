#ifndef _SERVER_H_
#define _SERVER_H_

#include "base.h"
#include "session_list.h" 

#define SESSIONCHECK 35
#define USERMODIFY 55
#define SESSION_ID_SIZE 64
#define LOGINFAIL 36

typedef struct {
	char *key;
	char *value;
} two_strings;

typedef enum { CONFIG_UNSET, CONFIG_DOCUMENT_ROOT } config_var_t;

int config_read(server *srv, const char *fn);
int config_set_defaults(server *srv);
buffer *config_get_value_buffer(server *srv, connection *con, config_var_t field);
extern int check_session;
extern int user_modify;
extern int login_fail;

typedef struct session_handler_t
{
    int status; // 1: in use; 0: free session
    char sessionid[SESSION_ID_SIZE];
    char username[SESSION_ID_SIZE];
    unsigned long login_time;
    int loginLevel;
    int iNeedSession;   //1: need add Session , 0: do need add session
}session_handler_t;

extern struct list_head list;
extern struct list_head logout_list;
//LIST_HEAD(list);

#endif
