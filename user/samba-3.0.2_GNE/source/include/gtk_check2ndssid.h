#ifndef __GMTK_CHECK2NDSSID_H__
#define __GMTK_CHECK2NDSSID_H__

char * get_client_mac_form_ip(char *client_ip);
int check_2ndssid_client(struct in_addr client_ip);

#endif	
