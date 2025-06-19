#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#define PROC_NAME "hello_proc"
#define MAX_SIZE 1024

static struct proc_dir_entry *proc_entry;
static char *kernel_buffer;

// Read function - called when user reads from /proc/hello_proc
static ssize_t proc_read(struct file *file, char __user *buffer, 
                        size_t count, loff_t *pos)
{
    int len;
    
    if (*pos > 0)
        return 0;  // EOF
        
    len = snprintf(kernel_buffer, MAX_SIZE, 
                   "Hello from kernel! PID: %d\n", current->pid);
    
    if (copy_to_user(buffer, kernel_buffer, len))
        return -EFAULT;
        
    *pos = len;
    return len;
}

// Write function - called when user writes to /proc/hello_proc
static ssize_t proc_write(struct file *file, const char __user *buffer,
                         size_t count, loff_t *pos)
{
    if (count > MAX_SIZE - 1)
        return -EINVAL;
        
    if (copy_from_user(kernel_buffer, buffer, count))
        return -EFAULT;
        
    kernel_buffer[count] = '\0';
    pr_info("Received from user: %s", kernel_buffer);
    
    return count;
}

static const struct proc_ops proc_fops = {
    .proc_read = proc_read,
    .proc_write = proc_write,
};

static int __init proc_init(void)
{
    kernel_buffer = kzalloc(MAX_SIZE, GFP_KERNEL);
    if (!kernel_buffer)
        return -ENOMEM;
        
    proc_entry = proc_create(PROC_NAME, 0666, NULL, &proc_fops);
    if (!proc_entry) {
        kfree(kernel_buffer);
        return -ENOMEM;
    }
    
    pr_info("procfs module loaded: /proc/%s\n", PROC_NAME);
    return 0;
}

static void __exit proc_exit(void)
{
    proc_remove(proc_entry);
    kfree(kernel_buffer);
    pr_info("procfs module unloaded\n");
}

module_init(proc_init);
module_exit(proc_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kernel Course");
MODULE_DESCRIPTION("Basic procfs example");