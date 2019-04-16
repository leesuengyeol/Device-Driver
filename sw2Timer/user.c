#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include "myswitch.h"

static int fd;

void signal_handler(int signum)
{
	char buf[BUFSIZ];
	if (signum == SIGUSR1)
	{
		read(fd, buf, 10);
		printf("time : %s \n", buf);
	}
	else if (signum == SIGUSR2)	printf("time stop\n");

}


int main(int argc, char** argv)
{
	char buf[BUFSIZ];
	int i=0;
	int count;
	unsigned long temp;

	memset(buf, 0, BUFSIZ);

	signal(SIGUSR1, signal_handler);
	signal(SIGUSR2, signal_handler);
	sprintf(buf, "/dev/%s", GPIO_DEVICE);
	fd=open(buf,O_RDWR);//�̰������� char D.D Ȯ��
	
	if(fd<0)
	{
		printf("Error open()\n");
		printf("sudo chmod 666 /dev/%s\n", GPIO_DEVICE);
		return -1;	
	}

	sprintf(buf,"%d",getpid()); //:(�ݷ�)�� �־ kernel�ܿ��� strsep���� �����ش�
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
