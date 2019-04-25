//��Ƽ������ led , HC-SR04, image ������ server	, client �����̸� : ledclnt.c
//server object
//1. led�� on/off�� clinet�� ���� �޾Ƽ� multithread�� ó���ϱ�
//2. HC-SR04 �� distance���� client�� ��û�ؿö� ���� multithread�� �̿��Ͽ� write�ϱ�
//3. ���� �̹����� ������ buf�� �Ἥ client �� write�ϱ�

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <wiringPi.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "raspi.h"

#define	LED		1	// GPIO18
#define trig	4	// GPIO23
#define echo	5	// GPIO24
#define DEBUG
#define MAX_CLNT 256


//LED�ڿ��� �����ϱ� ���� mutex������ �����Ѵ�.
pthread_mutex_t led_lock;
//������ ������ ���� mutex �ڿ� ����
pthread_mutex_t hc04_lock;

struct Data buf;//raspi.h
struct Data data;//raspi.h

int clnt_cnt = 0;
int clnt_socks[MAX_CLNT];
pthread_mutex_t mutx;

void error_handling(char *message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}

//=====================================================
// LED Function
//=====================================================
/*
	###### ȿ������ �ڿ� ���� � ######

	�ܼ��� led�� on/off���� ����� 3���� �������� �и��Ͽ� �����Ѵ�
	multithread�� �̿��Կ� �־� �ſ� �����ϴ�.

	1. ledFunction:	led�� ������ �ʿ䰡 ������ ������� ȣ��Ǵ� �Լ�
	2. ledControl :	led�� on/off�� ���������� �����ϴ� �Լ�, ledFunction ���� ledControl�Լ��� ȣ���ϴµ� �ܼ��� value���� ������ led�� on/off �� �����Ѵ�
	3. ledWrite	  : ledControl���� �����ϴ� value���� �������ش�. �� led�� on�ؾ����� off�ؾ����� �˷��ִ� ���� 
					1,2�� main���� �������� thread �ȿ��� ������ �ϰ� �ڵ峻 � �������� ledWrite�� ���� led�� on/off value�� �������� �� �ְ� ���ش�
					�� �̶� ���� thread���� value���� �����ϸ� ���α׷��� ���� �� �ֱ� ������ �� ledWrite�Լ��� ���� value������ ���ɽ����� �̷������ �ϹǷ�
					mutex�� ����Ͽ� critical section �� ���� ����������Ѵ�!
*/

void ledWrite(struct Data *data, int value)
{
	data->led_Value = value;
}

void ledControl(int value)
{
	// LED�� ���� MUTEX����
	if (pthread_mutex_trylock(&led_lock) != EBUSY) //critical section�� ������ thread�� ���ٸ�
	{
		digitalWrite(LED, value);
		pthread_mutex_unlock(&led_lock);
	}
	return;
}

void* ledFunction(void *arg)
{

	pinMode(LED, OUTPUT);

	while (!data.endFlag)
	{
		ledControl(data.led_Value);
		//printf("led=%d\n", data.led_Value);
		usleep(200000);
	}
	
}


//=====================================================
// HC_SR04 Function
//=====================================================

// led�� ���� �и��Ͽ� ȿ�������� ����
//thread�� �̿��Ͽ� ���������� �����

//HC-SR04�� �̿��Ͽ� distance ���� ����
float hc04Control(void)
{
	int startTime, endTime;
	float distance;

	// LED�� ���� MUTEX����
	if (pthread_mutex_trylock(&hc04_lock) != EBUSY)	//critical section�� ������ thread�� ���ٸ�
	{
		// Trig��ȣ ���� (10us)
		digitalWrite(trig, HIGH);
		usleep(10);					//wiringPi : delayMicroseconds(10); 
		digitalWrite(trig, LOW);

		// Echo back�� '1'�̸� ���۽ð� ��� 
		while (digitalRead(echo) == 0);
		//wiringPi : wiringPiSetup������ �ð��� ����ũ���ʷ� �����ϴ� �Լ�
		startTime = micros();

		// Echo back�� '1'�̸� ��� 
		while (digitalRead(echo) == 1);
		// Echo back�� '0'�̸� ����ð� ��� 
		endTime = micros();

		distance = (endTime - startTime) / 58;

		pthread_mutex_unlock(&hc04_lock);
		usleep(10000);
	}
	return distance;
}

//hc04Control �Լ��κ��� distance ���� ���Ϲ޾� Data����ü �ȿ� �Է�
void* hc04Function(void *arg)
{
	float ret;
	pinMode(trig, OUTPUT);
	pinMode(echo, INPUT);
	digitalWrite(trig, LOW);
	while (!data.endFlag)
	{
		ret = hc04Control();
		data.hc04_dist = ret; //����ü�� distance�� ����
		//printf("distance:%f\n", ret);
	}
	return NULL;
}

