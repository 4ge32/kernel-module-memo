#
# Makefile for the FUSE filesystem.
# Standalone edition
#

obj-m := tuse.o
tuse-y := dev.o inode.o dir.o file.o

KDIR    := /lib/modules/$(shell uname -r)/build
PWD     := $(shell pwd)
VERBOSE = 0

all:
	$(MAKE) -C $(KDIR) M=$(PWD) KBUILD_VERBOSE=$(VERBOSE) modules

clean:
	rm -f *.o *.ko *.mod.c
