//멀티쓰레드 led , HC-SR04, image 보내기 server	, client 파일이름 : ledclnt.c
//server object
//1. led의 on/off를 clinet로 부터 받아서 multithread로 처리하기
//2. HC-SR04 의 distance값을 client가 요청해올때 마다 multithread를 이용하여 write하기
//3. 사진 이미지를 읽은후 buf에 써서 client 에 write하기

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


//LED자원을 관리하기 위해 mutex변수를 선언한다.
pthread_mutex_t led_lock;
//초음파 센서에 대한 mutex 자원 관리
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
	###### 효율적인 자원 관리 운영 ######

	단순한 led의 on/off제어 기능을 3가지 계층으로 분리하여 제어한다
	multithread를 이용함에 있어 매우 유용하다.

	1. ledFunction:	led를 조작할 필요가 있을때 가장먼저 호출되는 함수
	2. ledControl :	led의 on/off를 직접적으로 제어하는 함수, ledFunction 에서 ledControl함수를 호출하는데 단순히 value값을 참조해 led의 on/off 를 제어한다
	3. ledWrite	  : ledControl에서 참조하는 value값을 지정해준다. 즉 led를 on해야할지 off해야할지 알려주는 역할 
					1,2는 main문과 독립적인 thread 안에서 동작을 하고 코드내 어떤 곳에서든 ledWrite에 의해 led의 on/off value를 변경해줄 수 있게 해준다
					단 이때 여러 thread에서 value값에 접근하면 프로그램이 꼬일 수 있기 때문에 이 ledWrite함수에 의한 value변경은 조심스럽게 이루어져야 하므로
					mutex를 사용하여 critical section 을 만들어서 제어해줘야한다!
*/

void ledWrite(struct Data *data, int value)
{
	data->led_Value = value;
}

