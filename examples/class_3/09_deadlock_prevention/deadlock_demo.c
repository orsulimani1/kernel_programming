#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/uaccess.h>


static DEFINE_MUTEX(resource_a);
static DEFINE_MUTEX(resource_b);
static struct proc_dir_entry *proc_entry;

// Safe approach: trylock with timeout
static int safe_acquire_both_locks(void)
{
    int retries = 0;
    const int MAX_RETRIES = 10;
    
    while (retries < MAX_RETRIES) {
        // Try to acquire first lock
        if (mutex_trylock(&resource_a)) {
            // Got first lock, try second
            if (mutex_trylock(&resource_b)) {
                // Got both locks!
                return 0;
            }
            // Couldn't get second lock, release first
            mutex_unlock(&resource_a);
        }
        
        // Wait a bit and try again
        msleep(10);
        retries++;
    }
    
    return -EBUSY;  // Failed to acquire both locks
}

static void release_both_locks(void)
{
    mutex_unlock(&resource_b);
    mutex_unlock(&resource_a);
}

static ssize_t deadlock_demo_read(struct file *file, char __user *buffer,
                                 size_t count, loff_t *pos)
{
    char kernel_buffer[128];
    int len;
    
    if (*pos > 0)
        return 0;
    
    pr_info("Process %d attempting to acquire both locks\n", current->pid);
    
    if (safe_acquire_both_locks() == 0) {
        pr_info("Process %d acquired both locks safely\n", current->pid);
        
        // Simulate critical section work
        msleep(1000);
        
        len = snprintf(kernel_buffer, sizeof(kernel_buffer),
                      "Successfully acquired both resources (PID: %d)\n",
                      current->pid);
        
        release_both_locks();
        pr_info("Process %d released both locks\n", current->pid);
    } else {
        pr_info("Process %d failed to acquire locks (avoiding deadlock)\n", 
                current->pid);
        len = snprintf(kernel_buffer, sizeof(kernel_buffer),
                      "Failed to acquire resources - try again later\n");
    }
    
    if (copy_to_user(buffer, kernel_buffer, len))
        return -EFAULT;
        
    *pos = len;
    return len;
}

static const struct proc_ops deadlock_fops = {
    .proc_read = deadlock_demo_read,
};

static int __init deadlock_init(void)
{
    proc_entry = proc_create("deadlock_demo", 0444, NULL, &deadlock_fops);
    if (!proc_entry)
        return -ENOMEM;
        
    pr_info("Deadlock prevention demo loaded\n");
    return 0;
}

static void __exit deadlock_exit(void)
{
    proc_remove(proc_entry);
    pr_info("Deadlock prevention demo unloaded\n");
}

module_init(deadlock_init);
module_exit(deadlock_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Deadlock prevention demonstration");