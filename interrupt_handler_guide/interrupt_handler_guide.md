# Creating and Testing Interrupt Handlers

## Overview
Complete guide for creating, registering, and testing custom interrupt handlers in the Linux kernel.

## Handler Implementation

### Basic Handler Structure
```c
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/atomic.h>

static atomic_t irq_count = ATOMIC_INIT(0);

static irqreturn_t my_interrupt_handler(int irq, void *dev_id)
{
    atomic_inc(&irq_count);
    
    /* Quick hardware operations only */
    pr_info("IRQ %d handled, count: %d\n", irq, atomic_read(&irq_count));
    
    return IRQ_HANDLED;
}
```

### Registration and Cleanup
```c
#define DEVICE_IRQ 15

static int __init irq_demo_init(void)
{
    int result = request_irq(DEVICE_IRQ, my_interrupt_handler,
                           IRQF_SHARED, "my_device", &irq_count);
    if (result) {
        pr_err("Failed to request IRQ %d: %d\n", DEVICE_IRQ, result);
        return result;
    }
    return 0;
}

static void __exit irq_demo_exit(void)
{
    free_irq(DEVICE_IRQ, &irq_count);
    pr_info("Handler unregistered, total interrupts: %d\n", 
            atomic_read(&irq_count));
}
```

### Shared Handler Example
```c
struct my_device {
    int device_id;
    atomic_t count;
};

static irqreturn_t shared_handler(int irq, void *dev_id)
{
    struct my_device *dev = (struct my_device *)dev_id;
    
    /* Check if our device caused interrupt */
    if (dev->device_id < 1 || dev->device_id > 2)
        return IRQ_NONE;
    
    atomic_inc(&dev->count);
    return IRQ_HANDLED;
}
```

## Simulation and Testing

### Timer-Based Simulation
```c
#include <linux/timer.h>

static struct timer_list sim_timer;

static void timer_callback(struct timer_list *t)
{
    my_interrupt_handler(DEVICE_IRQ, &irq_count);
    mod_timer(&sim_timer, jiffies + msecs_to_jiffies(1000));
}

static int __init setup_simulation(void)
{
    timer_setup(&sim_timer, timer_callback, 0);
    mod_timer(&sim_timer, jiffies + msecs_to_jiffies(500));
    return 0;
}
```

### Proc Interface for Monitoring
```c
static int irq_show(struct seq_file *m, void *v)
{
    seq_printf(m, "Interrupt count: %d\n", atomic_read(&irq_count));
    seq_printf(m, "Last IRQ time: %lu\n", jiffies);
    return 0;
}

static const struct proc_ops irq_fops = {
    .proc_open = single_open,
    .proc_read = seq_read,
    .proc_release = single_release,
};
```

### Testing Commands
```bash
# Build and load
make
sudo insmod irq_demo.ko

# Monitor interrupts
watch -n 1 "cat /proc/irq_demo"
dmesg | tail -f

# Check system interrupts
cat /proc/interrupts | grep my_device

# Unload
sudo rmmod irq_demo
```

## Complete Example (irq_test.c)
```c
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/atomic.h>

MODULE_LICENSE("GPL");

#define TEST_IRQ 15
static atomic_t irq_count = ATOMIC_INIT(0);
static struct timer_list test_timer;
static struct proc_dir_entry *proc_entry;

static irqreturn_t test_handler(int irq, void *dev_id)
{
    atomic_inc(&irq_count);
    pr_info("Test IRQ %d: count=%d\n", irq, atomic_read(&irq_count));
    return IRQ_HANDLED;
}

static void timer_func(struct timer_list *t)
{
    test_handler(TEST_IRQ, NULL);
    mod_timer(&test_timer, jiffies + HZ);
}

static int proc_show(struct seq_file *m, void *v)
{
    seq_printf(m, "IRQ count: %d\n", atomic_read(&irq_count));
    return 0;
}

static int proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, proc_show, NULL);
}

static const struct proc_ops proc_fops = {
    .proc_open = proc_open,
    .proc_read = seq_read,
    .proc_release = single_release,
};

static int __init test_init(void)
{
    timer_setup(&test_timer, timer_func, 0);
    mod_timer(&test_timer, jiffies + HZ);
    
    proc_entry = proc_create("irq_test", 0444, NULL, &proc_fops);
    pr_info("IRQ test module loaded\n");
    return 0;
}

static void __exit test_exit(void)
{
    del_timer_sync(&test_timer);
    proc_remove(proc_entry);
    pr_info("IRQ test module unloaded: %d interrupts\n", 
            atomic_read(&irq_count));
}

module_init(test_init);
module_exit(test_exit);
```

## Do's and Don'ts

### DO's ✅
- **Use atomic operations**: `atomic_inc()`, `atomic_read()`
- **Use brief spinlocks**: `spin_lock()`, `spin_unlock()`
- **Check device ownership**: Return `IRQ_NONE` if not your interrupt
- **Use GFP_ATOMIC**: For memory allocation in interrupt context
- **Keep processing minimal**: Hardware acknowledgment, data saving
- **Return proper codes**: `IRQ_HANDLED` or `IRQ_NONE`

### DON'Ts ❌
- **Never sleep**: `msleep()`, `mutex_lock()`, `schedule()`
- **No user space access**: `copy_to_user()`, `copy_from_user()`
- **No blocking allocation**: `GFP_KERNEL` memory allocation
- **No long processing**: Complex algorithms, file I/O
- **No floating point**: FPU operations not allowed

## Shared vs Basic Handlers

### Basic Interrupt Handler
- **Exclusive IRQ line**: Only one device uses the IRQ
- **No device checking**: Assumes interrupt is always ours
- **Simpler logic**: Direct processing without validation

### Shared Interrupt Handler
- **Shared IRQ line**: Multiple devices share same IRQ
- **Device validation**: Must check if interrupt belongs to device
- **Proper return codes**: `IRQ_NONE` if not handled

```c
static irqreturn_t shared_handler(int irq, void *dev_id)
{
    struct my_device *dev = (struct my_device *)dev_id;
    
    /* Check if our device caused interrupt */
    if (!my_device_pending(dev))
        return IRQ_NONE;    /* Not our interrupt */
    
    /* Handle our device's interrupt */
    handle_my_device(dev);
    return IRQ_HANDLED;     /* We handled it */
}
```

## Makefile
```makefile
obj-m += irq_test.o

KDIR := ~/course/kernel_programming/linux

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

test:
	sudo insmod irq_test.ko
	cat /proc/irq_test
	sleep 5
	cat /proc/irq_test
	sudo rmmod irq_test
	dmesg | tail -10

.PHONY: all clean test
```