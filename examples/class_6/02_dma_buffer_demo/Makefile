CC = x86_64-linux-gnu-gcc
export CROSS_COMPILE=x86_64-linux-gnu-

KERNEL_SRC=~/course/kernel_programming/linux
KDIR ?= $(KERNEL_SRC)
INSTALL_PATH ?= /tmp/initramfs_new
INITRAMFS_ORIG=~/course/kernel_programming/initramfs.cpio.gz
CFLAGS = -m32 -Wall -Wextra -g -std=c11-std=gnu11


obj-m += shared_memory_demo.o

# User space program
USER_PROG = shared_mem_test

all: modules $(USER_PROG)

modules:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

$(USER_PROG):
	$(CC) -o $(USER_PROG) $(USER_PROG).c


clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean


install:
	mkdir -p $(INSTALL_PATH)/kernel_modules $(INSTALL_PATH)/user_programs
	cp *.ko $(INSTALL_PATH)/kernel_modules/ 2>/dev/null || true
	mkdir -p  $(INSTALL_PATH)/lib64
	cp /lib64/ld-linux-x86-64.so.2 $(INSTALL_PATH)/lib64/
	mkdir -p $(INSTALL_PATH)/lib/x86_64-linux-gnu
	cp /lib/x86_64-linux-gnu/libc.so.6 $(INSTALL_PATH)/lib/x86_64-linux-gnu/
	cp $(USER_PROG) $(INSTALL_PATH)/user_programs/ 2>/dev/null || true


update-initramfs: install
	@echo "Extracting original initramfs..."
	mkdir -p $(INSTALL_PATH)
	cd $(INSTALL_PATH) && gunzip -c $(INITRAMFS_ORIG) | cpio -id
	
	@echo "Copying kernel modules..."
	mkdir -p $(INSTALL_PATH)/lib/modules
	cp -r $(INSTALL_PATH)/kernel_modules/* $(INSTALL_PATH)/lib/modules/ 2>/dev/null || true
	
	@echo "Creating new initramfs..."
	cd $(INSTALL_PATH) && find . -print0 | cpio --null -ov --format=newc | gzip -9 > ~/course/kernel_programming/initramfs_class6.cpio.gz
	
	@echo "New initramfs: ~/course/kernel_programming/initramfs_class6.cpio.gz"

# Test with QEMU
test-qemu:
	qemu-system-x86_64 \
		-kernel $(KDIR)/arch/x86/boot/bzImage \
		-initrd ~/course/kernel_programming/initramfs_class6.cpio.gz \
		-append "console=ttyS0 nokaslr" \
		-nographic \
		-m 512M

test:
	@echo "Testing ..."

	insmod timer_implementation.ko

	# Single message test
	./msg_test "Hello Kernel!"

	# Interactive mode
	./msg_test -i
	# Then type messages, 'quit' to exit

	# Check kernel logs
	dmesg | tail
	@echo " test complete"


.PHONY: all clean install test

