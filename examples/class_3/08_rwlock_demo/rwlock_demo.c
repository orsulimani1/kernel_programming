#include <linux/module.h>
#include <linux/rwlock.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/jiffies.h>

static DEFINE_RWLOCK(data_rwlock);
static struct proc_dir_entry *proc_read_entry, *proc_write_entry;
static int shared_data = 0;
static unsigned long last_update;

// Reader function
static ssize_t reader_read(struct file *file, char __user *buffer,
                          size_t count, loff_t *pos)
{
    char kernel_buffer[128];
    int len, local_data;
    unsigned long local_time;
    
    if (*pos > 0)
        return 0;
    
    pr_info("Reader %d acquiring read lock\n", current->pid);
    
    // Acquire read lock (multiple readers allowed)
    read_lock(&data_rwlock);
    
    local_data = shared_data;
    local_time = last_update;
    
    // Simulate read operation
    msleep(1000);
    
    read_unlock(&data_rwlock);
    
    pr_info("Reader %d released read lock\n", current->pid);
    
    len = snprintf(kernel_buffer, sizeof(kernel_buffer),
                   "Data: %d (updated at jiffies: %lu)\n",
                   local_data, local_time);
    
    if (copy_to_user(buffer, kernel_buffer, len))
        return -EFAULT;
        
    *pos = len;
    return len;
}

// Writer function
static ssize_t writer_write(struct file *file, const char __user *buffer,
                           size_t count, loff_t *pos)
{
    char input[32];
    int new_value;
    
    if (count > sizeof(input) - 1)
        return -EINVAL;
        
    if (copy_from_user(input, buffer, count))
        return -EFAULT;
        
    input[count] = '\0';
    
    if (kstrtoint(input, 10, &new_value) != 0)
        return -EINVAL;
    
    pr_info("Writer %d acquiring write lock\n", current->pid);
    
    // Acquire write lock (exclusive access)
    write_lock(&data_rwlock);
    
    shared_data = new_value;
    last_update = jiffies;
    
    // Simulate write operation
    msleep(2000);
    
    write_unlock(&data_rwlock);
    
    pr_info("Writer %d updated data to %d\n", current->pid, new_value);
    
    return count;
}

static const struct proc_ops reader_fops = {
    .proc_read = reader_read,
};

static const struct proc_ops writer_fops = {
    .proc_write = writer_write,
};

static int __init rw_lock_init(void)
{
    proc_read_entry = proc_create("rwlock_reader", 0444, NULL, &reader_fops);
    if (!proc_read_entry)
        return -ENOMEM;
        
    proc_write_entry = proc_create("rwlock_writer", 0222, NULL, &writer_fops);
    if (!proc_write_entry) {
        proc_remove(proc_read_entry);
        return -ENOMEM;
    }
    
    last_update = jiffies;
    pr_info("RWLock demo loaded\n");
    pr_info("Read from: /proc/rwlock_reader\n");
    pr_info("Write to: /proc/rwlock_writer\n");
    return 0;
}

static void __exit rw_lock_exit(void)
{
    proc_remove(proc_read_entry);
    proc_remove(proc_write_entry);
    pr_info("RWLock demo unloaded\n");
}

module_init(rw_lock_init);
module_exit(rw_lock_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Reader-Writer lock demonstration");