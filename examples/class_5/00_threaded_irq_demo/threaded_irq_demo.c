// advanced_threaded_irq_demo.c
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/atomic.h>
#include <linux/ktime.h>
#include <linux/mutex.h>
#include <linux/slab.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kernel Programming Course");
MODULE_DESCRIPTION("Advanced Threaded IRQ with Context Demonstration");
MODULE_VERSION("1.0");

#define SHARED_IRQ 9
static int irq = SHARED_IRQ, my_dev_id, irq_counter = 0;
module_param(irq, int, S_IRUGO);

// Statistics and shared data
static atomic_t primary_handler_count = ATOMIC_INIT(0);
static atomic_t thread_handler_count = ATOMIC_INIT(0);
static struct mutex shared_data_mutex;
static char shared_message[256];
static int shared_counter = 0;

// Proc interface
static struct proc_dir_entry *proc_entry;

void print_context(void)
{
    if (in_interrupt()) {
        pr_info("Code is running in interrupt context\n");
    } else {
        pr_info("Code is running in process context\n");
    }
}

/* Primary (hard) IRQ handler - minimal processing */
static irqreturn_t my_interrupt(int irq, void *dev_id)
{
    pr_info("%s - IRQ %d triggered\n", __func__, irq);
    print_context();
    
    atomic_inc(&primary_handler_count);
    irq_counter++;
    
    // In primary handler: only essential work
    // - Acknowledge hardware
    // - Save critical data
    // - Schedule thread handler
    
    pr_info("Primary: IRQ acknowledged, scheduling thread handler\n");
    
    return IRQ_WAKE_THREAD;
}

/* Threaded IRQ handler - complex processing */
static irqreturn_t my_threaded_interrupt(int irq, void *dev_id)
{
    int local_counter;
    char *buffer;
    
    pr_info("%s - Thread handler processing IRQ %d\n", __func__, irq);
    print_context();
    
    atomic_inc(&thread_handler_count);
    
    // Show stack trace to demonstrate we're in thread context
    pr_info("Thread context stack trace:\n");
    dump_stack();
    
    // Demonstrate thread context capabilities:
    
    // 1. Can use mutexes (sleeping locks)
    mutex_lock(&shared_data_mutex);
    shared_counter++;
    local_counter = shared_counter;
    snprintf(shared_message, sizeof(shared_message),
             "Processed by thread handler #%d", local_counter);
    mutex_unlock(&shared_data_mutex);
    
    pr_info("Thread: Updated shared data (counter: %d)\n", local_counter);
    
    // 2. Can allocate memory with GFP_KERNEL (can sleep)
    buffer = kmalloc(512, GFP_KERNEL);
    if (buffer) {
        snprintf(buffer, 512, "Thread handler %d allocated memory", local_counter);
        pr_info("Thread: %s\n", buffer);
        kfree(buffer);
    }
    
    // 3. Can sleep (this would CRASH in primary handler!)
    pr_info("Thread: Going to sleep for 50ms...\n");
    msleep(50);
    pr_info("Thread: Woke up from sleep\n");
    
    // 4. Can perform complex processing
    int i, result = 0;
    for (i = 0; i < 10000; i++) {
        result += i;
        if (i % 1000 == 0) {
            cond_resched();  // Voluntary preemption
        }
    }
    pr_info("Thread: Complex calculation result: %d\n", result);
    
    pr_info("Thread handler completed for IRQ %d\n", irq);
    
    return IRQ_HANDLED;
}

