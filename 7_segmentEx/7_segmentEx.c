#include <linux/fs.h> //struct file_operaton ���� �ý���
#include <linux/cdev.h> // ���� ����̽� : register_chrdev_region(),cdev_init()
						//cdev_add() , cdev_del()
#include <linux/io.h>  //ioremap  ,  iounmap()
#include <linux/uaccess.h> //copy to user , copy from user
#include <linux/init.h> //init
#include <linux/module.h>
#include <linux/kernel.h> //pr_info
#include <linux/gpio.h>  //gpio_get_value , gpio_set_value, request_gpio , gpiofree

//hrtimer timer ���� �Լ�
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

#define MS_TO_NS(x) (x * 1E6L) // ms -> ns ��ȯ

struct cdev gpio_cdev;  //cdev ����ü����
static struct hrtimer hr_timer;  //hrtimer ����ü ����


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

//����: http://jake.dothome.co.kr/hrtimer/ 
//�ð��Լ�

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
		currtime = ktime_get(); //����ð��� �޾ƿ�
		interval = ktime_set(0, MS_TO_NS(delay_in_ms)); //���ϴ� �����ð� ����
		hrtimer_forward(timer, currtime, interval); 
		/* hrtimer_forward  - ����ð� �缳��
			����� Ÿ�̸ӿ� ���� ����ð����� ���� interval ������ currtime�� ���� �ð����� 
			����ð��� ����
		*/
		num++;
		return HRTIMER_RESTART;
	}	
}



//mapping �Ǵ� �Լ� ����ü file_operation 
static struct file_operations gpio_fops = {
	.owner = THIS_MODULE,
	/*.read = gpio_read,
	.write = gpio_write,
	.open = gpio_open,
	.release = gpio_close,*/
};

/*
//��ġ open				inode�ּ�		file ���� �ּ� 
static int gpio_open(struct inode *inod, struct file *fil)
{
	//����� ��� ī��Ʈ�� ������Ų��
	try_module_get(THIS_MODULE);
	try_module_get
	���� ����ġ�� ����ϰ� �ִ� ���μ����� ������ �˷��ش�
	���μ����� ��ġ�� ����ϰ� �������� rmmod�� �Ұ����ϴ� ���� open�� �̰��� üũ
	

	
}
*/



static int __init initModule(void)
{
	dev_t devno; //dev_t ����̽� ������ MAJOR, MINOR��ȣ�� ���� ������ ����
	int err;
	int count;


	//�ð� ���� ����
	ktime_t ktime; //ns ����
	unsigned long delay_in_ms = 1000L;


	pr_info("called initModule()\n");

	//init : register_chrdev  -> cdev_init ->  cdev_add -> gpio_request

	//kernel�� char����̽� ���̺� ��������

	//1. GPIO_MAJOR ,GPIO_MINOR ,  DEVICE �̸� ���
	devno = MKDEV(GPIO_MAJOR, GPIO_MINOR); //devno���·� MARJOR MINOR�� ����
	register_chrdev_region(devno, 1, GPIO_DEVICE);
	/*
		register_chrdev_region(dev_t , ���� , ����̽� �̸�) :  ���ڵ���̽��� ���
	*/




	//2. ���� ����̽��� ���� ����ü �ʱ�ȭ
	cdev_init(&gpio_cdev, &gpio_fops); // cdev ����ü�� fops ����ü �ּ� �Ѱܼ� 
										//����ü �ʱ�ȭ
	gpio_cdev.owner = THIS_MODULE; // ����ü .owner ���� THIS_MODULE �ֱ�
	count = 1; //����̽� ����



	//3. ���� ����̽��� �߰�
	err = cdev_add(&gpio_cdev, devno, count); //Ŀ���� ����̹��� �߰�
	/*
	���������� kobj_map()�� �̿��� ���� ����̽� ����ü�� ����̽� ��ȣ�� ����
	cdev_add( the cdev structure for the device,
			  ù��° ����̽� ��ġ�� number (= devno),
			  ���̳ʳѹ� ���� (��ġ ����)

	*/
	if (err < 0)
	{
		pr_info("Error: cdev_add()\n");
		return -1;
	}
	
	pr_info("'sudo mknod /dev/%s c %d 0'\n", GPIO_DEVICE, GPIO_MAJOR);
	pr_info("'sudo chmod 666 /dev/%s'\n", GPIO_DEVICE);
	   
	//4. gpio �� ������ ȹ��
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
	
	//Pin �ʱ�ȭ
	gpio_direction_output(A, 1);
	gpio_direction_output(B, 1);
	gpio_direction_output(C, 1);
	gpio_direction_output(D, 1);
	gpio_direction_output(E, 1);
	gpio_direction_output(F, 1);
	gpio_direction_output(G, 1);
	gpio_direction_output(DP, 1);



	

	pr_info("HR Timer module installing\n");

	ktime = ktime_set(0, MS_TO_NS(delay_in_ms)); //ktime ���� ns ����

	hrtimer_init(&hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL); //�ð� �ʱ�ȭ

	hr_timer.function = &my_hrtimer_callback; // �ð��̵Ǹ� ����Ǵ� callback�Լ�
	hrtimer_start(&hr_timer, ktime, HRTIMER_MODE_REL);
	//			ī���� �ð�  ����	��� �ð����� ���� �ð����� ���� ��û�� ktime(ns)�� ���� �Ŀ� ����




	return 0;
}

static void __exit cleanupModule(void)
{
	int ret;
	dev_t devno;
	devno= MKDEV(GPIO_MAJOR, GPIO_MINOR); //MAJOR MINOR dev_t �������� ��ȯ

	//pin �ʱ�ȭ
	gpio_direction_output(A, 1);
	gpio_direction_output(B, 1);
	gpio_direction_output(C, 1);
	gpio_direction_output(D, 1);
	gpio_direction_output(E, 1);
	gpio_direction_output(F, 1);
	gpio_direction_output(G, 1);
	gpio_direction_output(DP, 1);
	
	//gpio_request()���� �޾ƿ� �������� �ݳ��Ѵ�
	gpio_free(A);
	gpio_free(B);
	gpio_free(C);
	gpio_free(D);
	gpio_free(E);
	gpio_free(F);
	gpio_free(G);
	gpio_free(DP);
	//unregister_chrdev_region -> cdev_del

	//1. ���� ����̽� �������
	unregister_chrdev_region(devno, 1);

	//2. ���ڵ���̽� ����ü�� ����
	cdev_del(&gpio_cdev); //�Ҵ�� �ڿ��� �ݳ�

	ret = hrtimer_cancel(&hr_timer); //0�̾�� �������

	if (ret)
		pr_info("The timer was still in use ...\n");

	pr_info("HR Timer module uninstalling\n");

	pr_info("Good-bye!\n");
	
}

//insmod �Լ� ����
module_init(initModule);
//rmmod �Լ� ����
module_exit(cleanupModule);
//module ����
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GPIO Driver Moudle");
MODULE_AUTHOR("lee");
