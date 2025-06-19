# Lab 3: Kernel Communication and Synchronization

## Objective
Implement kernel-user space communication interfaces and synchronization primitives to understand concurrent kernel programming.

## Prerequisites
- Completed Lab 2
- Understanding of system calls and kernel modules
- Basic knowledge of concurrency concepts

## Part 1: Communication Interfaces

### Exercise 1: Basic procfs Interface

Create a simple procfs module:

```c
// 01_communication/procfs_basic.c
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>

#define PROC_NAME "lab3_basic"
#define MAX_SIZE 1024

static struct proc_dir_entry *proc_entry;
static char kernel_buffer[MAX_SIZE];

static ssize_t proc_read(struct file *file, char __user *buffer, 
                        size_t count, loff_t *pos)
{
    int len;
    
    if (*pos > 0)
        return 0;
        
    len = snprintf(kernel_buffer, MAX_SIZE, 
                   "Hello from procfs! PID: %d\n", current->pid);
    
    if (copy_to_user(buffer, kernel_buffer, len))
        return -EFAULT;
        
    *pos = len;
    return len;
}

static ssize_t proc_write(struct file *file, const char __user *buffer,
                         size_t count, loff_t *pos)
{
    if (count > MAX_SIZE - 1)
        return -EINVAL;
        
    if (copy_from_user(kernel_buffer, buffer, count))
        return -EFAULT;
        
    kernel_buffer[count] = '\0';
    pr_info("Received: %s", kernel_buffer);
    
    return count;
}

static const struct proc_ops proc_fops = {
    .proc_read = proc_read,
    .proc_write = proc_write,
};

static int __init procfs_init(void)
{
    proc_entry = proc_create(PROC_NAME, 0666, NULL, &proc_fops);
    if (!proc_entry)
        return -ENOMEM;
        
    pr_info("procfs module loaded: /proc/%s\n", PROC_NAME);
    return 0;
}

static void __exit procfs_exit(void)
{
    proc_remove(proc_entry);
    pr_info("procfs module unloaded\n");
}

module_init(procfs_init);
module_exit(procfs_exit);
MODULE_LICENSE("GPL");
```

### Exercise 2: sysfs Interface

Implement sysfs attributes:

```c
// 01_communication/sysfs_demo.c
#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>

static struct kobject *lab3_kobj;
static int lab3_value = 0;

static ssize_t value_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", lab3_value);
}

static ssize_t value_store(struct kobject *kobj, struct kobj_attribute *attr,
                          const char *buf, size_t count)
{
    int ret = kstrtoint(buf, 10, &lab3_value);
    if (ret < 0)
        return ret;
    return count;
}

static struct kobj_attribute value_attribute = __ATTR_RW(value);

static int __init sysfs_init(void)
{
    lab3_kobj = kobject_create_and_add("lab3_sysfs", kernel_kobj);
    if (!lab3_kobj)
        return -ENOMEM;
        
    if (sysfs_create_file(lab3_kobj, &value_attribute.attr)) {
        kobject_put(lab3_kobj);
        return -ENOMEM;
    }
    
    pr_info("sysfs interface: /sys/kernel/lab3_sysfs/\n");
    return 0;
}

static void __exit sysfs_exit(void)
{
    sysfs_remove_file(lab3_kobj, &value_attribute.attr);
    kobject_put(lab3_kobj);
}

module_init(sysfs_init);
module_exit(sysfs_exit);
MODULE_LICENSE("GPL");
```

### Exercise 3: Netlink Socket

Create bidirectional netlink communication:

