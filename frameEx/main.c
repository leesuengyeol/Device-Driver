//�����带 �ΰ������ �Ÿ��� 30cm�̻� �־����� led�� ������ �����

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

//led �ڿ��� �����ϱ� ���� mutex ������ ����
pthread_mutex_t led_lock;//(led Control �Լ����� led ��� ���ÿ� ���� ���ϰ� ����)

// �� ������ ������ ���� ����
int flag;
pthread_mutex_t flag_lock; 



void ledControl(int value)
{
	//LED�� ���� MUTEX����
	//trylock �õ��ϰ� �ٽõ��ư��� �õ��ϰ�ٽõ��ƿ� ���� ��ӱ�ٸ�
	if (pthread_mutex_trylock(&led_lock) != EBUSY) 
	{
		digitalWrite(LED, value);
		pthread_mutex_unlock(&led_lock);
	}
	return;
}


//�����忡 ���������� ���� �Լ����� ledControl �Լ��� mutex�� ���ؼ� ���� ���� ���ϰ��Ѵ�
void * ledFunction(void* arg)  
{
	pinMode(LED, OUTPUT);

	//flag�� ���ϸ� led value�� ���ϰ� �����
	while (1)
	{
		//flag ���� �о�ͼ� led���� : led��� �Ҷ��� flag�� ���ڱ� �ٲ� �ȵȴ�
		//���� ���ؽ� ����
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

	//Ʈ���� ���� �ʱ�ȭ
	digitalWrite(trig, LOW);
	delay(500);

	//10microsec ���� trig ����
	digitalWrite(trig, HIGH);
	delayMicroseconds(10);
	digitalWrite(trig, LOW);

	//Echo back�� '0'�̸� ���۽ð� ���
	while (digitalRead(echo) == 0);
	startTime = micros();

	//Echo back �� '1'�̸� ���
	while (digitalRead(echo) == 1);
	endTime = micros();

	//�Ÿ� ��� ����
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
	pthread_t thread_LED; //led �� �����ϴ� �����庯��
	pthread_t thread_HC;  //HC-SR04 �� �����ϴ� �����庯��

	//init
	wiringPiSetup();
	
	pthread_create(&thread_LED, NULL, ledFunction, NULL); //led������
	pthread_create(&thread_HC, NULL, HCFunction, NULL);	//hc ������

	pthread_join(thread_LED, 0);//led������ ����
	pthread_join(thread_HC, 0);//HC������ ����
}