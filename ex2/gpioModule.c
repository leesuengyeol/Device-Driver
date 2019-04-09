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
#include <asm/uaccess.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#define GPIO_MAJOR 200
#define GPIO_MINOR 0
#define GPIO_DEVICE "gpioled"
#define GPIO_LED 18
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


//#define DEBUG

struct cdev gpio_cdev;
volatile unsigned int *gpio;



static struct file_operations gpio_fops={
	.owner   = THIS_MODULE,
//	.read    = gpio_read,
//	.write   = gpio_write,
//	.open    = gpio_open,
//	.release = gpio_close,
};

static int __init initModule(void)
{
	dev_t devno; //dev_t ����̽� ������ MAJOR , MINOR ��ȣ�� �����ϱ� ���� ����Ÿ��(32bit)
	int err;
	static void *map;
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

	//4. �����޸� ������ ���ڰ��� �����ϸ� ����޸� ������ ���� (ioremap)
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

	GPIO_OUT(GPIO_LED);
	//GPIO_CLR(GPIO_LED); //���α׷��� ���۵ɶ� led�� ������ ����  
	GPIO_SET(GPIO_LED); //test

	return 0;
}

static void __exit cleanupModule(void)
{
	dev_t devno = MKDEV(GPIO_MAJOR,GPIO_MINOR);
	GPIO_CLR(GPIO_LED);
	// 1. ���ڵ���̽� ��� ����
	unregister_chrdev_region(devno,1);

	// 2. ���ڵ���̽� ����ü�� ����
	cdev_del(&gpio_cdev);

	//�Ҵ�� �ڿ��� �ݳ�
	if(gpio) //if(gpio) == ����� gpio ������ �ִٸ� 1�̱⶧���� �ݳ��ϰԵȴ�
			iounmap(gpio);

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
