obj-m += sample_synchronize_rcu.o
obj-m += _sample.o
obj-m += sample_call_rcu.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=`pwd`

clean:
	make -C /lib/modules/$(shell uname -r)/build M=`pwd` clean

