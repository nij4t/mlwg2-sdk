#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

struct in_addr client_ip;

struct check2ndssid {
	char *clientMAC;
	int checkflag;
};

#define Check2ndSsidDev "/proc/GTK_Check2ndSsidIO"


char * get_client_mac_form_ip(char *client_ip)
{
	int count=0;
	FILE *arp = NULL;
	char arpip[20],arpmac[20],line[100],arpdevice[20],arpflag[20];

	if ( !( arp = fopen ( "/proc/net/arp" , "r" ) ) ) 
    return	NULL;

	while(fgets( line, 100, arp )!=NULL)
  {
	  if(count!=0)
		{
      sscanf ( line , "%s   0x1  %s  %s   * %s" ,arpip ,arpflag ,arpmac,arpdevice);
			//CGI_DBG("ip=%s ,mac=%s \n",arpip ,arpmac);

 			if (strcasecmp("br0", arpdevice)||!strcasecmp("00:00:00:00:00:00", arpmac)||!strcasecmp("0x0", arpflag))
        continue;
			if(!strcasecmp(arpip, client_ip))
			{
				//CGI_DBG("arpip=%s\n",arpip);
				fclose(arp);
				return arpmac;
			}
		}
		count++;
  }
  return	NULL;
}


int check_2ndssid_client(struct in_addr client_ip)
{
	
	char *tmp = NULL;
	char client_MAC[17]; // 12:34:56:78:90:12
	
	struct check2ndssid ioctl_data;
	int sockfd = 0; 
	int status;
	int result;
	
	tmp = get_client_mac_form_ip(inet_ntoa(client_ip));
										
	if(tmp!=NULL)
		strcpy(client_MAC, tmp);
	
	memset(&ioctl_data, '\0', sizeof(struct check2ndssid));

	sockfd = open ( Check2ndSsidDev , O_RDONLY );
	
	if(sockfd < 0)
	{
		printf("check_2ndssid_client failed!\n");
		return 1; //if failed, block it
	}
	
	ioctl_data.clientMAC = client_MAC;
	
	
	if ( ( status = ioctl ( sockfd , 0 , &ioctl_data ) ) < 0 ) {
			printf ( "check_2ndssid_client :: ioctl failed!\n" );
			close ( sockfd );
			return 1;
	}

	result = ioctl_data.checkflag;
	printf("result=%d\n", result);
	
	close ( sockfd );
	
	return result;
}