```c
// 01_communication/netlink_demo/netlink_kernel.c
#include <linux/module.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>
#include <net/sock.h>

#define NETLINK_LAB3 31

struct lab3_msg {
    int type;
    int data;
    char text[64];
};

static struct sock *nl_sock = NULL;

static void netlink_recv_msg(struct sk_buff *skb)
{
    struct nlmsghdr *nlh = (struct nlmsghdr *)skb->data;
    struct lab3_msg *msg = (struct lab3_msg *)nlmsg_data(nlh);
    int pid = nlh->nlmsg_pid;
    
    pr_info("Received from PID %d: type=%d, data=%d, text='%s'\n",
            pid, msg->type, msg->data, msg->text);
    
    // Echo back with modified data
    msg->data += 100;
    strcpy(msg->text, "Response from kernel");
    
    struct sk_buff *skb_out = nlmsg_new(sizeof(struct lab3_msg), GFP_KERNEL);
    if (!skb_out) return;
    
    struct nlmsghdr *nlh_out = nlmsg_put(skb_out, 0, 0, NLMSG_DONE, sizeof(struct lab3_msg), 0);
    memcpy(nlmsg_data(nlh_out), msg, sizeof(struct lab3_msg));
    
    netlink_unicast(nl_sock, skb_out, pid, MSG_DONTWAIT);
}

static int __init netlink_init(void)
{
    struct netlink_kernel_cfg cfg = {
        .input = netlink_recv_msg,
    };
    
    nl_sock = netlink_kernel_create(&init_net, NETLINK_LAB3, &cfg);
    if (!nl_sock) {
        pr_err("Failed to create netlink socket\n");
        return -ENOMEM;
    }
    
    pr_info("Netlink module loaded (protocol %d)\n", NETLINK_LAB3);
    return 0;
}

static void __exit netlink_exit(void)
{
    if (nl_sock)
        netlink_kernel_release(nl_sock);
    pr_info("Netlink module unloaded\n");
}

module_init(netlink_init);
module_exit(netlink_exit);
MODULE_LICENSE("GPL");
```

User space test program:

```c
// 01_communication/netlink_demo/netlink_user.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/netlink.h>

#define NETLINK_LAB3 31

struct lab3_msg {
    int type;
    int data;
    char text[64];
};

int main()
{
    int sock_fd;
    struct sockaddr_nl src_addr, dest_addr;
    struct nlmsghdr *nlh = NULL;
    struct lab3_msg *msg;
    struct iovec iov;
    struct msghdr msg_hdr;
    
    sock_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_LAB3);
    if (sock_fd < 0) {
        perror("socket");
        return -1;
    }
    
    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.nl_family = AF_NETLINK;
    src_addr.nl_pid = getpid();
    
    bind(sock_fd, (struct sockaddr *)&src_addr, sizeof(src_addr));
    
    nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(sizeof(struct lab3_msg)));
    memset(nlh, 0, NLMSG_SPACE(sizeof(struct lab3_msg)));
    nlh->nlmsg_len = NLMSG_SPACE(sizeof(struct lab3_msg));
    nlh->nlmsg_pid = getpid();
    nlh->nlmsg_flags = 0;
    
    msg = (struct lab3_msg *)NLMSG_DATA(nlh);
    msg->type = 1;
    msg->data = 42;
    strcpy(msg->text, "Hello from userspace");
    
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.nl_family = AF_NETLINK;
    dest_addr.nl_pid = 0;
    
    iov.iov_base = (void *)nlh;
    iov.iov_len = nlh->nlmsg_len;
    msg_hdr.msg_name = (void *)&dest_addr;
    msg_hdr.msg_namelen = sizeof(dest_addr);
    msg_hdr.msg_iov = &iov;
    msg_hdr.msg_iovlen = 1;
    
    printf("Sending message to kernel...\n");
    sendmsg(sock_fd, &msg_hdr, 0);
    
    recvmsg(sock_fd, &msg_hdr, 0);
    msg = (struct lab3_msg *)NLMSG_DATA(nlh);
    printf("Received: type=%d, data=%d, text='%s'\n", 
           msg->type, msg->data, msg->text);
    
    close(sock_fd);
    free(nlh);
    return 0;
}
```

## Part 2: Synchronization Primitives

### Exercise 4: Atomic Operations

Comprehensive atomic operations demo:

