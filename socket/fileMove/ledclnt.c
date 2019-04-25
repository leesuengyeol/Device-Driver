//멀티쓰레드 led , HC-SR04, image 받기 client , server 파일이름 : multithreadServ.c

//멀티 프로세서를 이용하여 read와 write를 나누어 동시에 읽고 쓸수 있도록 구현

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "raspi.h"


void error_handling(char *message);
void read_routine(int sock, char *buf);
void write_routine(int sock, char *buf);
struct Data data; //raspi.h

int main(int argc, char *argv[])
{
	int sock;
	pid_t pid;
	char buf[BUF_SIZE];
	struct sockaddr_in serv_adr;

	if (argc != 3) {
		printf("Usage : %s <IP> <port>\n", argv[0]);
		exit(1);
	}

	//통신 방법과 ip등 정의
	sock = socket(PF_INET, SOCK_STREAM, 0);
	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_addr.s_addr = inet_addr(argv[1]);
	serv_adr.sin_port = htons(atoi(argv[2]));

	//server에 연결요청
	if (connect(sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == -1)
		error_handling("connect() error!");

	pid = fork(); //프로세스 만들기
	if (pid == 0)
		write_routine(sock, buf);	//쓰기 (server에 data송신)
	else
		read_routine(sock, buf);	//읽기 (server에 data수신)

	close(sock);
	return 0;
}

void read_routine(int sock, char *buf)
{
	int read_cnt;
	FILE* fp;
	char filebuf[BUF_SIZE];
	while (1)
	{
		int str_len = read(sock, &data, sizeof(data));
		if (str_len == 0)
			return;

		switch (data.cmd)
		{
			//write_routine에서 요청한 HC04의 거리데이터를 수신
			case WR_DIST: printf("data.hc04_dist=%f\n", data.hc04_dist);
				break;
			//write_routine에서 요청한 HC04의 거리데이터를 수신
			case WR_IMG: 
				fp = fopen("reslut.jpg", "w+b"); //w+b binary로 쓰기 , 파일이 있다면 덮어쓰기
				while ((read_cnt = read(sock, filebuf, BUF_SIZE)) != 0)
				{
					fwrite((void*)filebuf, 1, read_cnt, fp);
					if (read_cnt < BUF_SIZE) //받아온 싸이즈가 버퍼보다 더크면 = 다받아옴 종료시키고 
						break;
				}
				fclose(fp);	//file close
				printf("File Write is done\n");
				break;
		}
	}
}

void write_routine(int sock, char *buf)
{
	data.led_Value = 0;
	while (1)
	{
		//led값 제어
		if (data.led_Value == 1)
			data.led_Value = 0;
		else
			data.led_Value = 1;

		//서버에 LED값 변경을 위한 요청 데이터 보내기
		data.cmd = WR_LED;
		printf("data.led_Value = %d \n", data.led_Value);
		write(sock, &data, sizeof(data));
		
		//서버에 HC04 거리 요청 데이터 보내기
		data.cmd = RD_HC04;
		write(sock, &data, sizeof(data));

		//image 요청 데이터
		data.cmd = RD_IMG;
		write(sock, &data, sizeof(data));

		sleep(5);
	}
		
	
}
void error_handling(char *message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}