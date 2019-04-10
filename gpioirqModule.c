/*

 문자 디바이스 만들기!!
 커널 내부의 디바이스 는 3가지 존재

1. character device (문자 디바이스)
- 파일처럼 바이트 스트림(stream)으로 접근(read/write) 할수 있는 디바이스
-커널안에 있기도하며 모듈로 추가할 수 있다

2. block device
-하드디스크처럼 내부에 파일 시스템을 가질 수 있는 디바이스
-마운트된 특정 파일 시스템의 파일에 접근할 때, 블록 디바이스 드라이버와 상호 동작하게 됨
-즉 디렉토리와 같이 블록단위로  하드디스크에 접근해야하는 경우 블록디바이스 드라이버를 통해서 제어된다(질문할것)

3. network device
-모든 네트워크의 동작은 네트워크 인터페이스를 통해 이루어진다


 */

#include <linux/fs.h> //struct file_operation
#include <linux/cdev.h> //charactor device
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h> //gpio_get_value()
#include <linux/interrupt.h>


#define GPIO_MAJOR 200
#define GPIO_MINOR 0
#define GPIO_DEVICE "gpio_swled"
#define GPIO_LED 18
#define GPIO_SW 17
#define BLK_SIZE 100

/*
//라즈베리파이 GPIO  물리메모리
#define BCM_IO_BASE 0x3F000000

//GPIO_BaseAddr
#define GPIO_BASE (BCM_IO_BASE + 0x200000) 
 
//GPIO Function Select Register 0 ~ 5
#define GPIO_IN(g) (*(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))) 
#define GPIO_OUT(g) {(*(gpio+((g)/10)) &= ~(7<<(((g)%10)*3)));\
						(*(gpio+((g)/10)) |= (1<<(((g)%10)*3))); } 
 
//GPIO Pin Output Set 0 / clr 0
//                  addr(20001C)
#define GPIO_SET(g) (*(gpio+7) = (1<<g))
//                  addr(200028)
#define GPIO_CLR(g) (*(gpio+10) = (1<<g))

#define GPIO_GET(g) (*(gpio+13)&(1<<g))
#define GPIO_SIZE 0xB4
*/

//#define DEBUG

struct cdev gpio_cdev;
//volatile unsigned int *gpio;
static char msg[BLK_SIZE]={0};
static int switch_irq; // irq handler 를 다른 함수에서도 사용가능하게끔

static int gpio_open(struct inode *, struct file *); //file_operation 과 함수이름 맞춰줘야한다!!
static int gpio_close(struct inode *, struct file *);
static ssize_t gpio_read(struct file *, char *buff, size_t , loff_t *); //user 단으로 데이터 전송
static ssize_t gpio_write(struct file *, const char *, size_t , loff_t *);

static struct file_operations gpio_fops={
	.owner   = THIS_MODULE,
	.read    = gpio_read,
	.write   = gpio_write,
	.open    = gpio_open,
	.release = gpio_close,
};

static irqreturn_t isr_func(int irq, void *data) //스위치가 눌리는 순간 isr_func에 들어옴
{
	//IRQ발생 & LED가 OFF일때
	static int count; //이함수안에서만 변경가능한 전역변수
	if((irq == switch_irq) && !gpio_get_value(GPIO_LED)) //irq발생 & led가 of일때
			gpio_set_value(GPIO_LED, 1); //led on 해줌
	else //irq발생 & led가 on 일때
			gpio_set_value(GPIO_LED, 0);

	printk(KERN_INFO " Called isr_func(): %d\n",count);
	
	count++;

	return IRQ_HANDLED;
}



//						inode주소 			file 정보 주소 를 인자로
static int gpio_open(struct inode *inod, struct file *fil) //file_operation 과 함수이름 맞춰줘야한다!!
{
	//모듈의 사용 카운트를 증가시킨다
	try_module_get(THIS_MODULE);//try_module_get은 현재 이장치를 사용하고 있는 프로세서의 갯수를 알려준다 
								//프로세서가 장치를 사용하고 있을때는 rmmod가 불가능하다 따라서 open시 이것을 채크
	printk(KERN_INFO "GPIO Device opened\n");
	return 0;
}



static int gpio_close(struct inode *inod, struct file *fil)
{
	//모듈의 사용 카운트를 감소시킨다.
	module_put(THIS_MODULE); // <-> try_module_get()
	printk(KERN_INFO "GPIO Device closed\n");
	return 0;
}


						//파일의 위치	어떤버퍼를 읽어올지 , 읽어올 사이즈 ,버퍼의 오프셋값
static ssize_t gpio_read(struct file *fil, char *buff, size_t len, loff_t *off) //user 단으로 데이터 전송
{
	int count;
	if(gpio_get_value(GPIO_LED)) //gpio_get_value = 현재 gpio 의 값을 읽어옴
			msg[0]='1';
	else
			msg[1]='0';
	strcat(msg," from kernel"); // print msg + from kernel
	count = copy_to_user(buff,msg,strlen(msg)+1); //유저단에 msg 보내줌 +1 은  널문자
	
	printk(KERN_INFO "GPIO Device read: %s\n",msg);

	return count;
	
}



static ssize_t gpio_write(struct file *fil, char const *buff, size_t len, loff_t *off)
{
	int count;
	memset(msg, 0, BLK_SIZE);

	count = copy_from_user(msg,buff,len); //유저단의 buff가져옴
	gpio_set_value(GPIO_LED, (strcmp(msg, "0"))); //gpio 에 값을 써줌

	printk(KERN_INFO "GPIO Device write: %s\n", msg);

	return count;
}



static int __init initModule(void)
{
	dev_t devno; //dev_t 디바이스 파일의 MAJOR , MINOR 번호를 지정하기 위한 변수타입(32bit)
	int err;
	//static void *map;
	int count;

	printk("called initModule()\n");
	
	// 1. 문자디바이스 드라이버를 등록한다
	devno = MKDEV(GPIO_MAJOR, GPIO_MINOR); //MAJOR , MINOR 값을 이용하여 dev_t값을 얻는다
	register_chrdev_region(devno,1,GPIO_DEVICE); //dev_t 값을 집어 넣어서 디바이스 드라이버 등록



/*

	register_chrdev_region() :  문자디바이스를 등록한다
	
	함수의 동작과정
	1. struct char_device_struct 생성 -> 2. __register_chrdev_region() 문자디바이스 등록 -> 3. cdev_alloc()
	
	struct cdev {
	struct kobject kobj;
	struct module *owner;
	const struct file_operations *ops;
	struct list_head list;
	dev_t dev;
	unsigned int count;
	};
	
	cdev 구조체 내의 내용들을 char_device_struct에서 골라서 복사(alloc)
	=> 커널에서 char_device_struct를 공개하지 않고 cdev만 공개 , 사용하도록하기 위함

*/



	//2. 문자 디바이스를 위한 구조체를 초기화 한다
	cdev_init(&gpio_cdev, &gpio_fops); //디바이스를 초기화 하고 , 사용자가 생성한 fops를 생성한 cdev의 struct file_operation을 등록
	gpio_cdev.owner = THIS_MODULE;
	count=1;

	//3. 문자디바이스를 추가
	err=cdev_add(&gpio_cdev, devno,count); // 커널의 드라이버에 추가
											//내부적으로 kobj_map()를 이용해 실제 디바이스 구조체와 디바이스 번호를 연결
	if(err<0)
	{
		printk(KERN_INFO "Error: cdev_add()\n");
		return -1;
	}
	printk(KERN_INFO "'sudo mknod /dev/%s c %d 0'\n",GPIO_DEVICE, GPIO_MAJOR);
	printk(KERN_INFO "'sudo chmod 666 /dev/%s'\n",GPIO_DEVICE);
	
	
	//현재 GPIO_LED핀이 사용중인지 확인하고 사용권한 획득
	err= gpio_request(GPIO_LED, "LED"); 
	
	if(err==-EBUSY)
	{
		printk(KERN_INFO "Error gpio_request LED\n");
		return -1;
	}
	/* =
	/4. 물리메모리 번지로 인자값을 전달하면 가상메모리 번지를 리턴 (ioremap)
	map = ioremap(GPIO_BASE, GPIO_SIZE);

	if(!map)
	{
		printk(KERN_INFO "Error: mapping GPIO memory\n");
		iounmap(map);
		return -EBUSY;
	}
	
#ifdef DEBUG
	printk(KERN_INFO "devno=%d",devno);
#endif

	gpio = (volatile unsigned int*)map;
	*/


	//현재 GPIO_SW핀이 사용중인지 확인하고 사용권한 획득
	err= gpio_request(GPIO_SW, "SW"); 
	
	if(err==-EBUSY)
	{
		printk(KERN_INFO "Error gpio_request SW\n");
		return -1;
	}


	gpio_direction_output(GPIO_LED,0); //gpio select function= output ,pin =GPIOLED (18), gpio_value=0
										//chipset 회사에서 이러한 함수를 지원해줌으로서 우리가 직접적으로 register를 건들이지 않아도 된다
/* = GPIO_OUT(GPIO_LED);
	 //GPIO_CLR(GPIO_LED); //프로그램이 시작될때 led를 꺼놓고 시작  
	 GPIO_SET(GPIO_LED); //test
*/
	

	switch_irq=gpio_to_irq(GPIO_SW);
	//= gpio_direction_input(GPIO_SW); 인터럽트를 사용하기 위해서 gpio_to_irq를 대신 사용한다
	err= request_irq(switch_irq, isr_func, IRQF_TRIGGER_RISING, "switch", NULL); 
				//								rising edge 일때

	//request_irq가 실행 안될때
	if(err)
	{
		printk(KERN_INFO "Error request_irq\n");
		return -1;
	}

	return 0;
}



static void __exit cleanupModule(void)
{
	dev_t devno = MKDEV(GPIO_MAJOR,GPIO_MINOR);
	
	gpio_direction_output(GPIO_LED,0);
	//=GPIO_CLR(GPIO_LED);

	//gpio_request()에서 받아온 사용권한을 반납한다.
	gpio_free(GPIO_LED);
	gpio_free(GPIO_SW);
	// 1. 문자디바이스 등록 해제
	unregister_chrdev_region(devno,1);

	// 2. 문자디바이스 구조체를 제거
	//할당된 자원을 반납
	
	/*if(gpio) //if(gpio) == 사용한 gpio 번지가 있다면 1이기때문에 반납하게된다
			iounmap(gpio);
	*/
	
	printk("Good-bye!\n");
}

//insmod 함수 정의
module_init(initModule);
//rmmod 함수 정의
module_exit(cleanupModule);
//module 정보
MODULE_LICENSE("GPL"); 
MODULE_DESCRIPTION("GPIO Driver Moudle");
MODULE_AUTHOR("lee");
