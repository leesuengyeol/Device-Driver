#kernel directory
KDIR = /lib/modules/$(shell uname -r)/build

obj-m := 7_segmentEx.o

default:
	$(MAKE) -C $(KDIR) M=$$PWD modules

clean:
	$(MAKE) -C $(KDIR) M=$$PWD clean