void ledControl(int value)
{
	// LED를 위한 MUTEX구현
	if (pthread_mutex_trylock(&led_lock) != EBUSY) //critical section에 접근한 thread가 없다면
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

// led와 같이 분리하여 효율적으로 구현
//thread를 이용하여 독립적으로 사용중

//HC-SR04를 이용하여 distance 값을 구함
float hc04Control(void)
{
	int startTime, endTime;
	float distance;

	// LED를 위한 MUTEX구현
	if (pthread_mutex_trylock(&hc04_lock) != EBUSY)	//critical section에 접근한 thread가 없다면
	{
		// Trig신호 생성 (10us)
		digitalWrite(trig, HIGH);
		usleep(10);					//wiringPi : delayMicroseconds(10); 
		digitalWrite(trig, LOW);

		// Echo back이 '1'이면 시작시간 기록 
		while (digitalRead(echo) == 0);
		//wiringPi : wiringPiSetup이후의 시간을 마이크로초로 측정하는 함수
		startTime = micros();

		// Echo back이 '1'이면 대기 
		while (digitalRead(echo) == 1);
		// Echo back이 '0'이면 종료시간 기록 
		endTime = micros();

		distance = (endTime - startTime) / 58;

		pthread_mutex_unlock(&hc04_lock);
		usleep(10000);
	}
	return distance;
}

//hc04Control 함수로부터 distance 값을 리턴받아 Data구조체 안에 입력
void* hc04Function(void *arg)
{
	float ret;
	pinMode(trig, OUTPUT);
	pinMode(echo, INPUT);
	digitalWrite(trig, LOW);
	while (!data.endFlag)
	{
		ret = hc04Control();
		data.hc04_dist = ret; //구조체의 distance값 변경
		//printf("distance:%f\n", ret);
	}
	return NULL;
}

/*	userthread : 1. thread를 두개 더 만들어서 led제어와 HC-SR04제어를 분리
				 2. client로부터 값을 받아와 client가 원하는 data제공
*/

void* userThread(void *arg)
{
	int clnt_sock = *((int*)arg);
	int str_len = 0, i;
	int read_cnt;
	pthread_t t_id[2];
	pinMode(LED, OUTPUT);
	char filebuf[BUF_SIZE];

	//file 옮기기 위한 파일 구조체 포인터 (fopen ,fwrite , fread등을 위해 사용)
	FILE* fp;

	//1. thread를 두개 더 만들어서 led제어와 HC-SR04제어를 분리
	
	//led thread
	pthread_create(&t_id[0], NULL, ledFunction, 0);
	//hc04 thread
	pthread_create(&t_id[1], NULL, hc04Function,0);
	
	//client로 부터 값을 받아와 원하는 data를 제공할 수 있도록 동작 수행
	while ((str_len = read(clnt_sock, &buf, sizeof(buf))) != 0)	//client가 보낸 값을 받아옴 ,read함수는 block함수기 때문에 보내기 전까지 내려가지 않음
	{
		/*	내부적으로 같은 client의 구조체를 server에 구조체에 덮어 씌우지 않고 switch 문을 이용하여 buf.cmd를 확인하는 이유!!!
			
			구조체안에 데이터를 한꺼번에 적고 넘겨주는 방법일때 ,  멀티 thread를 이용하여 제공하는 경우
			client에서 적용한 data구조체와 
			server에서 적용한 data구조체의 값이 
			동기화가 되지 않아 data가 꼬이는 경우가 발생한다
			이를 막기위해 data 구조체의 cmd변수를 추가하여 client가 어떤값을 요청하는지 확인후
			이 data만 동기화 하는 방식으로 가야 꼬이지 않는다


			ex) 내부 구조가 같은 구조체 a,b 가 있을때,  server의 구조체는 a  client의 구조체는 b 라고 하자
			
			struct a,b {
				int cmd;
				int endFlag;
				int led_Value;
				float hc04_dist;
			}

			이때 b의 값을 a에 덮어 씌우는 경우
			현재 독립적으로 측정중인 HC-SR04 thread의 distance 값이 client에 있던 b의 값이 덮어 씌워지면서 제대로된 측정이 불가능하다
			이러한 이유로 client의 요청(buf.cmd)를 확인하고 구조체 전체를 덮어 씌우는 것이 아닌 내부값 중 하나만을 콕 찝어 변경해야 꼬이는 일이없다!!
		*/
		switch (buf.cmd)// buf.cmd를 확인하여 client가 어떤 data를 원하는지 확인
		{
		case WR_LED: //client에서 보내온 구조체의 cmd가 WR_LED(led를 on/off제어) 일경우
			data.led_Value = buf.led_Value;	//server 구조체의 led_Value를 client가 보내온 led_Value로 동기화
			printf("data.led_Value=%d\n", data.led_Value);
			break;
		case RD_HC04:			//HC-SR04 의 distance값을 원할 경우
			data.cmd = WR_DIST;	//cmd를 WR_DIST 바꿔서 client에서 현재 읽어온 구조체에서 어떤값이 동기화 됬는지 알려준다
			write(clnt_sock, &data, sizeof(data));	
			printf("data.dist:%f\n", data.hc04_dist);
			break;
		case RD_IMG:	//IMG를 원할경우
			data.cmd = WR_IMG;
			write(clnt_sock, &data, sizeof(data));

			fp = fopen(FILENAME, "rb"); //read 할때 binary로 읽어야됨 ascii가 아닐수도 있기때문에
			while (1)
			{
				read_cnt = fread((void*)filebuf, 1, BUF_SIZE, fp); //image 파일 읽어옴 BUFSIZE만큼

				//남아있는 파일의 사이즈가 BUF_SIZE보다 작을 경우 (읽어올 데이터가 한번에 읽어오는 데이터사이즈인 1024보다 작을경우 남아있는 사이즈만큼만 읽기) 
				if (read_cnt < BUF_SIZE)
				{
					write(clnt_sock, filebuf, read_cnt);
					break; //img data 를 모두 읽어온후 while문 나감
				}

				//남아있는 파일의 사이즈가 BUF_SIZE보다 큰경우
				write(clnt_sock, filebuf, BUF_SIZE);
			}
			printf("File Write is done\n");
			fclose(fp);
			break;
			
		default:
			
			break;
		}
	}

	data.endFlag = 1;	//각 쓰레드의 동작을 멈춘다
	
	/*	pthread_join  vs pthread_detach
		pthread_join은 block 함수로서 쓰레드가 끝날때 까지 기다리고 자원을 회수하지만
		pthread_detach는 non_block 함수로서 thread가 종료되면 알아서 자원을 회수하도록 하고 코드는 코드대로 진행된다
	*/
	pthread_detach(t_id[0]); //자원을 받납받는다
	pthread_detach(t_id[1]); //자원을 받납받는다

	printf("userThread is end\n");

	/*
		remove 된 client의 파일 디스크립터를 지우고
		그에 맞게 file descriptor table 을 최신화 해준다 (중간에 빈곳이 없게 file descriptor table을 최신화)
	*/

	pthread_mutex_lock(&mutx);
	for (i = 0; i < clnt_cnt; i++)   // remove disconnected client
	{
		if (clnt_sock == clnt_socks[i]) //지워진 clnt_sock의 파일디스크립터의 table 배열 위치인 i 를 찾는다
		{
			while (i++ < clnt_cnt - 1)				 //i= 지워진 clnt_sock의 파일 디스크립터 table배열위치  , clnt_cnt = 파일 디스크립터 테이블 가장 맨위값 +1
				clnt_socks[i] = clnt_socks[i + 1];	 //clnt_socks[i] 는 지워진 sock의 위치에 바로 뒷자리에 있는 소켓을 집어넣어서 빈자리르 매꾼다
													 //clnt_cnt -1 , 가장끝에 있는 sock 까지 앞으로 한칸씩 땡겨준다
			break;
		}
	}
	
	clnt_cnt--; //최신화 완료후 파일디스크립터 테이블 가장맨위값 --
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
	struct Data data, buf;	//server에서 관리중인 구조체, client에서 받아온 임시구조체 
	pthread_t t_id;		//userthread id
	data.endFlag = 0;	//endFlag 초기화(구조체 내에서 초기화 안됨), endFlag ==1 => led , HC-SR04 thread들을 종료

	//Init
	wiringPiSetup();

	signal(SIGINT, sigHandler);


	// STEP 1.
	serv_sock = socket(PF_INET, SOCK_STREAM, 0);
	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_adr.sin_port = htons(atoi(argv[1]));

	// bind() error는 발생하지 않는다.
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

		//critical section  -> clnt_sock의 file descriptor 관리
		pthread_mutex_lock(&mutx);
		clnt_socks[clnt_cnt++] = clnt_sock; //clnt_socks는 012 를 제외한 파일디스크립터테이블 배열로 보면된다
											//clnt_cnt 초기화 값 0 이므로 clnt는 배열 0부터 차례로 들어가진다
		pthread_mutex_unlock(&mutx);

		pthread_create(&t_id, NULL, userThread, (void*)&clnt_sock);
		pthread_detach(t_id); //알아서 자원반납
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