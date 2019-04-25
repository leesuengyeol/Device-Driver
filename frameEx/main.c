//쓰레드를 두개만들어 거리가 30cm이상 멀어지면 led불 켜지게 만들기

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
#include <time.h>

#define LED 6	//GPIO18
#define trig 4	//
#define echo 5

//led 자원을 관리하기 위해 mutex 변수를 선언
pthread_mutex_t led_lock;//(led Control 함수에서 led 제어를 동시에 하지 못하게 해줌)

// 두 쓰레드 사이의 공유 변수
int flag;
pthread_mutex_t flag_lock; 



void ledControl(int value)
{
	//LED를 위한 MUTEX구현
	//trylock 시도하고 다시돌아가고 시도하고다시돌아옴 락은 계속기다림
	if (pthread_mutex_trylock(&led_lock) != EBUSY) 
	{
		digitalWrite(LED, value);
		pthread_mutex_unlock(&led_lock);
	}
	return;
}


//쓰레드에 의존적이지 않은 함수지만 ledControl 함수의 mutex를 통해서 동시 접근 못하게한다
void * ledFunction(void* arg)  
{
	pinMode(LED, OUTPUT);

	//flag가 변하면 led value도 변하게 만들기
	while (1)
	{
		//flag 값을 읽어와서 led제어 : led제어를 할때는 flag가 갑자기 바뀌어선 안된다
		//따라서 뮤텍스 제어
		if (pthread_mutex_lock(&flag_lock) != EBUSY) 
		{
			ledControl(flag);
			pthread_mutex_unlock(&flag_lock);
		}
	}
	return 0;
}
float HCControl(void)
{
	float distance;
	int startTime, endTime;

	//트리거 상태 초기화
	digitalWrite(trig, LOW);
	delay(500);

	//10microsec 동안 trig 보냄
	digitalWrite(trig, HIGH);
	delayMicroseconds(10);
	digitalWrite(trig, LOW);

	//Echo back이 '0'이면 시작시간 기록
	while (digitalRead(echo) == 0);
	startTime = micros();

	//Echo back 이 '1'이면 대기
	while (digitalRead(echo) == 1);
	endTime = micros();

	//거리 계산 공식
	distance = (endTime - startTime) / 58;

	printf("distance %.2f cm\n", distance);

	return distance;

}

void * HCFunction(void * arg)
{
	float distance;
	pinMode(trig, OUTPUT);
	pinMode(echo, INPUT);

	while (1)
	{
		distance = HCControl();

		if (distance >= 20)
		{
			if (pthread_mutex_trylock(&flag_lock) != EBUSY)
			{
				flag = 1;
				pthread_mutex_unlock(&flag_lock);
			}
		}
		else
		{
			if (pthread_mutex_trylock(&flag_lock) != EBUSY)
			{
				flag = 0;
				pthread_mutex_unlock(&flag_lock);
			}
		}
	}
}



int main(int argc, char **argv)
{
	int err;
	pthread_t thread_LED; //led 를 제어하는 스레드변수
	pthread_t thread_HC;  //HC-SR04 를 제어하는 스레드변수

	//init
	wiringPiSetup();
	
	pthread_create(&thread_LED, NULL, ledFunction, NULL); //led쓰레드
	pthread_create(&thread_HC, NULL, HCFunction, NULL);	//hc 쓰레드

	pthread_join(thread_LED, 0);//led스레드 종료
	pthread_join(thread_HC, 0);//HC스레드 종료
}