obj-m += kfetch_mod_312551129.o 
 
PWD := $(CURDIR) 
 
all: 
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules 
	cc kfetch.c -I ./ -o kfetch

load: 
	sudo insmod kfetch_mod_312551129.ko

unload:
	sudo rmmod kfetch_mod_312551129
run:
	sudo ./kfetch -h	
 
clean: 
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
