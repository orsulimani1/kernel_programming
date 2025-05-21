# Targets for individual programs
# sched_test: sched_test.c
# 	x86_64-linux-gnu-gcc  $(CFLAGS) -static -o $@ $# Makefile for Lab1 - Process Management
	

# For Linux Kernel Programming course

# Kernel module variables
obj-m += process_info.o
obj-m += task_monitor.o

# Kernel module build variables
# Path to the kernel source used by QEMU
KERNEL_SRC=~/course/kernel_programming/linux
KDIR ?= $(KERNEL_SRC)
PWD := $(shell pwd)

# User space programs
CC = gcc
CFLAGS = -Wall -Wextra
PROGRAMS = sched_test

# Default target
all: modules programs

# Build kernel modules
modules:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

# Build user space programs
# programs: $(PROGRAMS)

# Targets for individual programs
# sched_test: sched_test.c
# 	$(CC) $(CFLAGS) -o $@ $<

# Clean up
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f $(PROGRAMS) *.o

# Install module (requires root privileges)
install: modules
	insmod process_info.ko
	insmod task_monitor.ko pid=1

# Remove module (requires root privileges)
uninstall:
	rmmod task_monitor
	rmmod process_info

# Targets that aren't file names
.PHONY: all modules programs clean install uninstall