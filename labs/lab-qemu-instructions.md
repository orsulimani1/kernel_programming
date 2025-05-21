# Adding Lab Examples to QEMU for Demonstrations

This guide explains how to add lab examples to the QEMU environment for demonstrations during class.

## Method 1: Add to Initramfs

### 1. Copy lab files to initramfs directory

```bash
# Assuming you're in your kernel_programming directory
mkdir -p /tmp/initramfs_new/lab1
cp labs/lab1/* /tmp/initramfs_new/lab1/

# Copy the current initramfs content
cd /tmp/initramfs_new
gunzip -c /path/to/kernel_programming/initramfs.cpio.gz | cpio -id
```

### 2. Add build tools to initramfs

```bash
# Add kernel headers and build tools if needed
mkdir -p /tmp/initramfs_new/lib/modules/$(make -C /path/to/linux kernelrelease)
cp -r /path/to/linux/include /tmp/initramfs_new/
cp -r /path/to/linux/Module.symvers /tmp/initramfs_new/
```

### 3. Repack the initramfs

```bash
cd /tmp/initramfs_new
find . -print0 | cpio --null -ov --format=newc | gzip -9 > \
  /path/to/kernel_programming/initramfs_lab1.cpio.gz
```

### 4. Boot QEMU with the new initramfs

```bash
qemu-system-x86_64 \
  -kernel /path/to/linux/arch/x86/boot/bzImage \
  -initrd /path/to/kernel_programming/initramfs_lab1.cpio.gz \
  -append "console=ttyS0 nokaslr" \
  -nographic \
  -m 512M
```

## Building and Running Lab Examples in QEMU

### 1. Compile the kernel modules

```bash
# Inside QEMU after mounting the shared directory
cd /mnt/lab1
make
```

### 2. Load and test the modules

```bash
# Load the process_info module
insmod process_info.ko

# Check the module output
dmesg | tail

# Test the module functionality
cat /proc/process_info

# Load the task monitor module
insmod task_monitor.ko pid=1

# Unload modules when done
rmmod task_monitor
rmmod process_info
```

### 3. Real-time demonstration examples

```bash
# Show process creation
./process_spawn 5 &
cat /proc/process_info

# Test different scheduling policies
./sched_test SCHED_FIFO 99
./sched_test SCHED_RR 50
./sched_test SCHED_OTHER 0
```

## Tips for Effective Demonstrations

1. **Prepare scripts in advance**: Create shell scripts that automate the demonstration steps for smoother presentations.

2. **Use tmux or screen**: Split the terminal into multiple panes to show different aspects simultaneously:
   ```bash
   # Install tmux in your initramfs if needed
   # Inside QEMU
   tmux
   # Ctrl+b % to split vertically
   # Ctrl+b " to split horizontally
   ```

3. **Monitor kernel logs in real-time**:
   ```bash
   # In one tmux pane
   dmesg -w
   ```

4. **Prepare simplified versions** of commands for quick demonstrations:
   ```bash
   # Create an alias
   alias load_demo='insmod process_info.ko && insmod task_monitor.ko pid=1'
   alias unload_demo='rmmod task_monitor && rmmod process_info'
   ```

5. **Have fallback options**: Prepare pre-built modules in case compilation issues arise during the demonstration.

## Troubleshooting Common Issues

### Module compilation fails inside QEMU

Ensure you have the correct kernel headers and build tools in your initramfs:
```bash
# On host, prepare a more complete build environment
mkdir -p /tmp/kernel_build_env
cp -r /path/to/linux/. /tmp/kernel_build_env
# Copy to initramfs
cp -r /tmp/kernel_build_env /tmp/initramfs_new/usr/src/linux
```

### "Invalid module format" when loading modules

This usually means the module was built for a different kernel version:
```bash
# Check kernel version inside QEMU
uname -r
# Make sure this matches the version you built the module against
```
