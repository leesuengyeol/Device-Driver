#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include "myioctl.h"

void signal_handler(int signum)
{
	static int count;
	printf("user app: signal is catched\n");
	if(signum == SIGIO)
	{
		printf("count:%d\n",count);
	}
	count++;
}


int main(int argc, char** argv)
{
	char buf[BUFSIZ];
	int i=0;
	int fd;
	int count;
	unsigned long temp;

	memset(buf, 0, BUFSIZ);

	signal(SIGIO, signal_handler);

	printf("argv[1]:%s\n", argv[1]);
	fd=open("/dev/gpio_led",O_RDWR);
	if(fd<0)
	{
		printf("Error open()\n");
		return -1;	
	}

	sprintf(buf,"%s:%d",argv[1],getpid()); //:(콜론)을 넣어서 kernel단에서 strsep으로 나눠준다
	count=write(fd,buf, strlen(buf));
	

	if(count<0)
	{
		printf("Error write()\n");
	}
	sleep(1);
	
	//count=read(fd,buf,10);
	//printf("Read data:%s\n",buf);
	
	//Device Driver : file*, cmd, arg
	temp=ioctl(fd, CMD1, 10);
	printf("temp date: %ld\n", temp);
	temp=ioctl(fd,CMD2, 25);
	printf("temp date: %ld\n", temp);
	temp=ioctl(fd,CMD3, 125);
	printf("temp date: %ld\n", temp);


	close(fd);
	return 0;

}
