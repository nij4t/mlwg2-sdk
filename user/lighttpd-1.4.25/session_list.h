#ifndef _SESSION_H_
#define _SESSION_H_

#include "list.h"

#define L_OK 0
#define L_ERR 1

struct sessionNode
{
    char *ip;
    char sessionID[128];
    char *username;
    int loginTime;    
    int loginLevel; // 0:power, 1:guest, 2:root
    int is_liveview;
    struct list_head node;
};

struct logoutNode
{
    char sessionID[128];
    int reason; //0:new 1:timeout 2:other pc login, 3:logout, 4:login fail, 5:MAX user reached
    char new_ip[16]; //if reason = 2, this ip is a newly login ip 
    struct list_head node;
};

#endif
