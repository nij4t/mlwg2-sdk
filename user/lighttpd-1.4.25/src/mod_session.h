#ifndef _MOD_SESSION_H_
#define _MOD_SESSION_H_

#include "session_list.h"
#define HTTPD_SESSION_PATH "/tmp/httpd_session"
#include "server.h"

#define DEFAULT_TIMEOUT 600
#define GTKCORE_PATH "/tmp/gtkcore.sock"

/* Copy each token in wordlist delimited by delim into word */
#define foreachby(word, wordlist, delim, next) \
        for (next = &wordlist[strspn(wordlist, delim)], \
             strncpy(word, next, sizeof(word)), \
             word[strcspn(word, delim)] = '\0', \
             word[sizeof(word) - 1] = '\0', \
             next = strstr(next, delim); \
             strlen(word); \
             next = next ? &next[strspn(next, delim)] : "", \
             strncpy(word, next, sizeof(word)), \
             word[strcspn(word, delim)] = '\0', \
             word[sizeof(word) - 1] = '\0', \
             next = strstr(next, delim))


/*
typedef struct session_handler_t
{
    int status; // 1: in use; 0: free session
    char sessionid[SESSION_ID_SIZE];
    char username[SESSION_ID_SIZE];
    unsigned long login_time;
    int loginLevel;
    int iNeedSession;   //1: need add Session , 0: do need add session
}session_handler_t;
*/
struct previliege_handler
{
    char *page_name;
    int level;
};


#endif
