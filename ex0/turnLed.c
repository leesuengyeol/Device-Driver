//mmap�� �̿��� ����̽� ����̹�
//mmap = ����� ������ unallocated memory �� resgister ���� (������ ������ ������ test�����θ� ���)
 
// ����� �ܰ��� include �� ����� �� �ִ�
 #include <stdio.h>
 #include <unistd.h>
 #include <stdlib.h>
 #include <fcntl.h>
 #include <sys/mman.h> //allow mmap function
 
 
// *data sheet �����ϱ�* 89��
 
 //Raspberry Pi3 PHYSICAL I/O Peripherals baseAddr
 #define BCM_IO_BASE 0x3F000000 // ���� �����޸� �Ҵ翵��

 //GPIO_BaseAddr
 #define GPIO_BASE (BCM_IO_BASE + 0x200000) // 0x200000���� gpio����
 
 //GPIO Function Select Register 0 ~ 5
 #define GPIO_IN(g) (*(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))) //selectfunction register = 3bit , 32��Ʈ�̹Ƿ� register �ϳ��� 10���� (%10)
 #define GPIO_OUT(g) {(*(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))); \
									(*(gpio+((g)/10)) |= (1<<(((g)%10)*3))); } // 1��°��:  0���� �ʱ�ȭ, 2��°��:  ���ϴ� ��Ʈ ����(001= output)
 
 
 //GPIO Pin Output Set 0 / clr 0
 //                  addr(20001C)
 #define GPIO_SET(g) (*(gpio+7) = (1<<g))
 //                  addr(200028)
 #define GPIO_CLR(g) (*(gpio+10) = (1<<g))

 #define GPIO_GET(g) (*(gpio+13)&(1<<g))
 #define GPIO_SIZE 0xB4
 
 volatile unsigned int *gpio;
 
 int main(int argc, char **argv)
 { 
      //GPIO Pin number
      int gno;
      int i, mem_fd;
 
      void *gpio_map;
 
      //if pin number not input
      if(argc<2)
      {
          printf("Usage:%s GPIO_NO\n", argv[0]);
          return -1;
      }
 
      gno= atoi(argv[1]);
 
      //device open /dev/mem -> register mem
      if((mem_fd = open("/dev/mem", O_RDWR | O_SYNC))<0) 
      
      //O_SYNC: ����ڴ��� ���ۿ� �����Ͱ� �Է����� �ϵ���� �ܱ��� ������ �������� �Ѿ������
     
      {
          printf("Error : open() /dev/mem/\n");
          return -1;
      }
  //                  VirtualAddr                                                 //PhysicalAddr
   
      gpio_map = mmap(NULL,GPIO_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED , mem_fd, GPIO_BASE);
                //mmap(������ �޸𸮼��� , �޸𸮻�����, �а�,����,�ٸ����μ����� ���������� �޸𸮼���,open�� ��ġ�� fd,�����޸� �����ּ�)
      
      void * mmap(void *start, size_t length, int prot, int flags, int fd, off_t offset);
      
      
      
      if(gpio_map == MAP_FAILED)
      {
          printf("Error: mmap() : %d\n", (int)gpio_map);
          return -1;
      }
 
      gpio = (volatile unsigned int *)gpio_map;
    
    
  /*    *volatile*
      
      cpu�� ���� ������ �ߺ��ؼ� �����Ϸ��ϸ� ������ ���� �������� �ʰ� cache ���� ���� �о��
      gpio�� cpu�� �𸣰� ���� ����Ǳ� ������
      volatile �� ����Ͽ� cache���� ���� �������� �ʰ� ���� gpio�� �����Ͽ� ������
      DMA�� ���ؼ� MEM�� ����ɶ��� volatile�� ����ؾ���
    */  
      GPIO_OUT(gno);
 
      for(i=0; i<5; i++)
      {
          GPIO_SET(gno); //on
          sleep(1);
          GPIO_CLR(gno); //off
          sleep(1);
      }
 
      munmap(gpio_map, GPIO_SIZE);
 
      return 0;
 }