/*	userthread : 1. thread�� �ΰ� �� ���� led����� HC-SR04��� �и�
				 2. client�κ��� ���� �޾ƿ� client�� ���ϴ� data����
*/

void* userThread(void *arg)
{
	int clnt_sock = *((int*)arg);
	int str_len = 0, i;
	int read_cnt;
	pthread_t t_id[2];
	pinMode(LED, OUTPUT);
	char filebuf[BUF_SIZE];

	//file �ű�� ���� ���� ����ü ������ (fopen ,fwrite , fread���� ���� ���)
	FILE* fp;

	//1. thread�� �ΰ� �� ���� led����� HC-SR04��� �и�
	
	//led thread
	pthread_create(&t_id[0], NULL, ledFunction, 0);
	//hc04 thread
	pthread_create(&t_id[1], NULL, hc04Function,0);
	
	//client�� ���� ���� �޾ƿ� ���ϴ� data�� ������ �� �ֵ��� ���� ����
	while ((str_len = read(clnt_sock, &buf, sizeof(buf))) != 0)	//client�� ���� ���� �޾ƿ� ,read�Լ��� block�Լ��� ������ ������ ������ �������� ����
	{
		/*	���������� ���� client�� ����ü�� server�� ����ü�� ���� ������ �ʰ� switch ���� �̿��Ͽ� buf.cmd�� Ȯ���ϴ� ����!!!
			
			����ü�ȿ� �����͸� �Ѳ����� ���� �Ѱ��ִ� ����϶� ,  ��Ƽ thread�� �̿��Ͽ� �����ϴ� ���
			client���� ������ data����ü�� 
			server���� ������ data����ü�� ���� 
			����ȭ�� ���� �ʾ� data�� ���̴� ��찡 �߻��Ѵ�
			�̸� �������� data ����ü�� cmd������ �߰��Ͽ� client�� ����� ��û�ϴ��� Ȯ����
			�� data�� ����ȭ �ϴ� ������� ���� ������ �ʴ´�


			ex) ���� ������ ���� ����ü a,b �� ������,  server�� ����ü�� a  client�� ����ü�� b ��� ����
			
			struct a,b {
				int cmd;
				int endFlag;
				int led_Value;
				float hc04_dist;
			}

			�̶� b�� ���� a�� ���� ����� ���
			���� ���������� �������� HC-SR04 thread�� distance ���� client�� �ִ� b�� ���� ���� �������鼭 ����ε� ������ �Ұ����ϴ�
			�̷��� ������ client�� ��û(buf.cmd)�� Ȯ���ϰ� ����ü ��ü�� ���� ����� ���� �ƴ� ���ΰ� �� �ϳ����� �� ��� �����ؾ� ���̴� ���̾���!!
		*/
		switch (buf.cmd)// buf.cmd�� Ȯ���Ͽ� client�� � data�� ���ϴ��� Ȯ��
		{
		case WR_LED: //client���� ������ ����ü�� cmd�� WR_LED(led�� on/off����) �ϰ��
			data.led_Value = buf.led_Value;	//server ����ü�� led_Value�� client�� ������ led_Value�� ����ȭ
			printf("data.led_Value=%d\n", data.led_Value);
			break;
		case RD_HC04:			//HC-SR04 �� distance���� ���� ���
			data.cmd = WR_DIST;	//cmd�� WR_DIST �ٲ㼭 client���� ���� �о�� ����ü���� ����� ����ȭ ����� �˷��ش�
			write(clnt_sock, &data, sizeof(data));	
			printf("data.dist:%f\n", data.hc04_dist);
			break;
		case RD_IMG:	//IMG�� ���Ұ��
			data.cmd = WR_IMG;
			write(clnt_sock, &data, sizeof(data));

			fp = fopen(FILENAME, "rb"); //read �Ҷ� binary�� �о�ߵ� ascii�� �ƴҼ��� �ֱ⶧����
			while (1)
			{
				read_cnt = fread((void*)filebuf, 1, BUF_SIZE, fp); //image ���� �о�� BUFSIZE��ŭ

				//�����ִ� ������ ����� BUF_SIZE���� ���� ��� (�о�� �����Ͱ� �ѹ��� �о���� �����ͻ������� 1024���� ������� �����ִ� �����ŭ�� �б�) 
				if (read_cnt < BUF_SIZE)
				{
					write(clnt_sock, filebuf, read_cnt);
					break; //img data �� ��� �о���� while�� ����
				}

				//�����ִ� ������ ����� BUF_SIZE���� ū���
				write(clnt_sock, filebuf, BUF_SIZE);
			}
			printf("File Write is done\n");
			fclose(fp);
			break;
			
		default:
			
			break;
		}
	}

	data.endFlag = 1;	//�� �������� ������ �����
	
	/*	pthread_join  vs pthread_detach
		pthread_join�� block �Լ��μ� �����尡 ������ ���� ��ٸ��� �ڿ��� ȸ��������
		pthread_detach�� non_block �Լ��μ� thread�� ����Ǹ� �˾Ƽ� �ڿ��� ȸ���ϵ��� �ϰ� �ڵ�� �ڵ��� ����ȴ�
	*/
	pthread_detach(t_id[0]); //�ڿ��� �޳��޴´�
	pthread_detach(t_id[1]); //�ڿ��� �޳��޴´�

	printf("userThread is end\n");

	/*
		remove �� client�� ���� ��ũ���͸� �����
		�׿� �°� file descriptor table �� �ֽ�ȭ ���ش� (�߰��� ����� ���� file descriptor table�� �ֽ�ȭ)
	*/

	pthread_mutex_lock(&mutx);
	for (i = 0; i < clnt_cnt; i++)   // remove disconnected client
	{
		if (clnt_sock == clnt_socks[i]) //������ clnt_sock�� ���ϵ�ũ������ table �迭 ��ġ�� i �� ã�´�
		{
			while (i++ < clnt_cnt - 1)				 //i= ������ clnt_sock�� ���� ��ũ���� table�迭��ġ  , clnt_cnt = ���� ��ũ���� ���̺� ���� ������ +1
				clnt_socks[i] = clnt_socks[i + 1];	 //clnt_socks[i] �� ������ sock�� ��ġ�� �ٷ� ���ڸ��� �ִ� ������ ����־ ���ڸ��� �Ų۴�
													 //clnt_cnt -1 , ���峡�� �ִ� sock ���� ������ ��ĭ�� �����ش�
			break;
		}
	}
	
	clnt_cnt--; //�ֽ�ȭ �Ϸ��� ���ϵ�ũ���� ���̺� ��������� --
	pthread_mutex_unlock(&mutx);
	close(clnt_sock);
}

