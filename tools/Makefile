obj-m += fsf.o
fsf-objs := ../build/callsyms.o ../build/free_space_finder.o

build:
	cd .. && make create_build_dir && cp tools/free_space_finder.c build
	cd ../build && make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	cd .. && rm -rf build && make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean