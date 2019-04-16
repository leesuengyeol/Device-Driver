//timer �� �̿��� led on/off

//================================
// Character device driver example 
// GPIO driver
//================================
#include <linux/fs.h>		//open(), read() ,write() close() Ŀ�� �Լ�

#include <linux/cdev.h>		//register_chrdev_region(), cdev_init()
							//cdev_add() , cdev_del()
#include <linux/module.h>

//#include <linux/io.h>		//ioremap(), iounmap()

#include <linux/uaccess.h>	//copy_from_user(), copy_to_user()


#include <linux/gpio.h> 	//request_gpio() , gpio_setvalue()
							//gpio_get_value(), gpio_free()

#include <linux/interrupt.h>//gpio_to_irq(), request_irq(), free_irq()
#include <linux/signal.h>	//signal�� ���
#include <linux/sched/signal.h>	//siginfo ����ü�� ����ϱ� ����
#include <linux/hrtimer.h>		//high-resolution timer
#include <linux/ktime.h>

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
static struct task_struct *task;	//�׽�ũ�� ���� ����ü -> ���μ��� �ϳ��� �׽�ũ����ü�� ������ �ִ� 
									//�̰��� �̿��� app���� pid�� �˾Ƴ��� signal ���۰���
pid_t pid;
char pid_valid;
static struct hrtimer hr_timer;
int flag=0;

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

enum hrtimer_restart myTimer_callback(struct hrtimer *timer)
{
	printk(KERN_INFO "myTimer_callback\n");
	flag =0;
	return HRTIMER_NORESTART;
}

static irqreturn_t isr_func(int irq, void *data)
{
	//IRQ�߻� & LED�� OFF�϶� 
	static int count;
	ktime_t ktime;
	unsigned long expireTime = 1000000L; //unit:ns

	static struct siginfo sinfo;
	if (flag == 0)
	{	//ktime_set(������,����������)
		
		flag=1;

		ktime=ktime_set(0,expireTime);
		//timer_init(Ÿ�̸� ����ü �ּҰ�, ����� Ÿ�̸Ӱ�, ���ð����� ����)
		hrtimer_init(&hr_timer, CLOCK_MONOTONIC,HRTIMER_MODE_REL);
		hr_timer.function = &myTimer_callback;
		printk(KERN_INFO"startTime\n");
		hrtimer_start(&hr_timer, ktime,HRTIMER_MODE_REL);

		if((irq==switch_irq) && !gpio_get_value(GPIO_LED))
		{
			gpio_set_value(GPIO_LED, 1);

			//����ġ�� ������ �� �������α׷����� SIGIO�� �����Ѵ�
			//sinfo ����ü�� 0���� �ʱ�ȭ �Ѵ�.
			memset(&sinfo, 0, sizeof(struct siginfo));
			sinfo.si_signo=SIGIO;
			sinfo.si_code = SI_USER;
			if(task!=NULL)
			{
				//kill()�� ������ kernel�Լ�
				send_sig_info(SIGIO,&sinfo, task); //task�� ����Ű�� ��ġ�� sig info �� SIGIO�� �Ѱ��ش�

			}
			else
			{
				printk(KERN_INFO "Error: USER PID\n");
			}
		}
		else //IRQ�߻� & LED ON�϶�
			gpio_set_value(GPIO_LED, 0);
	}
		printk(KERN_INFO " Called isr_func():%d\n", count);
		count++;
	
		return IRQ_HANDLED;
}

	
static int gpio_open(struct inode *inod, struct file *fil)
{
	//����� ��� ī��Ʈ�� ���� ��Ų��.
	try_module_get(THIS_MODULE);
	printk(KERN_INFO "GPIO Device opened\n");
	return 0;
}	


static int gpio_close(struct inode *inod, struct file *fil)
{
	//����� ��� ī��Ʈ�� ���� ��Ų��.
	module_put(THIS_MODULE);
	printk(KERN_INFO " GPIO Device closed\n");
	return 0;
}

static ssize_t gpio_read(struct file *fil, char *buff, size_t len, loff_t *off)
{
	int count;
	// <linux/gpio.h>���Ͽ� �ִ� gpio_get_value()�� ����
	// gpio�� ���°��� �о�´�. 
	if(gpio_get_value(GPIO_LED))
		msg[0]='1';
	else
		msg[1]='0';

	// �� �����Ͱ� Ŀ�ο��� �� ���������� ǥ���Ѵ�.
	strcat(msg," from kernel");

	//Ŀ�ο����� �ִ� msg���ڿ��� ���������� buff�ּҷ� ���� 
	count = copy_to_user(buff,msg,strlen(msg)+1);

	printk(KERN_INFO "GPIO Device read:%s\n",msg);
	
	return count;	
}


