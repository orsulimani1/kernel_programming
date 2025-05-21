# Lab 1: Exploring Process Management and Kernel Subsystems

This lab will help you understand the Linux kernel process management subsystem through hands-on exploration and module development.

## Overview

In this lab, you will:
1. Explore the Linux process management subsystem
2. Examine task_struct contents and relationships
3. Implement modules to interact with the process subsystem
4. Test different scheduling policies

## Files in this Lab

- `process_info.c`: Kernel module that displays all running processes
- `task_monitor.c`: Kernel module to monitor a specific process in detail
- `sched_test.c`: User-space program to set scheduler types and priorities
- `Makefile`: Build file for all components

## Prerequisites

- Linux kernel source code (Linux 5.15 recommended)
- Root access or sudo privileges

## Building the Lab Components

To build all components:

```bash
# Set the kernel source path
export KERNEL_SRC=~/course/kernel_programming/linux

# Clear any cross-compiler settings and build with correct compiler
unset CROSS_COMPILE
make clean
make all ARCH=x86_64 CC=x86_64-linux-gnu-gcc
```

This will build:
- Kernel modules: process_info.ko and task_monitor.ko
- User program: sched_test (statically linked)

## Running the Lab in QEMU

### Step 1: Adding modules to initramfs

Since we're not using 9P filesystem passthrough, we need to add the modules to the initramfs:

```bash
# On host system
mkdir -p /tmp/initramfs_new/lab1
cp *.ko sched_test /tmp/initramfs_new/lab1/

# Extract existing initramfs
cd /tmp/initramfs_new
export REPO_BASELINE=~/course/kernel_programming
gunzip -c ${REPO_BASELINE}/initramfs.cpio.gz | cpio -id

# Add dynamic linker and libc for C programs to work
mkdir -p /tmp/initramfs_new/lib64
cp /lib64/ld-linux-x86-64.so.2 /tmp/initramfs_new/lib64/

mkdir -p /tmp/initramfs_new/lib/x86_64-linux-gnu
cp /lib/x86_64-linux-gnu/libc.so.6 /tmp/initramfs_new/lib/x86_64-linux-gnu/

# Repack the initramfs
cd /tmp/initramfs_new
find . -print0 | cpio --null -ov --format=newc | gzip -9 > \
  ${REPO_BASELINE}/initramfs_lab1.cpio.gz

cd ${REPO_BASELINE}
# Boot QEMU with the new initramfs
qemu-system-x86_64 \
  -kernel ${REPO_BASELINE}/linux/arch/x86/boot/bzImage \
  -initrd ${REPO_BASELINE}/initramfs_lab1.cpio.gz \
  -append "console=ttyS0 nokaslr" \
  -nographic \
  -m 512M
```

### Step 2: Inside QEMU, navigate to the lab directory

```bash
# Inside QEMU
cd /lab1
```

### Step 3: Load the process_info Module

```bash
insmod process_info.ko
```

This will:
- Create a `/proc/process_info` file
- Print process information to kernel log (dmesg)

View the process information:
```bash
cat /proc/process_info
dmesg | tail
```

### Step 4: Monitor a Specific Process

```bash
# Monitor the init process (PID 1)
insmod task_monitor.ko pid=1

# Or monitor another process
insmod task_monitor.ko pid=<target_pid>
```

View the detailed information:
```bash
dmesg | tail
```

### Step 5: Test Scheduling Policies

```bash
# Set normal scheduling policy
./sched_test SCHED_OTHER 0

# Set real-time FIFO policy
./sched_test SCHED_FIFO 99

# Set real-time round-robin policy
./sched_test SCHED_RR 50
```

## Lab Exercises

### Exercise 1: Understanding Process State
1. Examine the state of various processes using `/proc/process_info`
2. Identify processes in different states (running, sleeping, etc.)
3. Use `task_monitor.ko` on processes in different states to see detailed information
4. Answer: What causes processes to transition between states?

### Exercise 2: Process Hierarchy
1. Use `task_monitor.ko` to examine the init process (PID 1)
2. Identify its child processes
3. Draw a partial process tree showing parent-child relationships
4. Answer: How are orphaned processes handled in Linux?

### Exercise 3: Scheduling Policy Comparison
1. Run the `sched_test` program with different scheduling policies
2. Compare the count of iterations when run with SCHED_OTHER vs. SCHED_FIFO
3. Observe the behavior with different priorities
4. Answer: What differences do you observe between the scheduling policies?

## Assignments

1. **Modify process_info.ko**:
   - Add additional information such as CPU usage or memory usage
   - Sort processes by priority or state

2. **Extend task_monitor.ko**:
   - Add tracking of file descriptors
   - Track CPU time used by the process

3. **Create a New Module**:
   - Implement a module that allows changing process priority from userspace
   - Create a `/proc` interface for this functionality


## Troubleshooting

### Module Loading Errors
If you encounter "unknown symbol" errors when loading modules, make sure:
1. You're building against the exact same kernel version running in QEMU
2. The kernel config for your build matches the running kernel

### State Field in task_struct
In newer kernel versions, the state field has been renamed to __state. Our code has been updated to use this field.

## References

- Linux Kernel Documentation: https://www.kernel.org/doc/html/latest/
- Linux Kernel Source: https://github.com/torvalds/linux
- Linux Process States: https://man7.org/linux/man-pages/man1/ps.1.html
