#include <linux/fs.h> //struct file_operaton 파일 시스템
#include <linux/cdev.h> // 문자 디바이스 : register_chrdev_region(),cdev_init()
						//cdev_add() , cdev_del()
#include <linux/io.h>  //ioremap  ,  iounmap()
#include <linux/uaccess.h> //copy to user , copy from user
#include <linux/init.h> //init
#include <linux/module.h>
#include <linux/kernel.h> //pr_info
#include <linux/gpio.h>  //gpio_get_value , gpio_set_value, request_gpio , gpiofree

//hrtimer timer 정의 함수
#include <linux/hrtimer.h>
#include <linux/ktime.h>





#define GPIO_MAJOR 200
#define GPIO_MINOR 0
#define GPIO_DEVICE "stopwatch"

// 7 segment gpio pin number
#define A 16 
#define B 12
#define C 13
#define D 19
#define E 26
#define F 20
#define G 21
#define DP 6
#define BLK_SIZE 100

#define MS_TO_NS(x) (x * 1E6L) // ms -> ns 변환

struct cdev gpio_cdev;  //cdev 구조체선언
static struct hrtimer hr_timer;  //hrtimer 구조체 선언


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

//참조: http://jake.dothome.co.kr/hrtimer/ 
//시간함수

enum hrtimer_restart my_hrtimer_callback(struct hrtimer *timer)
{
	int i = 0;
	static int num=0;
	ktime_t currtime, interval;
	unsigned long delay_in_ms = 1000L;
	pr_info("my_hrtimer_callback called (%ld).\n", jiffies);

	if (num == 10)
	{
		for (; i < 8; i++)
		{
			gpio_set_value(PIN[i], 1);
		}
		return HRTIMER_NORESTART;
	}

	else
	{
		for (; i < 8; i++)
		{
			gpio_set_value(PIN[i], DISP[num % 10][i]);
		}
		currtime = ktime_get(); //현재시간을 받아옴
		interval = ktime_set(0, MS_TO_NS(delay_in_ms)); //원하는 지연시간 설정
		hrtimer_forward(timer, currtime, interval); 
		/* hrtimer_forward  - 만료시간 재설정
			만료된 타이머에 한해 만료시각으로 부터 interval 간격의 currtime를 지난 시각으로 
			만료시간을 설정
		*/
		num++;
		return HRTIMER_RESTART;
	}	
}



//mapping 되는 함수 구조체 file_operation 
static struct file_operations gpio_fops = {
	.owner = THIS_MODULE,
	/*.read = gpio_read,
	.write = gpio_write,
	.open = gpio_open,
	.release = gpio_close,*/
};

/*
//장치 open				inode주소		file 정보 주소 
static int gpio_open(struct inode *inod, struct file *fil)
{
	//모듈의 사용 카운트를 증가시킨다
	try_module_get(THIS_MODULE);
	try_module_get
	현재 이장치를 사용하고 있는 프로세서의 갯수를 알려준다
	프로세서가 장치를 사용하고 있을때는 rmmod가 불가능하다 따라서 open시 이것을 체크
	

	
}
*/



static int __init initModule(void)
{
	dev_t devno; //dev_t 디바이스 파일의 MAJOR, MINOR번호를 저장 가능한 변수
	int err;
	int count;


	//시간 내용 변수
	ktime_t ktime; //ns 단위
	unsigned long delay_in_ms = 1000L;


	pr_info("called initModule()\n");

	//init : register_chrdev  -> cdev_init ->  cdev_add -> gpio_request

	//kernel의 char디바이스 테이블에 저장해줌

	//1. GPIO_MAJOR ,GPIO_MINOR ,  DEVICE 이름 등록
	devno = MKDEV(GPIO_MAJOR, GPIO_MINOR); //devno형태로 MARJOR MINOR값 변경
	register_chrdev_region(devno, 1, GPIO_DEVICE);
	/*
		register_chrdev_region(dev_t , 갯수 , 디바이스 이름) :  문자디바이스를 등록
	*/




	//2. 문자 디바이스를 위한 구조체 초기화
	cdev_init(&gpio_cdev, &gpio_fops); // cdev 구조체와 fops 구조체 주소 넘겨서 
										//구조체 초기화
	gpio_cdev.owner = THIS_MODULE; // 구조체 .owner 에서 THIS_MODULE 넣기
	count = 1; //디바이스 갯수



	//3. 문자 디바이스를 추가
	err = cdev_add(&gpio_cdev, devno, count); //커널의 드라이버에 추가
	/*
	내부적으로 kobj_map()를 이용해 실제 디바이스 구조체와 디바이스 번호를 연결
	cdev_add( the cdev structure for the device,
			  첫번째 디바이스 장치의 number (= devno),
			  마이너넘버 갯수 (장치 갯수)

	*/
	if (err < 0)
	{
		pr_info("Error: cdev_add()\n");
		return -1;
	}
	
	pr_info("'sudo mknod /dev/%s c %d 0'\n", GPIO_DEVICE, GPIO_MAJOR);
	pr_info("'sudo chmod 666 /dev/%s'\n", GPIO_DEVICE);
	   
	//4. gpio 핀 사용권한 획득
	err = gpio_request(A, "A"); //pin, pin name
	gpio_request(B, "B");
	gpio_request(C, "C");
	gpio_request(D, "D");
	gpio_request(E, "E");
	gpio_request(F, "F");
	gpio_request(G, "G");
	gpio_request(DP, "DP");

	if (err == -EBUSY)
	{
		pr_info("Error gpio_request\n");
		return -1;
	}
	
	//Pin 초기화
	gpio_direction_output(A, 1);
	gpio_direction_output(B, 1);
	gpio_direction_output(C, 1);
	gpio_direction_output(D, 1);
	gpio_direction_output(E, 1);
	gpio_direction_output(F, 1);
	gpio_direction_output(G, 1);
	gpio_direction_output(DP, 1);



	

	pr_info("HR Timer module installing\n");

	ktime = ktime_set(0, MS_TO_NS(delay_in_ms)); //ktime 정의 ns 단위

	hrtimer_init(&hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL); //시간 초기화

	hr_timer.function = &my_hrtimer_callback; // 시간이되면 실행되는 callback함수
	hrtimer_start(&hr_timer, ktime, HRTIMER_MODE_REL);
	//			카운팅 시간  까지	상대 시간으로 현재 시각으로 부터 요청한 ktime(ns)이 지난 후에 동작




	return 0;
}

static void __exit cleanupModule(void)
{
	int ret;
	dev_t devno;
	devno= MKDEV(GPIO_MAJOR, GPIO_MINOR); //MAJOR MINOR dev_t 형식으로 변환

	//pin 초기화
	gpio_direction_output(A, 1);
	gpio_direction_output(B, 1);
	gpio_direction_output(C, 1);
	gpio_direction_output(D, 1);
	gpio_direction_output(E, 1);
	gpio_direction_output(F, 1);
	gpio_direction_output(G, 1);
	gpio_direction_output(DP, 1);
	
	//gpio_request()에서 받아온 사용권한을 반납한다
	gpio_free(A);
	gpio_free(B);
	gpio_free(C);
	gpio_free(D);
	gpio_free(E);
	gpio_free(F);
	gpio_free(G);
	gpio_free(DP);
	//unregister_chrdev_region -> cdev_del

	//1. 문자 디바이스 등록해제
	unregister_chrdev_region(devno, 1);

	//2. 문자디바이스 구조체를 제거
	cdev_del(&gpio_cdev); //할당된 자원을 반납

	ret = hrtimer_cancel(&hr_timer); //0이어야 정상삭제

	if (ret)
		pr_info("The timer was still in use ...\n");

	pr_info("HR Timer module uninstalling\n");

	pr_info("Good-bye!\n");
	
}

//insmod 함수 정의
module_init(initModule);
//rmmod 함수 정의
module_exit(cleanupModule);
//module 정보
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GPIO Driver Moudle");
MODULE_AUTHOR("lee");