static void sigHandler(int signum)
{
	printf("sigHanlder\n");
	exit(0);
}


int main(int argc, char **argv)
{
	int err;
	int serv_sock, clnt_sock;
	pthread_t thread_LED;
	int str_len;
	struct sockaddr_in serv_adr, clnt_adr;
	int clnt_adr_sz;
	struct Data data, buf;	//server���� �������� ����ü, client���� �޾ƿ� �ӽñ���ü 
	pthread_t t_id;		//userthread id
	data.endFlag = 0;	//endFlag �ʱ�ȭ(����ü ������ �ʱ�ȭ �ȵ�), endFlag ==1 => led , HC-SR04 thread���� ����

	//Init
	wiringPiSetup();

	signal(SIGINT, sigHandler);


	// STEP 1.
	serv_sock = socket(PF_INET, SOCK_STREAM, 0);
	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_adr.sin_port = htons(atoi(argv[1]));

	// bind() error�� �߻����� �ʴ´�.
	//setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &option, optlen);

	//STEP 2.
	if (bind(serv_sock, (struct sockaddr*) &serv_adr, sizeof(serv_adr)) == -1)
		error_handling("bind() error");

	//STEP 3.
	if (listen(serv_sock, 5) == -1)
		error_handling("listen() error");

	while (1)
	{
		clnt_adr_sz = sizeof(clnt_adr);
		clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_adr, &clnt_adr_sz);

		//critical section  -> clnt_sock�� file descriptor ����
		pthread_mutex_lock(&mutx);
		clnt_socks[clnt_cnt++] = clnt_sock; //clnt_socks�� 012 �� ������ ���ϵ�ũ�������̺� �迭�� ����ȴ�
											//clnt_cnt �ʱ�ȭ �� 0 �̹Ƿ� clnt�� �迭 0���� ���ʷ� ������
		pthread_mutex_unlock(&mutx);

		pthread_create(&t_id, NULL, userThread, (void*)&clnt_sock);
		pthread_detach(t_id); //�˾Ƽ� �ڿ��ݳ�
		printf("Connected client IP: %s \n", inet_ntoa(clnt_adr.sin_addr));
	}



#ifdef DEBUG
	//TODO
	while (1)
	{
		if (data.led_Value == 1)
			ledWrite(&data, LOW);
		else
			ledWrite(&data, HIGH);
		sleep(1);
	}
#endif



	close(serv_sock);
	return 0;

}