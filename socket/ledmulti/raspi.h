#define WR_LED		1
#define RD_HC04		2
#define WR_DIST		3
//이 구조체의 값을 보고 thread에서 led를 제어한다
struct Data
{
	int cmd;
	int endFlag;
	int led_Value;
	float hc04_dist;
};