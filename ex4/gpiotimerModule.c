//timer 를 이용한 led on/off

//================================
// Character device driver example 
// GPIO driver
//================================
#include <linux/fs.h>		//open(), read() ,write() close() 커널 함수

#include <linux/cdev.h>		//register_chrdev_region(), cdev_init()
							//cdev_add() , cdev_del()
#include <linux/module.h>

//#include <linux/io.h>		//ioremap(), iounmap()

#include <linux/uaccess.h>	//copy_from_user(), copy_to_user()
//#include <linux/init.h>
//#include <linux/kernel.h>


#include <linux/gpio.h> 	//request_gpio() , gpio_setvalue()
							//gpio_get_value(), gpio_free()

#include <linux/interrupt.h>//gpio_to_irq(), request_irq(), free_irq()
#include <linux/timer.h>	//init_timer(), add_timer(), del_timer()


#define GPIO_MAJOR	200
#define GPIO_MINOR	0
#define GPIO_DEVICE	"gpio_led"
#define GPIO_LED	18
#define GPIO_SW		17
#define BLK_SIZE	100
//#define DEBUG


struct cdev gpio_cdev;
volatile unsigned int *gpio;
static char msg[BLK_SIZE]={0};
static int switch_irq;
static struct timer_list timer;	//타이머처리를 위한 구조체

static int gpio_open(struct inode *, struct file *);
static int gpio_close(struct inode *, struct file *);
static ssize_t gpio_read(struct file *, char *buff, size_t, loff_t *);
static ssize_t gpio_write(struct file *, const char *, size_t, loff_t *);

static struct file_operations gpio_fops={
	.owner   = THIS_MODULE,
	.read    = gpio_read,
	.write   = gpio_write,
	.open    = gpio_open,
	.release = gpio_close,
};

static void timer_func(unsigned long data)
{
	gpio_set_value(GPIO_LED, data);
	if(data)
			timer.data=0L;
	else
	
			timer.data=1L;

	timer.expires=jiffies + (1*HZ);
	add_timer(&timer);
}

static irqreturn_t isr_func(int irq, void *data)
{
	//IRQ발생 & LED가 OFF일때 
	static int count;
	static int flag=0;
	if(!flag)
	{
		flag=1;
		if((irq==switch_irq) && !gpio_get_value(GPIO_LED))
			gpio_set_value(GPIO_LED, 1);
		else //IRQ발생 & LED ON일때
			gpio_set_value(GPIO_LED, 0);
	
		printk(KERN_INFO " Called isr_func():%d\n", count);
		count++;
	}
	else
	{
		flag=0;
	}
	return IRQ_HANDLED;
}

	
static int gpio_open(struct inode *inod, struct file *fil)
{
	//모듈의 사용 카운트를 증가 시킨다.
	try_module_get(THIS_MODULE);
	printk(KERN_INFO "GPIO Device opened\n");
	return 0;
}	


static int gpio_close(struct inode *inod, struct file *fil)
{
	//모듈의 사용 카운트를 감소 시킨다.
	module_put(THIS_MODULE);
	printk(KERN_INFO " GPIO Device closed\n");
	return 0;
}

static ssize_t gpio_read(struct file *fil, char *buff, size_t len, loff_t *off)
{
	int count;
	// <linux/gpio.h>파일에 있는 gpio_get_value()를 통해
	// gpio의 상태값을 읽어온다. 
	if(gpio_get_value(GPIO_LED))
		msg[0]='1';
	else
		msg[1]='0';

	// 이 데이터가 커널에서 온 데이터임을 표기한다.
	strcat(msg," from kernel");

	//커널영역에 있는 msg문자열을 유저영역의 buff주소로 복사 
	count = copy_to_user(buff,msg,strlen(msg)+1);

	printk(KERN_INFO "GPIO Device read:%s\n",msg);
	
	return count;	
}


