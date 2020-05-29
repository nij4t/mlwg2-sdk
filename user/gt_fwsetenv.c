#include "gt_fwsetenv.h"

/*
Gemtek +++ 20110816 --- by Roy Wu

for Ralink Uboot, choose '4' to enter command line mode
'printenv' to show current Uboot env values
'setenv inspection 1 (or 0)' to add a new "inspection" variable
'saveenv' to write Uboot env values to flash memory
this C program can locate Uboot env mtd partition in firmware and 
	change value of Uboot env variable for special purpose

*/

void setenv(int type)
{
	int config_fd;
	char wbuf[MTDSIZE+1], rbuf[MTDSIZE+1];
	int wret, rret;
	int fi, ec;
	
	config_fd = open(MTDPATH, O_RDWR);
	
	if (config_fd == -1)
		printf("Open mtd failed...do nothing\n");
	else
	{
		memset(rbuf, '\0', MTDSIZE+1);
		fi = 0;
		ec = 0;
				
		rret = read(config_fd, rbuf, MTDSIZE);
		if (rret == -1)
		{
			printf("Read mtd failed...do nothing\n");
			return;
		}

		while (fi < CHKLENGTH) // check only first 1024 bytes (characters) to save time
		{
			if (rbuf[fi] == '=')
			{
				ec++;
				//printf("ec=[%d]\n", ec);
				//if (ec > 3) // it should be located after position 3, modify this if needed
				{
					if (!strncmp( rbuf+fi-10, "inspection", 10))
					{
						printf("Got inspection!!\n");
						if (type == 1)
							rbuf[fi+1] = '1';
						else
							rbuf[fi+1] = '0';	
						break;
					}
				}
			}
						
			fi++;	
		}
		if (fi == CHKLENGTH)
		{
			printf("Can't find inspection...do nothing\n");
			return;
		}
					
		system("mtd_write erase mtd1");//Ralink utility for erasing mtd
		sleep(1);
					
		lseek(config_fd, 0, SEEK_SET);
					
		wret = write(config_fd, &rbuf, MTDSIZE);
		if (wret == -1)
		{
			printf("Write mtd failed...\n");
		}
		
		fsync(config_fd);

		close(config_fd);
	}
	

}


void main(int argc, char *argv[])
{

		if (argc != 2)
		{
			fprintf(stderr, "gt_fwsetenv...argc error!\n");
			fprintf(stderr, "usage:\n");
			fprintf(stderr, "	gt_fwsetenv [inspection_on/inspection_off]\n");
			return;
		}

		if(!strcmp(argv[1], "inspection_on"))
		{
			fprintf(stderr, "gt_fwsetenv : set inspection on\n");
			setenv(1);
		}
		else if(!strcmp(argv[1], "inspection_off"))
		{
			fprintf(stderr, "gt_fwsetenv : set inspection off\n");
			setenv(0);
		}
		else if(!strcmp(argv[1], "help"))
		{
			fprintf(stderr, "usage:\n");
			fprintf(stderr, "	gt_fwsetenv [inspection_on/inspection_off]\n");
		}
		else
		{
			fprintf(stderr, "usage:\n");
			fprintf(stderr, "	gt_fwsetenv [inspection_on/inspection_off]\n");
		}	
			

}