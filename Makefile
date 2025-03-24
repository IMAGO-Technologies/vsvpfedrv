# Comment/uncomment the following line to disable/enable debugging
# or call make DEBUG=y default is DEBUG=n
#DEBUG = y

ifeq ($(DEBUG),y)
  DEBFLAGS = -O -g -DDEBUG # "-O" is needed to expand inlines
  $(info we use debug flags [${DEBFLAGS}])
else
  DEBFLAGS = -O2  
endif

ccflags-y := $(DEBFLAGS) -Werror -Wall -Wno-unused-parameter -Wno-date-time 
vsvpfedrv-objs := VSDrv.o AM473X.o FileOps.o BufferCtrl.o
obj-m	:= vsvpfedrv.o

# If KERNELDIR is defined, we've been invoked from the DKMS
# otherwise we were called directly from the command line
# '?=' has only an effect if the variable not defined
KERNELDIR ?= /lib/modules/$(shell uname -r)/build


default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules 

# create a file like 'vsvpfedrv4.9.59-rt23-visioncam-xm-1.0.0.0_armv7l.ko'
deploy:
	make clean
	make
	strip --strip-debug vsvpfedrv.ko
	mv vsvpfedrv.ko vsvpfedrv_$(shell uname -r)_$(shell uname -m).ko

devel:
	make 
	make install
	rmmod vsvpfedrv
	modprobe vsvpfedrv

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions modules.order Module.symvers

# default: /lib/modules/$(KERNELRELEASE)/extra
install:
	make -C $(KERNELDIR) M=$(PWD) modules_install
	depmod -a 
