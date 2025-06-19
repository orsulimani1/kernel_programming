#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

static DEFINE_SPINLOCK(data_lock);
static struct proc_dir_entry *proc_entry;
static int shared_data = 0;
static unsigned long operation_count = 0;

static ssize_t spinlock_read(struct file *file, char __user *buffer,
                            size_t count, loff_t *pos)
{
    char *kernel_buffer;
    int len;
    int local_data;
    unsigned long local_count;
    
    if (*pos > 0)
        return 0;
        
    kernel_buffer = kmalloc(256, GFP_KERNEL);
    if (!kernel_buffer)
        return -ENOMEM;
    
    // Critical section - acquire lock
    spin_lock(&data_lock);
    local_data = shared_data;
    operation_count++;
    local_count = operation_count;
    spin_unlock(&data_lock);
    // End critical section
    
    len = snprintf(kernel_buffer, 256,
                   "Shared data: %d\nOperations: %lu\n",
                   local_data, local_count);
    
    if (copy_to_user(buffer, kernel_buffer, len)) {
        kfree(kernel_buffer);
        return -EFAULT;
    }
    
    kfree(kernel_buffer);
    *pos = len;
    return len;
}

static ssize_t spinlock_write(struct file *file, const char __user *buffer,
                             size_t count, loff_t *pos)
{
    char input[32];
    int value;
    
    if (count > sizeof(input) - 1)
        return -EINVAL;
        
    if (copy_from_user(input, buffer, count))
        return -EFAULT;
        
    input[count] = '\0';
    
    if (kstrtoint(input, 10, &value) != 0)
        return -EINVAL;
    
    // Critical section
    spin_lock(&data_lock);
    shared_data = value;
    operation_count++;
    pr_info("Updated shared_data to %d (operation #%lu)\n", 
            shared_data, operation_count);
    spin_unlock(&data_lock);
    
    return count;
}

static const struct proc_ops spinlock_fops = {
    .proc_read = spinlock_read,
    .proc_write = spinlock_write,
};

static int __init spinlock_init(void)
{
    proc_entry = proc_create("spinlock_demo", 0666, NULL, &spinlock_fops);
    if (!proc_entry)
        return -ENOMEM;
        
    pr_info("Spinlock demo module loaded\n");
    return 0;
}

static void __exit spinlock_exit(void)
{
    proc_remove(proc_entry);
    pr_info("Spinlock demo module unloaded\n");
}

module_init(spinlock_init);
module_exit(spinlock_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Spinlock demonstration");