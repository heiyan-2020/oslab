include $(AM_HOME)/scripts/isa/x86_64.mk
include $(AM_HOME)/scripts/platform/qemu.mk

AM_SRCS := x86/qemu/start64.S \
           x86/qemu/trap64.S \
           x86/qemu/trm.c \
           x86/qemu/cte.c \
           x86/qemu/ioe.c \
           x86/qemu/vme.c \
           x86/qemu/mpe.c

# QEMU = /home/heiyan/qemu/bin/debug/native/qemu-system-x86_64
QEMU = qemu-system-x86_64

run: build-arg
	@$(QEMU) $(QEMU_FLAGS)
