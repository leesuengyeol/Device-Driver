#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include "myswitch.h"

void signal_handler(int signum)
{

	if (signum == SIGUSR1)		printf("USER1\n");
	else if (signum == SIGUSR2)	printf("USER2\n");

}


int main(int argc, char** argv)
{
	char buf[BUFSIZ];
	int i=0;
	int fd;
	int count;
	unsigned long temp;

	memset(buf, 0, BUFSIZ);

	signal(SIGUSR1, signal_handler);
	signal(SIGUSR2, signal_handler);
	sprintf(buf, "/dev/%s", GPIO_DEVICE);
	fd=open(buf,O_RDWR);//이과정에서 char D.D 확인
	
	if(fd<0)
	{
		printf("Error open()\n");
		printf("sudo chmod 666 /dev/%s\n", GPIO_DEVICE);
		return -1;	
	}

	sprintf(buf,"%d",getpid()); //:(콜론)을 넣어서 kernel단에서 strsep으로 나눠준다
	count=write(fd,buf, strlen(buf));
	

	if(count<0)
	{
		printf("Error write()\n");
	}
	sleep(1);

	while (1);

	close(fd);
	return 0;

}
