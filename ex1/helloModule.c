#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

static int __init hello_init(void)
{
	printk("called Hello_init()\n");
	return 0;
}

static void __exit hello_exit(void)
{
	printk("called hello_exit()\n");
}
 
 
module_init(hello_init); //Ŀ�ο� ��� insert (�����Ҷ��� hello_init�Լ� ȣ��)
module_exit(hello_exit); //Ŀ�ο� ��� remove                 "
  
//���̼��� , ��� , ������ �������� warning�� �ȶ�
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("hello Moudle");
MODULE_AUTHOR("lee");
  
  
/*
Ȯ�� ��ɾ�
isnmod xxx.ko ->insert moudle (.ko = kernel object)
dmesg -> kernel log���Ȯ��
lsmod -> ��� list
rmmod xxx  ->remove module (�������� Ȯ������ ko�� ���ĵ� �ȴ� ������ �̹� ��� list�� �ö� �ֱ� ������ ����� ����)
*/