/* Proc interface */
static int advanced_threaded_irq_proc_show(struct seq_file *m, void *v)
{
    seq_printf(m, "echo 1 > /proc/threaded_test  # Primary only\n");
    seq_printf(m, "echo 2 > /proc/threaded_test  # Full threaded\n");
    
    seq_printf(m, "Advanced Threaded IRQ Demo\n");
    seq_printf(m, "==========================\n");
    seq_printf(m, "IRQ Number: %d\n", irq);
    seq_printf(m, "Total interrupts: %d\n", irq_counter);
    seq_printf(m, "Primary handler executions: %d\n", atomic_read(&primary_handler_count));
    seq_printf(m, "Thread handler executions: %d\n", atomic_read(&thread_handler_count));
    
    mutex_lock(&shared_data_mutex);
    seq_printf(m, "\nShared Data (protected by mutex):\n");
    seq_printf(m, "Shared counter: %d\n", shared_counter);
    seq_printf(m, "Last message: %s\n", shared_message);
    mutex_unlock(&shared_data_mutex);
    
    seq_printf(m, "\nContext Capabilities Demonstrated:\n");
    seq_printf(m, " Primary handler: Fast, minimal processing\n");
    seq_printf(m, " Thread handler: Can sleep, use mutexes, allocate memory\n");
    seq_printf(m, " Stack traces show different execution contexts\n");
    seq_printf(m, " Shared data protection with mutexes\n");
    seq_printf(m, " Complex processing without blocking interrupts\n");
    
    return 0;
}

static ssize_t advanced_threaded_irq_proc_write(struct file *file, 
                                               const char __user *buffer,
                                               size_t count, loff_t *pos)
{
    char input[8];
    int value;
    
    if (count >= sizeof(input))
        return -EINVAL;
        
    if (copy_from_user(input, buffer, count))
        return -EFAULT;
        
    input[count] = '\0';
    
    if (kstrtoint(input, 10, &value) != 0)
        return -EINVAL;
    
    switch (value) {
    case 1:
        pr_info("USER: Manually triggering primary handler only\n");
        my_interrupt(irq, &my_dev_id);
        break;
    case 2:
        pr_info("USER: Manually triggering full threaded IRQ\n");
        if (my_interrupt(irq, &my_dev_id) == IRQ_WAKE_THREAD) {
            my_threaded_interrupt(irq, &my_dev_id);
        }
        break;
    default:
        pr_info("USER: Invalid value %d (use 1 or 2)\n", value);
        return -EINVAL;
    }
    
    return count;
}

static int advanced_threaded_irq_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, advanced_threaded_irq_proc_show, NULL);
}

static const struct proc_ops advanced_threaded_irq_proc_ops = {
    .proc_open = advanced_threaded_irq_proc_open,
    .proc_read = seq_read,
    .proc_write = advanced_threaded_irq_proc_write,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static int __init my_init(void)
{
    int ret;
    
    pr_info("Advanced Threaded IRQ Demo: Loading module\n");
    
    // Initialize mutex and shared data
    mutex_init(&shared_data_mutex);
    strcpy(shared_message, "Initial message");
    
    ret = request_threaded_irq(irq, 
                              my_interrupt, my_threaded_interrupt,
                              IRQF_SHARED, "my_interrupt", &my_dev_id);
    
    if (ret != 0) {
        pr_err("Failed to reserve irq %d, ret:%d\n", irq, ret);
        return -1;
    }
    
    // Create proc interface
    proc_entry = proc_create("advanced_threaded_irq_demo", 0666, NULL, &advanced_threaded_irq_proc_ops);
    if (!proc_entry) {
        pr_err("Failed to create proc entry\n");
        free_irq(irq, &my_dev_id);
        return -ENOMEM;
    }
    
    pr_info("Successfully loaded ISR handler for IRQ %d\n", irq);
    pr_info("Monitor with: cat /proc/advanced_threaded_irq_demo\n");
    pr_info("Trigger interrupts to see threaded IRQ capabilities\n");
    
    return 0;
}

static void __exit my_exit(void)
{
    pr_info("Advanced Threaded IRQ Demo: Unloading module\n");
    
    // Remove proc interface
    proc_remove(proc_entry);
    
    // Synchronize and free IRQ
    synchronize_irq(irq);
    free_irq(irq, &my_dev_id);
    
    pr_info("Successfully unloaded, irq_counter = %d\n", irq_counter);
    pr_info("Final shared counter: %d\n", shared_counter);
    pr_info("Primary handlers: %d, Thread handlers: %d\n",
            atomic_read(&primary_handler_count), atomic_read(&thread_handler_count));
}

module_init(my_init);
module_exit(my_exit);