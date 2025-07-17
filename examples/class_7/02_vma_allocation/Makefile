CC = x86_64-linux-gnu-gcc
export CROSS_COMPILE=x86_64-linux-gnu-

KERNEL_SRC=~/src/test/kernel_programming/linux
KDIR ?= $(KERNEL_SRC)
INSTALL_PATH ?= /tmp/initramfs_new
INITRAMFS_ORIG=~/src/test/kernel_programming/initramfs.cpio.gz
CFLAGS = -m32 -Wall -Wextra -g -std=c11-std=gnu11


obj-m += vma_allocation.o

# User space program
USER_PROG = test_vma

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
	cd $(INSTALL_PATH) && find . -print0 | cpio --null -ov --format=newc | gzip -9 > ~/src/test/kernel_programming/initramfs_class7.cpio.gz
	
	@echo "New initramfs: ~/src/test/kernel_programming/initramfs_class7.cpio.gz"

# Test with QEMU
test-qemu:
	qemu-system-x86_64 \
		-kernel $(KDIR)/arch/x86/boot/bzImage \
		-initrd ~/src/test/kernel_programming/initramfs_class7.cpio.gz \
		-append "console=ttyS0 nokaslr" \
		-nographic \
		-m 512M

test:
	@echo "Testing VMA Memory Allocation..."

	# Load the VMA module
	insmod simple_vma.ko
	
	# Wait for device to be created
	sleep 1
	
	# Check device was created
	ls -l /dev/simple_vma
	
	# Run the test program
	./test_vma
	
	# Show memory maps during test (run in background)
	@echo "Device memory info:"
	cat /proc/devices | grep simple_vma || echo "Device not found"
	
	# Check kernel logs
	dmesg | tail -20
	
	# Remove module
	rmmod simple_vma
	
	@echo "VMA test complete"

.PHONY: all clean install test modules update-initramfs test-qemu