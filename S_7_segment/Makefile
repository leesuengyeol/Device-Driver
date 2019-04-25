#kernel directory
KDIR = /lib/modules/$(shell uname -r)/build

obj-m := S_7_segment.o

default:
	$(MAKE) -C $(KDIR) M=$$PWD modules

clean:
	$(MAKE) -C $(KDIR) M=$$PWD clean
