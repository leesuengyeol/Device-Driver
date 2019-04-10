/*

 ���� ����̽� �����!!
 Ŀ�� ������ ����̽� �� 3���� ����

1. character device (���� ����̽�)
- ����ó�� ����Ʈ ��Ʈ��(stream)���� ����(read/write) �Ҽ� �ִ� ����̽�
-Ŀ�ξȿ� �ֱ⵵�ϸ� ���� �߰��� �� �ִ�

2. block device
-�ϵ��ũó�� ���ο� ���� �ý����� ���� �� �ִ� ����̽�
-����Ʈ�� Ư�� ���� �ý����� ���Ͽ� ������ ��, ��� ����̽� ����̹��� ��ȣ �����ϰ� ��
-�� ���丮�� ���� ��ϴ�����  �ϵ��ũ�� �����ؾ��ϴ� ��� ��ϵ���̽� ����̹��� ���ؼ� ����ȴ�(�����Ұ�)

3. network device
-��� ��Ʈ��ũ�� ������ ��Ʈ��ũ �������̽��� ���� �̷������


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
//��������� GPIO  �����޸�
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
static int switch_irq; // irq handler �� �ٸ� �Լ������� ��밡���ϰԲ�

static int gpio_open(struct inode *, struct file *); //file_operation �� �Լ��̸� ��������Ѵ�!!
static int gpio_close(struct inode *, struct file *);
static ssize_t gpio_read(struct file *, char *buff, size_t , loff_t *); //user ������ ������ ����
static ssize_t gpio_write(struct file *, const char *, size_t , loff_t *);

static struct file_operations gpio_fops={
	.owner   = THIS_MODULE,
	.read    = gpio_read,
	.write   = gpio_write,
	.open    = gpio_open,
	.release = gpio_close,
};

static irqreturn_t isr_func(int irq, void *data) //����ġ�� ������ ���� isr_func�� ����
{
	//IRQ�߻� & LED�� OFF�϶�
	static int count; //���Լ��ȿ����� ���氡���� ��������
	if((irq == switch_irq) && !gpio_get_value(GPIO_LED)) //irq�߻� & led�� of�϶�
			gpio_set_value(GPIO_LED, 1); //led on ����
	else //irq�߻� & led�� on �϶�
			gpio_set_value(GPIO_LED, 0);

	printk(KERN_INFO " Called isr_func(): %d\n",count);
	
	count++;

	return IRQ_HANDLED;
}



//						inode�ּ� 			file ���� �ּ� �� ���ڷ�
static int gpio_open(struct inode *inod, struct file *fil) //file_operation �� �Լ��̸� ��������Ѵ�!!
{
	//����� ��� ī��Ʈ�� ������Ų��
	try_module_get(THIS_MODULE);//try_module_get�� ���� ����ġ�� ����ϰ� �ִ� ���μ����� ������ �˷��ش� 
								//���μ����� ��ġ�� ����ϰ� �������� rmmod�� �Ұ����ϴ� ���� open�� �̰��� äũ
	printk(KERN_INFO "GPIO Device opened\n");
	return 0;
}



static int gpio_close(struct inode *inod, struct file *fil)
{
	//����� ��� ī��Ʈ�� ���ҽ�Ų��.
	module_put(THIS_MODULE); // <-> try_module_get()
	printk(KERN_INFO "GPIO Device closed\n");
	return 0;
}


						//������ ��ġ	����۸� �о���� , �о�� ������ ,������ �����°�
static ssize_t gpio_read(struct file *fil, char *buff, size_t len, loff_t *off) //user ������ ������ ����
{
	int count;
	if(gpio_get_value(GPIO_LED)) //gpio_get_value = ���� gpio �� ���� �о��
			msg[0]='1';
	else
			msg[1]='0';
	strcat(msg," from kernel"); // print msg + from kernel
	count = copy_to_user(buff,msg,strlen(msg)+1); //�����ܿ� msg ������ +1 ��  �ι���
	
	printk(KERN_INFO "GPIO Device read: %s\n",msg);

	return count;
	
}



static ssize_t gpio_write(struct file *fil, char const *buff, size_t len, loff_t *off)
{
	int count;
	memset(msg, 0, BLK_SIZE);

	count = copy_from_user(msg,buff,len); //�������� buff������
	gpio_set_value(GPIO_LED, (strcmp(msg, "0"))); //gpio �� ���� ����

	printk(KERN_INFO "GPIO Device write: %s\n", msg);

	return count;
}



static int __init initModule(void)
{
	dev_t devno; //dev_t ����̽� ������ MAJOR , MINOR ��ȣ�� �����ϱ� ���� ����Ÿ��(32bit)
	int err;
	//static void *map;
	int count;

	printk("called initModule()\n");
	
	// 1. ���ڵ���̽� ����̹��� ����Ѵ�
	devno = MKDEV(GPIO_MAJOR, GPIO_MINOR); //MAJOR , MINOR ���� �̿��Ͽ� dev_t���� ��´�
	register_chrdev_region(devno,1,GPIO_DEVICE); //dev_t ���� ���� �־ ����̽� ����̹� ���



/*

	register_chrdev_region() :  ���ڵ���̽��� ����Ѵ�
	
	�Լ��� ���۰���
	1. struct char_device_struct ���� -> 2. __register_chrdev_region() ���ڵ���̽� ��� -> 3. cdev_alloc()
	
	struct cdev {
	struct kobject kobj;
	struct module *owner;
	const struct file_operations *ops;
	struct list_head list;
	dev_t dev;
	unsigned int count;
	};
	
	cdev ����ü ���� ������� char_device_struct���� ��� ����(alloc)
	=> Ŀ�ο��� char_device_struct�� �������� �ʰ� cdev�� ���� , ����ϵ����ϱ� ����

*/



	//2. ���� ����̽��� ���� ����ü�� �ʱ�ȭ �Ѵ�
	cdev_init(&gpio_cdev, &gpio_fops); //����̽��� �ʱ�ȭ �ϰ� , ����ڰ� ������ fops�� ������ cdev�� struct file_operation�� ���
	gpio_cdev.owner = THIS_MODULE;
	count=1;

	//3. ���ڵ���̽��� �߰�
	err=cdev_add(&gpio_cdev, devno,count); // Ŀ���� ����̹��� �߰�
											//���������� kobj_map()�� �̿��� ���� ����̽� ����ü�� ����̽� ��ȣ�� ����
	if(err<0)
	{
		printk(KERN_INFO "Error: cdev_add()\n");
		return -1;
	}
	printk(KERN_INFO "'sudo mknod /dev/%s c %d 0'\n",GPIO_DEVICE, GPIO_MAJOR);
	printk(KERN_INFO "'sudo chmod 666 /dev/%s'\n",GPIO_DEVICE);
	
	
	//���� GPIO_LED���� ��������� Ȯ���ϰ� ������ ȹ��
	err= gpio_request(GPIO_LED, "LED"); 
	
	if(err==-EBUSY)
	{
		printk(KERN_INFO "Error gpio_request LED\n");
		return -1;
	}
	/* =
	/4. �����޸� ������ ���ڰ��� �����ϸ� ����޸� ������ ���� (ioremap)
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


	//���� GPIO_SW���� ��������� Ȯ���ϰ� ������ ȹ��
	err= gpio_request(GPIO_SW, "SW"); 
	
	if(err==-EBUSY)
	{
		printk(KERN_INFO "Error gpio_request SW\n");
		return -1;
	}


	gpio_direction_output(GPIO_LED,0); //gpio select function= output ,pin =GPIOLED (18), gpio_value=0
										//chipset ȸ�翡�� �̷��� �Լ��� �����������μ� �츮�� ���������� register�� �ǵ����� �ʾƵ� �ȴ�
/* = GPIO_OUT(GPIO_LED);
	 //GPIO_CLR(GPIO_LED); //���α׷��� ���۵ɶ� led�� ������ ����  
	 GPIO_SET(GPIO_LED); //test
*/
	

	switch_irq=gpio_to_irq(GPIO_SW);
	//= gpio_direction_input(GPIO_SW); ���ͷ�Ʈ�� ����ϱ� ���ؼ� gpio_to_irq�� ��� ����Ѵ�
	err= request_irq(switch_irq, isr_func, IRQF_TRIGGER_RISING, "switch", NULL); 
				//								rising edge �϶�

	//request_irq�� ���� �ȵɶ�
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

	//gpio_request()���� �޾ƿ� �������� �ݳ��Ѵ�.
	gpio_free(GPIO_LED);
	gpio_free(GPIO_SW);
	// 1. ���ڵ���̽� ��� ����
	unregister_chrdev_region(devno,1);

	// 2. ���ڵ���̽� ����ü�� ����
	//�Ҵ�� �ڿ��� �ݳ�
	
	/*if(gpio) //if(gpio) == ����� gpio ������ �ִٸ� 1�̱⶧���� �ݳ��ϰԵȴ�
			iounmap(gpio);
	*/
	
	printk("Good-bye!\n");
}

//insmod �Լ� ����
module_init(initModule);
//rmmod �Լ� ����
module_exit(cleanupModule);
//module ����
MODULE_LICENSE("GPL"); 
MODULE_DESCRIPTION("GPIO Driver Moudle");
MODULE_AUTHOR("lee");
