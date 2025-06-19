CC = x86_64-linux-gnu-gcc
export CROSS_COMPILE=x86_64-linux-gnu-

KERNEL_SRC=~/course/kernel_programming/linux
KDIR ?= $(KERNEL_SRC)
INSTALL_PATH ?= /tmp/initramfs_new
INITRAMFS_ORIG=~/course/kernel_programming/initramfs.cpio.gz
CFLAGS_atomic_demo.o += -std=gnu11


obj-m += atomic_demo.o

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
	cd $(INSTALL_PATH) && find . -print0 | cpio --null -ov --format=newc | gzip -9 > ~/course/kernel_programming/initramfs_class3.cpio.gz
	
	@echo "New initramfs: ~/course/kernel_programming/initramfs_class3.cpio.gz"

# Test with QEMU
test-qemu:
	qemu-system-x86_64 \
		-kernel $(KDIR)/arch/x86/boot/bzImage \
		-initrd ~/course/kernel_programming/initramfs_class3.cpio.gz \
		-append "console=ttyS0 nokaslr" \
		-nographic \
		-m 512M

test:
	@echo "Testing advanced procfs..."
	# Load module
	insmod atomic_comprehensive.ko

	# Test basic operations
	echo "inc" > /proc/atomic_demo/basic
	echo "add 5" > /proc/atomic_demo/basic
	echo "set 10" > /proc/atomic_demo/basic
	echo "xchg 20" > /proc/atomic_demo/basic
	echo "cmpxchg 25" > /proc/atomic_demo/basic
	cat /proc/atomic_demo/basic

	# Test advanced operations
	echo "dec_and_test" > /proc/atomic_demo/advanced
	echo "sub_and_test 5" > /proc/atomic_demo/advanced
	echo "add_negative -3" > /proc/atomic_demo/advanced
	echo "large_inc" > /proc/atomic_demo/advanced
	echo "large_add 1000" > /proc/atomic_demo/advanced
	echo "ref_get" > /proc/atomic_demo/advanced
	echo "ref_put" > /proc/atomic_demo/advanced
	cat /proc/atomic_demo/advanced

	# Test bit operations
	echo "set 0" > /proc/atomic_demo/bits      # Set SYSTEM_READY
	echo "set 1" > /proc/atomic_demo/bits      # Set HIGH_LOAD
	echo "test_and_set 2" > /proc/atomic_demo/bits  # Test and set MAINTENANCE
	echo "change 3" > /proc/atomic_demo/bits   # Toggle DEBUG_MODE
	cat /proc/atomic_demo/bits

	# Check overall status
	cat /proc/atomic_demo/status

	# Check kernel messages
	dmesg | tail -20
	@echo "Advanced procfs test complete"

.PHONY: all clean install test

