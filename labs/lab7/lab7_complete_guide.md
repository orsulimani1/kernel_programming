# Lab 7: VFS & Block I/O Layer
## Linux Kernel Programming for Embedded Systems

### Lab Overview

This lab provides hands-on experience with the Virtual Filesystem (VFS) layer, Block I/O subsystem, and Virtual Memory Areas (VMAs). You'll implement practical examples demonstrating how these kernel subsystems work together.

**Learning Objectives:**
- Implement VFS objects (superblock, inode, dentry, file)
- Create a RAM-based block device with bio handling
- Work with I/O vectors and scatter-gather operations
- Understand VMA operations for memory mapping
- Compare different I/O schedulers

**Duration:** 3-4 hours

---

## Prerequisites

**Required Environment:**
- Linux Kernel 5.15 source code
- x86_64 cross-compilation toolchain
- QEMU for testing
- Course repository with initramfs

**Verify Setup:**
```bash
ls ~/src/test/kernel_programming/linux
ls ~/src/test/kernel_programming/initramfs.cpio.gz
which x86_64-linux-gnu-gcc
```

---

## Lab Structure

This lab is organized into three main exercises:

1. **VFS Objects Implementation** - Custom filesystem demonstrating VFS operations
2. **Block Device with bio/I/O Vectors** - RAM-based block device with modern I/O handling
3. **VMA Memory Mapping** - Character device with custom memory mapping

Each exercise includes:
- Complete kernel module source code
- Userspace test program
- Makefile for building and testing
- QEMU integration

---

## Exercise 1: VFS Objects Implementation

### Objective
Build a minimal filesystem to understand VFS object relationships and operations.

### What You'll Learn
- How superblock, inode, dentry, and file objects work together
- VFS operation dispatch mechanism
- Filesystem registration and mounting
- File and directory operations implementation

### Implementation Details

The VFS example (`simple_vfs.c`) demonstrates:

**Superblock Operations:**
- `example_alloc_inode()` - Allocate custom inode structure
- `example_destroy_inode()` - Clean up inode memory
- `example_statfs()` - Report filesystem statistics

**Inode Operations:**
- `example_lookup()` - Resolve filenames to inodes
- `example_create()` - Create new files
- `example_unlink()` - Delete files

**File Operations:**
- `example_read()` - Read data from files
- `example_write()` - Write data to files
- `example_readdir()` - List directory contents

### Build and Test

```bash
cd lab7/vfs_example

# Build kernel module and userspace test
make clean
make

# Update initramfs
make update-initramfs

# Test in QEMU
make test-qemu
```

### Inside QEMU Environment

```bash
# Load VFS module
insmod /lib/modules/simple_vfs.ko

# Check filesystem registration
cat /proc/filesystems | grep example_vfs

# Manual testing
mkdir -p /tmp/example_vfs
mount -t example_vfs none /tmp/example_vfs

# Test file operations
echo "Hello VFS" > /tmp/example_vfs/testfile
cat /tmp/example_vfs/testfile
ls -la /tmp/example_vfs/

# Run automated test
/user_programs/test_vfs

# Cleanup
umount /tmp/example_vfs
rmmod simple_vfs
```

### Expected Output

```
=== VFS Filesystem Test ===
Checking if example_vfs is registered...
	example_vfs
Mounting example_vfs at /tmp/example_vfs...
Filesystem mounted successfully

Testing file operations:
Opening /tmp/example_vfs/testfile...
Writing data to file...
Wrote 21 bytes: Hello from userspace!
Reading data from file...
Read 21 bytes: Hello from userspace!

Testing file stat...
File size: 21 bytes
File mode: 0644
File inode: 1000

Testing directory listing...
Contents of /tmp/example_vfs:
  . (type: 4)
  .. (type: 4)
  testfile (type: 8)

Unmounting filesystem...
VFS test completed successfully
```

### Analysis Questions

1. **VFS Call Flow:** Trace what happens when you call `open("/tmp/example_vfs/testfile", O_RDWR)`
2. **Dentry Cache:** How does the dentry cache improve filesystem performance?
3. **Inode Allocation:** Why do we need custom inode allocation in our filesystem?
4. **Mount Process:** What kernel structures are created during filesystem mounting?

---

## Exercise 2: Block Device with bio and I/O Vectors

### Objective
Create a RAM-based block device demonstrating modern bio-based I/O and scatter-gather operations.

### What You'll Learn
- Block device registration and management
- bio structure usage and I/O vector handling
- Multi-queue block layer integration
- I/O scheduler interaction
- Request processing and completion

### Implementation Details

