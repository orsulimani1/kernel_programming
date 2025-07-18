CC = x86_64-linux-gnu-gcc
export CROSS_COMPILE=x86_64-linux-gnu-

KERNEL_SRC=~/course/kernel_programming/linux
KDIR ?= $(KERNEL_SRC)
INSTALL_PATH ?= /tmp/initramfs_new
INITRAMFS_ORIG=~/course/kernel_programming/initramfs.cpio.gz


obj-m += mem_allocator_demo.o

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

install:
	mkdir -p $(INSTALL_PATH)/kernel_modules
	cp *.ko $(INSTALL_PATH)/kernel_modules/ 2>/dev/null || true

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
	insmod mem_allocator_demo.ko

	# View initial results
	cat /proc/mem_allocator_demo

	# Run specific tests
	echo 'test' > /proc/mem_allocator_demo    # Performance tests
	echo 'frag' > /proc/mem_allocator_demo    # Fragmentation test
	echo 'fail' > /proc/mem_allocator_demo    # Failure scenarios

	# Check results
	cat /proc/mem_allocator_demo
	@echo " test complete"

.PHONY: all clean install test

