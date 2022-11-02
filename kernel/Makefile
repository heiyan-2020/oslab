NAME           := kernel
SRCS           := framework/main.c $(shell find -L ./src/ -name "*.c")
INC_PATH       := include/ framework/

export AM_HOME := $(PWD)/../abstract-machine
ifeq ($(ARCH),)
export ARCH    := x86_64-qemu
export MODULE := L1
endif

include $(AM_HOME)/Makefile
include ../Makefile.lab
image: git

test: git
	@gcc -g -Wall  $(shell find src/ -name "*.c")  \
		$(shell find debug/ -name "*.c") \
		-Iframework -Idebug -Iinclude -I$(AM_HOME)/am/inclue -I$(AM_HOME)/klib/include -DTEST -lpthread \
		-o build/test
	@build/test 0
qemu_d:
	qemu-system-x86_64 --serial mon:stdio -machine accel=tcg -smp "1" -drive format=raw,file=$(PWD)/build/kernel-x86_64-qemu -S  -s -no-reboot -no-shutdown