The block device example (`simple_block.c`) demonstrates:

**Device Management:**
- Block device registration with `register_blkdev()`
- Generic disk (`gendisk`) allocation and configuration
- Multi-queue tag set setup

**bio Processing:**
- `simple_handle_bio()` - Process individual bio requests
- `bio_for_each_segment()` - Iterate through I/O vectors
- Scatter-gather data transfer

**Request Handling:**
- `simple_queue_rq()` - Multi-queue request processing
- Request completion with proper status codes

### Build and Test

```bash
cd lab7/block_example

# Build and test
make clean
make
make update-initramfs
make test-qemu
```

### Inside QEMU Environment

```bash
# Load block device module
insmod /lib/modules/simple_block.ko

# Check device creation
ls -l /dev/simple_block
cat /proc/partitions | grep simple_block

# Test I/O operations
/user_programs/test_block

# Manual testing with dd
echo "Test data" | dd of=/dev/simple_block bs=512 count=1
dd if=/dev/simple_block bs=512 count=1

# Check different I/O schedulers
cat /sys/block/simple_block/queue/scheduler
echo deadline > /sys/block/simple_block/queue/scheduler

# Performance testing
dd if=/dev/zero of=/dev/simple_block bs=4096 count=100
dd if=/dev/simple_block of=/dev/null bs=512 count=200

# View kernel logs
dmesg | tail -20

# Cleanup
rmmod simple_block
```

### Expected Output

```
=== Block Device Test ===
Opening block device /dev/simple_block...
Block device opened successfully
Device size: 1048576 bytes (2048 sectors)

Testing block I/O operations:
Writing to sector 0...
Wrote 512 bytes to block device
Reading from sector 0...
Read 512 bytes from block device
Data: This is test data for our simple block device!
✓ Data verification successful

Testing multiple sector writes...
Wrote to sector 1: Sector 1 data
Wrote to sector 2: Sector 2 data
Wrote to sector 3: Sector 3 data
Wrote to sector 4: Sector 4 data

Reading back multiple sectors...
Read from sector 1: Sector 1 data
Read from sector 2: Sector 2 data
Read from sector 3: Sector 3 data
Read from sector 4: Sector 4 data

Testing large I/O (4KB write)...
Large write successful: 4096 bytes

Block device test completed successfully
```

### Analysis Questions

1. **bio vs Buffer Heads:** What are the advantages of bio structures over buffer heads?
2. **I/O Vectors:** How do I/O vectors enable efficient scatter-gather operations?
3. **Multi-queue:** Why is multi-queue important for modern storage devices?
4. **I/O Schedulers:** How do different schedulers affect our block device performance?

---

## Exercise 3: VMA Memory Mapping

### Objective
Implement VMA operations for memory-mapped I/O with custom page fault handling.

### What You'll Learn
- VMA structure and operations
- Page fault handling mechanism
- Memory mapping implementation
- Interaction between VMAs and character devices

### Implementation Details

The VMA example (`simple_vma.c`) demonstrates:

**VMA Operations:**
- `example_vma_open()` - VMA initialization
- `example_vma_close()` - VMA cleanup
- `example_vma_fault()` - Custom page fault handling

**Character Device Interface:**
- Standard file operations (read, write, open, release)
- `example_char_mmap()` - Memory mapping implementation
- Integration with VMA system

**Memory Management:**
- `vmalloc()` allocation for demonstration
- `vmalloc_to_page()` for page fault resolution
- Proper VMA flag management

### Build and Test

```bash
cd lab7/vma_example

# Build and test
make clean
make
make update-initramfs
make test-qemu
```

### Inside QEMU Environment

```bash
# Load VMA module
insmod /lib/modules/simple_vma.ko

# Check device creation
ls -l /dev/simple_vma

# Test memory mapping
/user_programs/test_vma

# Monitor memory maps during test
cat /proc/devices | grep simple_vma

# Check VMA information in another terminal
# (while test is running)
cat /proc/`pgrep test_vma`/maps

# View kernel logs
dmesg | tail -20

# Cleanup
rmmod simple_vma
```

### Expected Output

```
=== VMA Memory Mapping Test ===
Opening device /dev/simple_vma...
Device opened successfully

Testing regular file operations:
Writing via write() system call...
Wrote 32 bytes: Hello from userspace via mmap!
Read 32 bytes: Hello from userspace via mmap!

Testing memory mapping:
Mapping device memory...
Memory mapped successfully at address: 0x7f1234567000
Initial content via mmap: Hello from kernel VMA example!
Writing via memory mapping...
Wrote: Data written via mmap - direct memory access!

Testing page faults across multiple pages:
Page 0: Page 0 content via mmap
Page 1: Page 1 content via mmap
Page 2: Page 2 content via mmap
Page 3: Page 3 content via mmap

Verifying via regular read:
Read back: Data written via mmap - direct memory access!

Testing large data write via mmap:
Large write successful: Large data block: ABCDEFGHIJKLMNOPQRSTUVWXYZ...

Cleaning up...
Memory unmapped successfully
VMA test completed successfully
```