```c
// 02_synchronization/atomic_demo.c
#include <linux/module.h>
#include <linux/atomic.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>

static atomic_t counter = ATOMIC_INIT(0);
static atomic_t operations = ATOMIC_INIT(0);
static unsigned long flags = 0;

#define FLAG_READY 0
#define FLAG_BUSY  1

static ssize_t atomic_write(struct file *file, const char __user *buffer,
                           size_t count, loff_t *pos)
{
    char cmd[16];
    int value;
    
    if (count > sizeof(cmd) - 1)
        return -EINVAL;
        
    if (copy_from_user(cmd, buffer, count))
        return -EFAULT;
        
    cmd[count] = '\0';
    
    atomic_inc(&operations);
    
    if (sscanf(cmd, "inc") == 1) {
        atomic_inc(&counter);
        set_bit(FLAG_BUSY, &flags);
    } else if (sscanf(cmd, "dec") == 1) {
        atomic_dec(&counter);
    } else if (sscanf(cmd, "add %d", &value) == 1) {
        atomic_add(value, &counter);
    } else if (sscanf(cmd, "set %d", &value) == 1) {
        atomic_set(&counter, value);
        clear_bit(FLAG_BUSY, &flags);
    }
    
    return count;
}

static ssize_t atomic_read(struct file *file, char __user *buffer,
                          size_t count, loff_t *pos)
{
    char output[256];
    int len;
    
    if (*pos > 0)
        return 0;
    
    len = snprintf(output, sizeof(output),
                   "Counter: %d\nOperations: %d\nFlags: 0x%lx\n"
                   "Ready: %s\nBusy: %s\n",
                   atomic_read(&counter),
                   atomic_read(&operations),
                   flags,
                   test_bit(FLAG_READY, &flags) ? "YES" : "NO",
                   test_bit(FLAG_BUSY, &flags) ? "YES" : "NO");
    
    if (copy_to_user(buffer, output, len))
        return -EFAULT;
        
    *pos = len;
    return len;
}

static const struct proc_ops atomic_fops = {
    .proc_read = atomic_read,
    .proc_write = atomic_write,
};

static int __init atomic_init(void)
{
    set_bit(FLAG_READY, &flags);
    
    if (!proc_create("lab3_atomic", 0666, NULL, &atomic_fops))
        return -ENOMEM;
        
    pr_info("Atomic demo loaded\n");
    return 0;
}

static void __exit atomic_exit(void)
{
    proc_remove_entry("lab3_atomic", NULL);
    pr_info("Final counter: %d, operations: %d\n",
            atomic_read(&counter), atomic_read(&operations));
}

module_init(atomic_init);
module_exit(atomic_exit);
MODULE_LICENSE("GPL");
```

### Exercise 5: Mutex and Spinlock Comparison

```c
// 02_synchronization/lock_comparison.c
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/time.h>

static DEFINE_MUTEX(test_mutex);
static DEFINE_SPINLOCK(test_spinlock);
static int shared_data = 0;
static struct proc_dir_entry *proc_mutex, *proc_spinlock;

static ssize_t mutex_test(struct file *file, char __user *buffer,
                         size_t count, loff_t *pos)
{
    char output[128];
    int len;
    ktime_t start, end;
    
    if (*pos > 0)
        return 0;
    
    start = ktime_get();
    
    if (mutex_lock_interruptible(&test_mutex))
        return -ERESTARTSYS;
        
    shared_data++;
    msleep(100);  // Simulate work
    
    mutex_unlock(&test_mutex);
    
    end = ktime_get();
    
    len = snprintf(output, sizeof(output),
                   "Mutex test: data=%d, time=%lld ns\n",
                   shared_data, ktime_to_ns(ktime_sub(end, start)));
    
    if (copy_to_user(buffer, output, len))
        return -EFAULT;
        
    *pos = len;
    return len;
}

static ssize_t spinlock_test(struct file *file, char __user *buffer,
                            size_t count, loff_t *pos)
{
    char output[128];
    int len;
    ktime_t start, end;
    unsigned long flags;
    
    if (*pos > 0)
        return 0;
    
    start = ktime_get();
    
    spin_lock_irqsave(&test_spinlock, flags);
    shared_data++;
    // No sleeping in spinlock!
    spin_unlock_irqrestore(&test_spinlock, flags);
    
    end = ktime_get();
    
    len = snprintf(output, sizeof(output),
                   "Spinlock test: data=%d, time=%lld ns\n",
                   shared_data, ktime_to_ns(ktime_sub(end, start)));
    
    if (copy_to_user(buffer, output, len))
        return -EFAULT;
        
    *pos = len;
    return len;
}

static const struct proc_ops mutex_fops = {
    .proc_read = mutex_test,
};

static const struct proc_ops spinlock_fops = {
    .proc_read = spinlock_test,
};

static int __init lock_init(void)
{
    proc_mutex = proc_create("lab3_mutex", 0444, NULL, &mutex_fops);
    proc_spinlock = proc_create("lab3_spinlock", 0444, NULL, &spinlock_fops);
    
    if (!proc_mutex || !proc_spinlock) {
        if (proc_mutex) proc_remove(proc_mutex);
        if (proc_spinlock) proc_remove(proc_spinlock);
        return -ENOMEM;
    }
    
    pr_info("Lock comparison loaded\n");
    return 0;
}

static void __exit lock_exit(void)
{
    proc_remove(proc_mutex);
    proc_remove(proc_spinlock);
    pr_info("Lock comparison unloaded\n");
}

module_init(lock_init);
module_exit(lock_exit);
MODULE_LICENSE("GPL");
```

