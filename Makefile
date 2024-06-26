RM=rm -f
CP=cp
TAR=tar
GZIP=gzip
MKDIR=mkdir

MODULE = b4b

EXTRA_CFLAGS =
KDIR:=/lib/modules/$(shell uname -r)/build
#KDIR:/usr/src/linux-$(shell uname -r)

EXTRA_CFLAGS += -isystem -I$(KDIR)/drivers/usb/serial -Wall -Werror 
#CFLAGS +=  -g
EXTRA_CFLAGS += -g
EXTRA_CFLAGS += -DGSPCA_DEBUG

# Comment/uncomment the following line to disable/enable debugging
DEBUG = n
# Add your debugging flag (or not) to CFLAGS
ifeq ($(DEBUG),y)
  DEBFLAGS = -O -g -DSCULL_DEBUG # "-O" is needed to expand inlines
else
  DEBFLAGS = -O2
endif

EXTRA_CFLAGS += $(DEBFLAGS)


default:
	$(MAKE) -C $(KDIR) EXTRA_CFLAGS="$(EXTRA_CFLAGS)" M=$(PWD) 

clean:
	$(RM) *.mod.c *.o *.ko .*.cmd *~ Modules.* modules.* .cache.mk

load:
	insmod $(MODULE).ko

unload:
	rmmod $(MODULE)

install:
	insmod $(MODULE).ko

uninstall:
	-rmmod $(MODULE)

.PHONY: default clean load unload install uninstall dist

go: default
	-rmmod $(MODULE)
	insmod ./$(MODULE).ko debug=0xFF
	echo 'ready2roll'
