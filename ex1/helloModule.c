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
 
 
module_init(hello_init); //커널에 모듈 insert (시작할때는 hello_init함수 호출)
module_exit(hello_exit); //커널에 모듈 remove                 "
  
//라이센스 , 요약 , 만든사람 명시해줘야 warning이 안뜸
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("hello Moudle");
MODULE_AUTHOR("lee");
  
  
/*
확인 명령어
isnmod xxx.ko ->insert moudle (.ko = kernel object)
dmesg -> kernel log기록확인
lsmod -> 모듈 list
rmmod xxx  ->remove module (내릴때는 확장자인 ko를 안쳐도 된다 모듈명이 이미 모듈 list에 올라가 있기 때문에 상관이 없다)
*/