### Exercise 6: Reader-Writer Lock

```c
// 02_synchronization/rwlock_demo.c
#include <linux/module.h>
#include <linux/rwlock.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/jiffies.h>

static DEFINE_RWLOCK(shared_rwlock);
static int shared_data = 0;
static unsigned long last_update = 0;
static struct proc_dir_entry *proc_reader, *proc_writer;

static ssize_t reader_proc(struct file *file, char __user *buffer,
                          size_t count, loff_t *pos)
{
    char output[128];
    int len, data;
    unsigned long update_time;
    
    if (*pos > 0)
        return 0;
    
    read_lock(&shared_rwlock);
    data = shared_data;
    update_time = last_update;
    msleep(500);  // Simulate read work
    read_unlock(&shared_rwlock);
    
    len = snprintf(output, sizeof(output),
                   "Read data: %d (updated at %lu)\n", data, update_time);
    
    if (copy_to_user(buffer, output, len))
        return -EFAULT;
        
    *pos = len;
    return len;
}

static ssize_t writer_proc(struct file *file, const char __user *buffer,
                          size_t count, loff_t *pos)
{
    char input[16];
    int new_value;
    
    if (count > sizeof(input) - 1)
        return -EINVAL;
        
    if (copy_from_user(input, buffer, count))
        return -EFAULT;
        
    input[count] = '\0';
    
    if (kstrtoint(input, 10, &new_value) != 0)
        return -EINVAL;
    
    write_lock(&shared_rwlock);
    shared_data = new_value;
    last_update = jiffies;
    msleep(1000);  // Simulate write work
    write_unlock(&shared_rwlock);
    
    pr_info("Updated shared_data to %d\n", new_value);
    return count;
}

static const struct proc_ops reader_fops = {
    .proc_read = reader_proc,
};

static const struct proc_ops writer_fops = {
    .proc_write = writer_proc,
};

static int __init rwlock_init(void)
{
    proc_reader = proc_create("lab3_reader", 0444, NULL, &reader_fops);
    proc_writer = proc_create("lab3_writer", 0222, NULL, &writer_fops);
    
    if (!proc_reader || !proc_writer) {
        if (proc_reader) proc_remove(proc_reader);
        if (proc_writer) proc_remove(proc_writer);
        return -ENOMEM;
    }
    
    last_update = jiffies;
    pr_info("RWLock demo loaded\n");
    return 0;
}

static void __exit rwlock_exit(void)
{
    proc_remove(proc_reader);
    proc_remove(proc_writer);
    pr_info("RWLock demo unloaded\n");
}

module_init(rwlock_init);
module_exit(rwlock_exit);
MODULE_LICENSE("GPL");
```

