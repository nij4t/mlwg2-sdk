#include "session_list.h"

int
session_list_register(list_head *list, char *ip, int logintime, char *username, char *powerlevel, char *sSessionID)
{
    struct session_node * a_node;
    if (!(a_node = (struct session_node *) malloc(sizeof(struct session_node))))
    {
        printf("malloc fail \n");
        return L_ERR;
    }
    memset(a_node, 0, sizeof(struct session_node));

    if (ip != NULL && username != NULL && powerlevel != NULL) {
        a_node->ip = (char *) malloc(strlen(ip) + 1);
        strcpy(a_node->ip, ip);
        a_node->username = (char *) malloc(strlen(username) + 1);
        strcpy(a_node->username, username);
        a_node->powerLevel = (char *) malloc(strlen(powerLevel) + 1);
        strcpy(a_node->powerLevel, powerLevel);
        sprintf(a_node->sessionID, "%s", sSessionID);
    } else {
        if (a_node) free(a_node);
        return L_ERR;
    }
    a_node->loginTime = logintime;

    list_add_tail(&a_node->node, &list);

    return L_OK;
}

int print_list(list_head *list)
{
    struct session_node *curr_session;
    int i=1;
    printf("=============================== \n");
    printf("order,      IP,       username,       sessionID,      logintime,         powerlevel \n");
    list_for_each_entry(curr_session,&list,node)
    {
        printf("%d,     %s,       %s,           %s,        %d,       %s            \n",i++, curr_session->ip,curr_session->username, curr_session->sessionID, curr_session->loginTime, curr_session->powerLevel);
    }
    printf("=============================== \n");

    return L_OK;
}

struct session_node *find_session_by_sessionID(list_head *list, char *in_sessionID)
{
    struct session_node *curr_session;

    list_for_each_entry(curr_session,&list,node)
    {
        if(strcmp(curr_session->sessionID, in_sessionID) == 0)
                return curr_session;
    }
    return NULL;
}

int check_powerLevel(server *srv, char *in_sessionID)
{
    struct sessionNode *curr_session;

    list_for_each_entry(curr_session,&list,node)
    {
        if(strcmp(curr_session->username, in_sessionID) == 0)
            return curr_session->loginLevel;
    }
    return -1;

}


buffer * cache_export_cookie_params(server *srv, connection *con) {
	data_unset *d;

	buffer *session_id;
    session_id = buffer_init();
	UNUSED(srv);

	if (NULL != (d = array_get_element(con->request.headers, "Cookie"))) {
		data_string *ds = (data_string *)d;
		size_t key = 0, value = 0;
		size_t is_key = 1, is_sid = 0;
		size_t i;

		/* found COOKIE */
		if (!DATA_IS_STRING(d)) return -1;
		if (ds->value->used == 0) return -1;

		if (ds->value->ptr[0] == '\0' ||
		    ds->value->ptr[0] == '=' ||
		    ds->value->ptr[0] == ';') return -1;

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
    log_error_write(srv, __FILE__, __LINE__, "sb",
            "-- find session in cache_export_cookie_params--", session_id);
	return session_id;
}