static ssize_t gpio_write(struct file *fil, const char *buff, size_t len, loff_t *off)
{
	int count;
	memset(msg, 0, BLK_SIZE);

	count = copy_from_user(msg, buff, len);
	
	if(!strcmp(msg,"0"))
	{
		del_timer_sync(&timer); //동작이 만료될때까지 timer중단시킴 즉 중간에 꺼져도 timer는 중지되어서 커널에 문제가 생가지않음

	}
	else
	{
		init_timer(&timer); //시간이 지나면 timer_func들어가서 동작을 함
		timer.function=timer_func; //동작함수 알려줌
		timer.data=1L; //long값을 인자로 넘겨준다 (timer_func으로 전달하는 인자값)
		timer.expires = jiffies + (1*HZ); //언제 만료가 되는가 = jiffies(현재시간) +(원하는 시간 곱하거나 나누기) HZ(100설정됨 그냥 1초임) 1초뒤 만료
		
		add_timer(&timer); //timer가 설치가됨 이후부터 타이머 시작
	}

	gpio_set_value(GPIO_LED, (strcmp(msg,"0")));
	printk(KERN_INFO "GPIO Device write:%s\n",msg);
	return count;
}

static int __init initModule(void)
{
	dev_t devno;
	int err;
	int count;
	
	printk("Called initModule()\n");
	
	// 1. 문자디바이스 드라이버를 등록한다.
	devno = MKDEV(GPIO_MAJOR, GPIO_MINOR);
	register_chrdev_region(devno,1,GPIO_DEVICE);
	
	// 2. 문자 디바이스를 위한 구조체를 초기화 한다.
	cdev_init(&gpio_cdev, &gpio_fops);
	gpio_cdev.owner = THIS_MODULE;
	count=1;
	
	// 3. 문자디바이스를 추가
	err=cdev_add(&gpio_cdev,devno,count);
	if(err<0)
	{
		printk(KERN_INFO "Error: cdev_add()\n");
		return -1;
	}
	
	printk(KERN_INFO "'sudo mknod /dev/%s c %d 0'\n", GPIO_DEVICE, GPIO_MAJOR);
	printk(KERN_INFO "'sudo chmod 666 /dev/%s'\n", GPIO_DEVICE);
	
	// 현재 GPIO_LED핀이 사용중인지 확인하고 사용권한 획득
	err=gpio_request(GPIO_LED, "LED");
	if(err==-EBUSY)
	{
		printk(KERN_INFO "Error gpio_request LED\n");
		return -1;
	}

	// 현재 GPIO_SW핀이 사용중인지 확인하고 사용권한 획득
	err=gpio_request(GPIO_SW, "SW");
	if(err==-EBUSY)
	{
		printk(KERN_INFO "Error gpio_request SW\n");
		return -1;
	}

	gpio_direction_output(GPIO_LED, 0);
	switch_irq=gpio_to_irq(GPIO_SW);	
	err=request_irq(switch_irq, isr_func, IRQF_TRIGGER_RISING,"switch",NULL);
        if(err)
	{
		printk(KERN_INFO "Error request_irq\n");
		return -1;
	}	

	return 0;
}

static void __exit cleanupModule(void)
{
	dev_t devno = MKDEV(GPIO_MAJOR, GPIO_MINOR);
	del_timer_sync(&timer);

	// 1.문자 디바이스의 등록을 해제한다.
	unregister_chrdev_region(devno,1);

	// 2.문자 디바이스의 구조체를 삭제한다.
	cdev_del(&gpio_cdev);

	gpio_direction_output(GPIO_LED, 0);

	//request_irq에서 받아온 사용권한을 반납한다.
	free_irq(switch_irq, NULL);

	//gpio_request()에서 받아온 사용권한을 반납한다.
	gpio_free(GPIO_LED);
	gpio_free(GPIO_SW);
	
	
	printk("Good-bye!\n");
}


//sudo insmod 호출되는 함수명 정의
module_init(initModule);

//sudo rmmod 호출되는 함수명 정의
module_exit(cleanupModule);

//모듈의 정보
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GPIO Driver Module");
MODULE_AUTHOR("Heejin Park");