### Analysis Questions

1. **VMA vs Regular I/O:** What are the performance benefits of memory mapping?
2. **Page Fault Handling:** How does our custom fault handler work?
3. **Memory Types:** Why do we use `vmalloc()` instead of `kmalloc()`?
4. **VMA Flags:** What do the different VMA flags control?

---

## Exercise 4: I/O Scheduler Analysis

### Objective
Compare different I/O schedulers with your block device to understand their behavior and performance characteristics.

### Scheduler Testing Script

Create a script to test different schedulers:

```bash
#!/bin/bash
# test_schedulers.sh

DEVICE="/dev/simple_block"
SCHEDULERS="mq-deadline none"

echo "I/O Scheduler Performance Comparison"
echo "==================================="

for sched in $SCHEDULERS; do
    echo "Testing $sched scheduler..."
    echo $sched > /sys/block/simple_block/queue/scheduler
    
    # Test sequential writes
    echo "  Sequential writes:"
    time dd if=/dev/zero of=$DEVICE bs=4096 count=100 2>/dev/null
    
    # Test random reads
    echo "  Random reads:"
    time for i in {1..50}; do
        dd if=$DEVICE of=/dev/null bs=512 skip=$((RANDOM % 1000)) count=1 2>/dev/null
    done
    
    echo "  --- $sched complete ---"
    echo
done
```

### Monitor I/O Statistics

```bash
# Check I/O stats before and after tests
cat /proc/diskstats | grep simple_block

# Monitor queue depth and requests
cat /sys/block/simple_block/queue/nr_requests
cat /sys/block/simple_block/stat

# Check scheduler parameters
ls /sys/block/simple_block/queue/iosched/
```

### Analysis Questions

1. **Scheduler Selection:** Which scheduler performs better for sequential vs random I/O?
2. **Queue Depth:** How does queue depth affect performance?
3. **Request Merging:** How can you observe request merging in action?

---

## Exercise 5: Integration and Advanced Testing

### Objective
Combine all components and practice advanced debugging techniques.

### Combined Testing

```bash
# Load all modules
insmod /lib/modules/simple_vfs.ko
insmod /lib/modules/simple_block.ko
insmod /lib/modules/simple_vma.ko

# Create filesystem on block device
mke2fs /dev/simple_block
mkdir /mnt/block_fs
mount /dev/simple_block /mnt/block_fs

# Test memory mapping with files on block device
echo "Testing integration" > /mnt/block_fs/test.txt
cat /mnt/block_fs/test.txt

# Cleanup
umount /mnt/block_fs
rmmod simple_vma simple_block simple_vfs
```

### Debugging Techniques

```bash
# Enable dynamic debug for all modules
echo 'module simple_vfs +p' > /sys/kernel/debug/dynamic_debug/control
echo 'module simple_block +p' > /sys/kernel/debug/dynamic_debug/control
echo 'module simple_vma +p' > /sys/kernel/debug/dynamic_debug/control

# Monitor in real-time
dmesg -w &

# Check memory usage
cat /proc/meminfo | grep -E "(MemFree|Cached|Buffers)"

# Monitor I/O
iostat 1 5
```

### Performance Analysis

```bash
# Monitor VMA usage
cat /proc/vmstat | grep -E "(pgfault|pgmajfault)"

# Check block device performance
echo 3 > /proc/sys/vm/drop_caches  # Clear caches
time dd if=/dev/simple_block of=/dev/null bs=1M count=1

# Monitor memory allocation
cat /proc/buddyinfo
cat /proc/slabinfo | grep -E "(kmalloc|dentry|inode)"
```

---

## Common Issues and Solutions

### VFS Issues

**Problem:** Mount fails with "unknown filesystem type"
```bash
# Solutions:
lsmod | grep simple_vfs
cat /proc/filesystems | grep example_vfs
# Make sure module loaded and filesystem registered
```

**Problem:** Kernel panic during directory operations
```bash
# Solution: Check inode initialization
# Ensure inode_init_once() called in alloc_inode
# Verify i_mapping->a_ops is set
```

### Block Device Issues

