obj-m += pec.o
pec-objs := build/peoxy_execute_call.o

create_build_dir:
	mkdir build && cp -r src/. build && cp -r include/. build

build:
	make create_build_dir
	cd build && make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	rm -rf build && make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean