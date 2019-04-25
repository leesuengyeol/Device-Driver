//��Ƽ������ led , HC-SR04, image �ޱ� client , server �����̸� : multithreadServ.c

//��Ƽ ���μ����� �̿��Ͽ� read�� write�� ������ ���ÿ� �а� ���� �ֵ��� ����

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

	//��� ����� ip�� ����
	sock = socket(PF_INET, SOCK_STREAM, 0);
	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_addr.s_addr = inet_addr(argv[1]);
	serv_adr.sin_port = htons(atoi(argv[2]));

	//server�� �����û
	if (connect(sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == -1)
		error_handling("connect() error!");

	pid = fork(); //���μ��� �����
	if (pid == 0)
		write_routine(sock, buf);	//���� (server�� data�۽�)
	else
		read_routine(sock, buf);	//�б� (server�� data����)

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
			//write_routine���� ��û�� HC04�� �Ÿ������͸� ����
			case WR_DIST: printf("data.hc04_dist=%f\n", data.hc04_dist);
				break;
			//write_routine���� ��û�� HC04�� �Ÿ������͸� ����
			case WR_IMG: 
				fp = fopen("reslut.jpg", "w+b"); //w+b binary�� ���� , ������ �ִٸ� �����
				while ((read_cnt = read(sock, filebuf, BUF_SIZE)) != 0)
				{
					fwrite((void*)filebuf, 1, read_cnt, fp);
					if (read_cnt < BUF_SIZE) //�޾ƿ� ����� ���ۺ��� ��ũ�� = �ٹ޾ƿ� �����Ű�� 
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
		//led�� ����
		if (data.led_Value == 1)
			data.led_Value = 0;
		else
			data.led_Value = 1;

		//������ LED�� ������ ���� ��û ������ ������
		data.cmd = WR_LED;
		printf("data.led_Value = %d \n", data.led_Value);
		write(sock, &data, sizeof(data));
		
		//������ HC04 �Ÿ� ��û ������ ������
		data.cmd = RD_HC04;
		write(sock, &data, sizeof(data));

		//image ��û ������
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