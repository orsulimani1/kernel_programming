# Kernel Loadable Modules - Quick Start Guide

## Why Kernel Modules?

Kernel modules are pieces of code that can be loaded and unloaded into the kernel on demand, without rebooting the system. They provide several key advantages:

- **Dynamic Loading**: Add functionality without kernel recompilation
- **Memory Efficiency**: Only load drivers/features when needed
- **Development Speed**: Test kernel code without rebooting
- **Modularity**: Keep kernel core small and focused

Think of modules as plugins for the kernel - they extend functionality while running in kernel space with full hardware access.

## Setting Up Your Environment

In your existing kernel programming environment, navigate to your working directory:

```bash
cd kernel_modules
```

## Writing Your First Kernel Module

Create a simple kernel module file called `hello_module.c`:

```c
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

// Module initialization function
static int __init hello_init(void)
{
    pr_info("Hello from kernel module!\n");
    pr_info("Module loaded successfully\n");
    return 0;  // Return 0 for success
}

// Module cleanup function
static void __exit hello_exit(void)
{
    pr_info("Goodbye from kernel module!\n");
}

// Register init and exit functions
module_init(hello_init);
module_exit(hello_exit);

// Module information
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A simple hello world kernel module");
MODULE_VERSION("1.0");
```

### Key Components Explained:

- **`__init`**: Marks function for initialization - memory freed after loading
- **`__exit`**: Marks function for cleanup - only needed if module can be unloaded
- **`module_init()`**: Registers the initialization function
- **`module_exit()`**: Registers the cleanup function
- **`MODULE_LICENSE("GPL")`**: Required - specifies module license

## Understanding pr_info()

`pr_info()` is the kernel's logging mechanism, similar to `printf()` in user space:

- **Purpose**: Print informational messages to kernel log
- **Output**: Messages appear in `dmesg` output
- **Format**: Same printf-style formatting
- **Levels**: `pr_debug()`, `pr_info()`, `pr_warn()`, `pr_err()`

Why not `printf()`? User space functions don't exist in kernel space - the kernel has its own set of functions for I/O operations.

## Creating the Makefile

Create a `Makefile` to build your module against your specific kernel:

```makefile
# Kernel module name (without .c extension)
obj-m := hello_module.o

# Path to your built kernel source
KERNEL_DIR := /path/to/your/kernel_programming/linux

# Current directory
PWD := $(shell pwd)

# Default target
all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

# Clean target
clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean

# Install target (optional)
install:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules_install
```

### Makefile Explanation:

- **`obj-m`**: Tells kernel build system to build this as a module
- **`KERNEL_DIR`**: Path to your compiled kernel source tree
- **`M=$(PWD)`**: Tells kernel build system where to find module source
- **`-C $(KERNEL_DIR)`**: Change to kernel directory for build context

## Building and Testing

1. **Build the module**:
```bash
make
```

2. **Start your QEMU environment** (from your main kernel_programming directory):
```bash
qemu-system-x86_64 \
  -kernel linux/arch/x86/boot/bzImage \
  -initrd initramfs.cpio.gz \
  -append "console=ttyS0 nokaslr" \
  -nographic \
  -m 512M
```

3. **Copy module to your initramfs** (before booting, add to your initramfs creation):
```bash
# Copy your .ko file to the initramfs directory before packaging
mkdir -p /tmp/initramfs_new/kernel_modules
cp hello_module.ko /tmp/initramfs_new/kernel_modules/
```

4. **Copy module to your initramfs** (before booting, add to your initramfs creation):
```bash
gunzip -c /path/to/kernel_programming/initramfs.cpio.gz | cpio -id
find . -print0 | cpio --null -ov --format=newc | gzip -9 >   ~/course/kernel_programming/initramfs_module.cpio.gz4.
```

5. **Load the module in QEMU**:
```bash
# Inside your QEMU environment
insmod hello_module.ko
```

6. **Check kernel messages**:
```bash
dmesg | tail
```

7. **Unload the module**:
```bash
rmmod hello_module
dmesg | tail  # Check exit message
```

## Connecting to Your Specific Kernel

The Makefile connects to your specific kernel through:

1. **`KERNEL_DIR`**: Points to your compiled kernel source
2. **Module versioning**: Kernel checks module was built for same kernel version
3. **Symbol compatibility**: Module symbols must match kernel's exported symbols

Make sure your `KERNEL_DIR` path points to the same Linux source you used to build your bzImage.

## Quick Commands Reference

```bash
# Build module
make

# Load module
insmod hello_module.ko

# List loaded modules
lsmod

# Get module info
modinfo hello_module.ko

# Remove module
rmmod hello_module

# View kernel messages
dmesg
```
