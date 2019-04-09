#Makefile
#kernel dircetory
KDIR = /lib/modules/$(shell uname -r)/build   #리눅스의 소스(linux/init.h ..)가 있는 경로를 커널(KDIR)에 알려준다
                                                 #커널버전마다 include 파일이 다르다 (버전을 맞추기위해 uname -r하는것)
                                                 # + 모듈은 커널보다 버전이 같거나 최소한 낮아야함 높으면x
   
obj-m := helloModule.o
   
default:
	$(MAKE) -C $(KDIR) M=$$PWD modules
clean:
	$(MAKE) -C $(KDIR) M=$$PWD clean