## Build and Test

### Directory Structure
```
labs/lab3/
├── Makefile
├── 01_communication/
│   ├── Makefile
│   ├── procfs_basic.c
│   ├── sysfs_demo.c
│   └── netlink_demo/
│       ├── Makefile
│       ├── netlink_kernel.c
│       └── netlink_user.c
└── 02_synchronization/
    ├── Makefile
    ├── atomic_demo.c
    ├── lock_comparison.c
    └── rwlock_demo.c
```

### Master Makefile
```makefile
# labs/lab3/Makefile
KERNEL_SRC=~/course/kernel_programming/linux
KDIR ?= $(KERNEL_SRC)
INSTALL_PATH ?= /tmp/initramfs_lab3

SUBDIRS = 01_communication 02_synchronization

all: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ KDIR=$(KDIR)

clean:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done

install: all
	mkdir -p $(INSTALL_PATH)/{kernel_modules,user_programs}
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir install INSTALL_PATH=$(INSTALL_PATH); \
	done

test-qemu: install
	./scripts/build_test_env.sh

.PHONY: all clean install test-qemu $(SUBDIRS)
```

### Communication Makefile
```makefile
# 01_communication/Makefile
KERNEL_SRC=~/course/kernel_programming/linux
KDIR ?= $(KERNEL_SRC)
INSTALL_PATH ?= /tmp/initramfs_lab3

obj-m += procfs_basic.o
obj-m += sysfs_demo.o

all: modules netlink

modules:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

netlink:
	$(MAKE) -C netlink_demo

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	$(MAKE) -C netlink_demo clean

install:
	mkdir -p $(INSTALL_PATH)/kernel_modules
	cp *.ko $(INSTALL_PATH)/kernel_modules/ 2>/dev/null || true
	$(MAKE) -C netlink_demo install INSTALL_PATH=$(INSTALL_PATH)

.PHONY: all modules netlink clean install
```

### Testing Commands

```bash
# Build all modules
make all

# Test procfs
echo "Hello Lab3" > /proc/lab3_basic
cat /proc/lab3_basic

# Test sysfs
echo 42 > /sys/kernel/lab3_sysfs/value
cat /sys/kernel/lab3_sysfs/value

# Test atomic operations
echo "inc" > /proc/lab3_atomic
echo "add 5" > /proc/lab3_atomic
cat /proc/lab3_atomic

# Test lock comparison
cat /proc/lab3_mutex &
cat /proc/lab3_spinlock &

# Test reader-writer lock
cat /proc/lab3_reader &
echo 100 > /proc/lab3_writer

# Test netlink
cd 01_communication/netlink_demo
./netlink_user
```

## Expected Output

**procfs test:**
```
$ cat /proc/lab3_basic
Hello from procfs! PID: 1234
```

**Atomic operations:**
```
$ cat /proc/lab3_atomic
Counter: 6
Operations: 3
Flags: 0x3
Ready: YES
Busy: YES
```

**Lock timing comparison:**
```
$ cat /proc/lab3_mutex
Mutex test: data=1, time=100023456 ns

$ cat /proc/lab3_spinlock  
Spinlock test: data=2, time=234 ns
```

## Common Issues

### Build Errors
- Check kernel version compatibility
- Verify all headers are included
- Ensure proper MODULE_LICENSE

### Runtime Errors  
- `ENOENT`: procfs/sysfs entry not found
- `EFAULT`: User space memory access error
- Deadlocks: Check lock ordering

### Debugging Tips
```bash
# Monitor kernel logs
dmesg -w

# Check loaded modules
lsmod | grep lab3

# Verify procfs entries
ls -la /proc/lab3_*

# Check sysfs entries  
find /sys -name "*lab3*"
```

## Bonus Challenges

1. Implement debugfs interface with custom file operations
2. Create netlink multicast communication
3. Implement RCU (Read-Copy-Update) example


## Resources

- Kernel source: `kernel/`, `fs/proc/`, `fs/sysfs/`
- Synchronization primitives: `include/linux/`