**Problem:** Device not appearing in /dev
```bash
# Solutions:
cat /proc/devices | grep simple_block
# Check major number registration
# Verify udev rules or create manually:
mknod /dev/simple_block b <major> 0
```

**Problem:** Kernel crash during I/O operations
```bash
# Solution: Check bio handling
# Don't call bio_endio() in request context
# Verify proper request completion
# Check sector bounds in transfer function
```

### VMA Issues

**Problem:** Segmentation fault during mmap
```bash
# Solutions:
# Check VMA size limits and alignment
ulimit -a
# Verify page fault handler
# Check vmalloc allocation success
```

**Problem:** Page faults not handled correctly
```bash
# Solution:
# Verify vmalloc_to_page() usage
# Check get_page() call
# Ensure proper vmf->page assignment
```

---

## Performance Benchmarks

### Expected Results

**VFS Operations:**
- File creation: ~1000 ops/second
- Directory listing: ~500 ops/second
- File read/write: Limited by buffer size

**Block Device:**
- Sequential throughput: ~100 MB/s (limited by RAM copy)
- Random IOPS: ~5000 IOPS (no seek time)
- Latency: <1ms per operation

**Memory Mapping:**
- Page fault latency: ~10-50 μs
- Memory bandwidth: ~1 GB/s (memory copy limited)
- Zero-copy efficiency: High for large transfers

---

## Lab Report Questions

1. **VFS Architecture:**
   - Explain the relationship between superblock, inode, dentry, and file objects
   - How does the VFS layer provide filesystem independence?
   - What happens during a `mount()` system call?

2. **Block I/O:**
   - Compare bio structures vs buffer heads for different use cases
   - Explain how I/O vectors enable scatter-gather operations
   - Why do SSDs and HDDs need different I/O schedulers?

3. **Memory Management:**
   - How do VMAs organize virtual address space?
   - What triggers a page fault and how is it resolved?
   - Compare memory mapping vs regular file I/O performance

4. **Integration:**
   - How would you implement a filesystem using your block device?
   - What are the trade-offs between different memory allocation strategies?
   - How do I/O schedulers interact with virtual memory management?

5. **Performance Analysis:**
   - Which I/O scheduler performed best for your workload and why?
   - How does request merging affect performance?
   - What factors limit the performance of your implementations?

---

## Extensions and Advanced Topics

### Optional Enhancements

1. **Enhanced VFS Features:**
   - Add symlink support
   - Implement hard links
   - Add extended attributes
   - Persistent storage backend

2. **Advanced Block Device:**
   - Add write barriers
   - Implement discard/TRIM support
   - Add encryption support
   - Create partition support

3. **Memory Management:**
   - Implement read-ahead for memory mapping
   - Add copy-on-write semantics
   - Create shared memory regions
   - Add NUMA awareness

### Research Topics

1. Compare your implementations with real filesystems (ext4, XFS)
2. Analyze the impact of different page sizes on performance
3. Study how modern NVMe drives use multiple queues
4. Investigate how containers use VFS features

---

## Submission Requirements

**Deliverables:**
1. Working kernel modules for all three exercises
2. Test results and performance measurements
3. Answers to lab report questions
4. Code modifications demonstrating understanding

**Evaluation Criteria:**
- Functional correctness of implementations
- Understanding of kernel subsystem interactions
- Quality of performance analysis and debugging
- Depth of investigation and creative extensions

---

## Additional Resources

**Kernel Documentation:**
- `Documentation/filesystems/vfs.rst`
- `Documentation/block/`
- `Documentation/vm/`
- `Documentation/admin-guide/mm/`

**Source Code References:**
- `fs/` - VFS implementation
- `block/` - Block layer
- `mm/` - Memory management
- `include/linux/fs.h` - VFS structures
- `include/linux/bio.h` - Block I/O structures
- `include/linux/mm.h` - Memory management

**Debugging Tools:**
```bash
# Filesystem debugging
cat /proc/mounts
cat /proc/filesystems
debugfs /dev/simple_block

# Block device analysis
lsblk -f
blkid /dev/simple_block
blockdev --getbsz /dev/simple_block

# Memory debugging
cat /proc/meminfo
cat /proc/vmstat
echo m > /proc/sysrq-trigger  # Memory usage
```

**Performance Monitoring:**
```bash
# I/O monitoring
iotop -o
iostat -x 1
watch 'cat /proc/diskstats'

# Memory monitoring
vmstat 1
free -h
watch 'cat /proc/meminfo'

# VFS monitoring
watch 'cat /proc/sys/fs/dentry-state'
watch 'cat /proc/sys/fs/inode-state'
```