obj-m := hello_module.o

KERNEL_DIR := ../linux

PWD := $(shell pwd)
all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

# Clean target
clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean

# Install target (optional)
install:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules_install