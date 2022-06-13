name:=pec1

obj-m += $(name).o
$(name)-objs := build/proxy_execute.o build/callsyms.o build/page_rw.o build/store.o build/program_args.o

create_build_dir:
	mkdir build && cp -r src/. build && cp -r include/. build

build:
	make create_build_dir
	cd build && make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	rm -rf build && make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean