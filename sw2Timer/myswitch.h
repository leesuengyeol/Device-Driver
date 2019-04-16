//#include의 중복선언을 방지하기 위해
#ifndef MYSWITCH_H
#define MYSWITCH_H

#define GPIO_MAJOR	200
#define GPIO_MINOR	0
#define GPIO_DEVICE	"gpiosw"
#define GPIO_SW1	17
#define GPIO_SW2	27
#define BUF_SIZE	100

// 7 segment gpio pin number
#define A 16 
#define B 12
#define C 13
#define D 19
#define E 26
#define F 20
#define G 21
#define DP 6

int PIN[9] = { A,B,C,D,E,F,G,DP };

//0~9
int DISP[10][8] = { {0,0,0,0,0,0,1,1},
				{1,0,0,1,1,1,1,1},
				{0,0,1,0,0,1,0,1},
				{0,0,0,0,1,1,0,1},
				{1,0,0,1,1,0,0,1},
				{0,1,0,0,1,0,0,1},
				{0,1,0,0,0,0,0,1},
				{0,0,0,1,1,1,1,1},
				{0,0,0,0,0,0,0,1},
				{0,0,0,0,1,0,0,1}
};

int	TEST[2][8] = {
	{0,0,0,0,0,0,0,0},  //segment all on
	{1,1,1,1,1,1,1,1}	//segment all off
};


#endif
