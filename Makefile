ifeq ($(KERNELRELEASE),)  

KERNELDIR ?= /lib/modules/$(shell uname -r)/build 
PWD := $(shell pwd)

CC=gcc
CFLAGS= -std=c99 -O0 -ggdb -g -Werror -Wall -pedantic -fPIC

.PHONY: build clean  

all: build sneaky_process

sneaky_process: sneaky_process.c
	$(CC) $(CFLAGS) -o $@ $<

build:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules  

clean:
	rm -rf *.o *~ core .depend .*.cmd *.order *.symvers *.ko *.mod.c 
else  

$(info Building with KERNELRELEASE = ${KERNELRELEASE}) 
obj-m :=    sneaky_mod.o  

endif
