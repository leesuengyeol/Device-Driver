//mmap을 이용한 디바이스 드라이버
//mmap = 사용자 영역의 unallocated memory 에 resgister 적음 (보안이 안좋기 때문에 test용으로만 사용)
 
// 사용자 단계의 include 를 사용할 수 있다
 #include <stdio.h>
 #include <unistd.h>
 #include <stdlib.h>
 #include <fcntl.h>
 #include <sys/mman.h> //allow mmap function
 
 
// *data sheet 참조하기* 89쪽
 
 //Raspberry Pi3 PHYSICAL I/O Peripherals baseAddr
 #define BCM_IO_BASE 0x3F000000 // 실제 물리메모리 할당영역

 //GPIO_BaseAddr
 #define GPIO_BASE (BCM_IO_BASE + 0x200000) // 0x200000부터 gpio시작
 
 //GPIO Function Select Register 0 ~ 5
 #define GPIO_IN(g) (*(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))) //selectfunction register = 3bit , 32비트이므로 register 하나에 10개씩 (%10)
 #define GPIO_OUT(g) {(*(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))); \
									(*(gpio+((g)/10)) |= (1<<(((g)%10)*3))); } // 1번째줄:  0으로 초기화, 2번째줄:  원하는 비트 제어(001= output)
 
 
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
      
      //O_SYNC: 사용자단의 버퍼에 데이터가 입력됬을때 하드웨어 단까지 써져야 다음으로 넘어가게해줌
     
      {
          printf("Error : open() /dev/mem/\n");
          return -1;
      }
  //                  VirtualAddr                                                 //PhysicalAddr
   
      gpio_map = mmap(NULL,GPIO_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED , mem_fd, GPIO_BASE);
                //mmap(임의의 메모리설정 , 메모리사이즈, 읽고,쓰기,다른프로세서와 공유가능한 메모리설정,open된 장치의 fd,물리메모리 시작주소)
      
      void * mmap(void *start, size_t length, int prot, int flags, int fd, off_t offset);
      
      
      
      if(gpio_map == MAP_FAILED)
      {
          printf("Error: mmap() : %d\n", (int)gpio_map);
          return -1;
      }
 
      gpio = (volatile unsigned int *)gpio_map;
    
    
  /*    *volatile*
      
      cpu가 같은 번지를 중복해서 접근하려하면 번지를 직접 접근하지 않고 cache 에서 값을 읽어옴
      gpio는 cpu가 모르게 값이 변경되기 때문에
      volatile 을 사용하여 cache에서 값을 가져오지 않고 직접 gpio에 접근하여 가져옴
      DMA에 의해서 MEM이 변경될때도 volatile을 사용해야함
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