static ssize_t gpio_write(struct file *fil, const char *buff, size_t len, loff_t *off)
{
	int count;
	char *cmd, *str;
	char *sep=":";
	char *endptr, *pidstr;

	memset(msg, 0, BLK_SIZE);

	count = copy_from_user(msg, buff, len); // user���� buf�� �о�� msg����
	str=kstrdup(msg, GFP_KERNEL);	
	//kstrdup : ���� ���ڿ��� ���� ������ �Ҵ��ϰ� ����
	//GFP_KERNEL:�޸� �Ҵ��� �׻� �����ϵ��� �䱸 , �޸𸮰� ������� ���� ���� ȣ���� ���μ����� ���߰� �����޷θ� �Ҵ��� �� �ִ� ���°� �ɶ����� ��� , �޸鰡��
	
	cmd = strsep(&str, sep); //argv[1]�� pid����
							//sep(:) �ݷ� �������� ���ڿ��� �ɰ��ش� - > �ݷоտ��ִ¹��ڿ��� �����Ѵ�
							//cmd = argv[1]

	pidstr= strsep(&str,sep); //pid����

	//cmd�� ���ڿ��� �νĽ�Ű������
	cmd[1]='0'; //�ι��ڸ� �ٿ���� ���ڿ� ����Ͽ� ���׸����̼� ������ ��������

	printk(KERN_INFO "cmd:%s, pid:%s\n",cmd,pidstr);
	
	if(!strcmp(cmd,"end"))
	{
		pid_valid=0;
	}
	else
	{
		pid_valid=1;
	}

	//pidstr���ڿ��� ���ڷ� ��ȯ
	pid=simple_strtol(pidstr, &endptr, 10);
	printk(KERN_INFO "pid=%d\n",pid);

	if(endptr != NULL) //������
	{
		task = pid_task(find_vpid(pid), PIDTYPE_PID); //pid���� �־ task�� �ּҰ��� �����´�
		if(task==NULL)
		{
			printk(KERN_INFO "Error:Can't find PID from user APP\n");
			return 0;
		}
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
	
	// 1. ���ڵ���̽� ����̹��� ����Ѵ�.
	devno = MKDEV(GPIO_MAJOR, GPIO_MINOR);
	register_chrdev_region(devno,1,GPIO_DEVICE);
	
	// 2. ���� ����̽��� ���� ����ü�� �ʱ�ȭ �Ѵ�.
	cdev_init(&gpio_cdev, &gpio_fops);
	gpio_cdev.owner = THIS_MODULE;
	count=1;
	
	// 3. ���ڵ���̽��� �߰�
	err=cdev_add(&gpio_cdev,devno,count);
	if(err<0)
	{
		printk(KERN_INFO "Error: cdev_add()\n");
		return -1;
	}
	
	printk(KERN_INFO "'sudo mknod /dev/%s c %d 0'\n", GPIO_DEVICE, GPIO_MAJOR);
	printk(KERN_INFO "'sudo chmod 666 /dev/%s'\n", GPIO_DEVICE);
	
	// ���� GPIO_LED���� ��������� Ȯ���ϰ� ������ ȹ��
	err=gpio_request(GPIO_LED, "LED");
	if(err==-EBUSY)
	{
		printk(KERN_INFO "Error gpio_request LED\n");
		return -1;
	}

	// ���� GPIO_SW���� ��������� Ȯ���ϰ� ������ ȹ��
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
	int ret;

	dev_t devno = MKDEV(GPIO_MAJOR, GPIO_MINOR);
	
	ret=hrtimer_cancel(&hr_timer);
	if(ret)
			printk(KERN_INFO "myTimer was still in use\n");
	
	printk(KERN_INFO "myTimer is uninstall\n");

	// 1.���� ����̽��� ����� �����Ѵ�.
	unregister_chrdev_region(devno,1);

	// 2.���� ����̽��� ����ü�� �����Ѵ�.
	cdev_del(&gpio_cdev);

	gpio_direction_output(GPIO_LED, 0);

	//request_irq���� �޾ƿ� �������� �ݳ��Ѵ�.
	free_irq(switch_irq, NULL);

	//gpio_request()���� �޾ƿ� �������� �ݳ��Ѵ�.
	gpio_free(GPIO_LED);
	gpio_free(GPIO_SW);
	
	
	printk("Good-bye!\n");
}


//sudo insmod ȣ��Ǵ� �Լ��� ����
module_init(initModule);

//sudo rmmod ȣ��Ǵ� �Լ��� ����
module_exit(cleanupModule);

//����� ����
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GPIO Driver Module");
MODULE_AUTHOR("Heejin Park");