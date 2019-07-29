#
# Makefile for the linux filesystem, AEON.
#

obj-m += sample.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=`pwd`

clean:
	make -C /lib/modules/$(shell uname -r)/build M=`pwd` clean
