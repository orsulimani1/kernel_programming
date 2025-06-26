#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/semaphore.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/atomic.h>

#define MAX_RESOURCES 3

// Synchronization primitives
static DEFINE_MUTEX(shared_data_mutex);
static struct semaphore resource_pool;

// Shared data protected by mutex
static int shared_data = 0;
static char shared_message[64] = "Initial state";

// Resource tracking (protected by semaphore)
static atomic_t active_users = ATOMIC_INIT(0);
static int resource_id_counter = 1;

static struct proc_dir_entry *proc_dir;
static struct proc_dir_entry *proc_mutex, *proc_semaphore, *proc_status;

// Mutex demonstration
static ssize_t mutex_demo_read(struct file *file, char __user *buffer,
                              size_t count, loff_t *pos)
{
    char output[256];
    int len, local_data;
    char local_msg[64];
    
    if (*pos > 0)
        return 0;
    
    pr_info("PID %d requesting mutex lock\n", current->pid);
    
    if (mutex_lock_interruptible(&shared_data_mutex)) {
        pr_info("PID %d interrupted while waiting for mutex\n", current->pid);
        return -ERESTARTSYS;
    }
    
    pr_info("PID %d acquired mutex\n", current->pid);
    
    // Critical section - can sleep here
    shared_data += current->pid % 100;
    snprintf(shared_message, sizeof(shared_message), 
             "Updated by PID %d", current->pid);
    
    // Copy data while holding lock
    local_data = shared_data;
    strncpy(local_msg, shared_message, sizeof(local_msg));
    
    // Simulate work that might sleep
    msleep(1000000);
    
    mutex_unlock(&shared_data_mutex);
    pr_info("PID %d released mutex\n", current->pid);
    
    len = snprintf(output, sizeof(output),
                   "=== Mutex Demo ===\n"
                   "Shared data: %d\n"
                   "Message: %s\n"
                   "PID: %d\n"
                   "Mutex allows sleeping in critical section\n",
                   local_data, local_msg, current->pid);
    
    if (copy_to_user(buffer, output, len))
        return -EFAULT;
        
    *pos = len;
    return len;
}

// Semaphore demonstration
static ssize_t semaphore_demo_read(struct file *file, char __user *buffer,
                                  size_t count, loff_t *pos)
{
    char output[256];
    int len, user_count, my_resource_id;
    
    if (*pos > 0)
        return 0;
    
    pr_info("PID %d requesting semaphore (resource)\n", current->pid);
    
    if (down_interruptible(&resource_pool)) {
        pr_info("PID %d interrupted while waiting for resource\n", current->pid);
        return -ERESTARTSYS;
    }
    
    // Got a resource - track usage
    user_count = atomic_inc_return(&active_users);
    my_resource_id = resource_id_counter++;
    
    pr_info("PID %d acquired resource #%d (%d/%d resources in use)\n", 
            current->pid, my_resource_id, user_count, MAX_RESOURCES);
    
    // Simulate resource usage
    msleep(2000000);
    
    len = snprintf(output, sizeof(output),
                   "=== Semaphore Demo ===\n"
                   "Resource ID: %d\n"
                   "Used by PID: %d\n"
                   "Active users: %d/%d\n"
                   "Semaphore controls resource pool access\n",
                   my_resource_id, current->pid, user_count, MAX_RESOURCES);
    
    // Release resource
    atomic_dec(&active_users);
    up(&resource_pool);
    
    pr_info("PID %d released resource #%d\n", current->pid, my_resource_id);
    
    if (copy_to_user(buffer, output, len))
        return -EFAULT;
        
    *pos = len;
    return len;
}

// Status overview
static ssize_t status_read(struct file *file, char __user *buffer,
                          size_t count, loff_t *pos)
{
    char output[256];
    int len;
    
    if (*pos > 0)
        return 0;
    
    len = snprintf(output, sizeof(output),
                   "=== Mutex & Semaphore Status ===\n"
                   "Shared data: %d\n"
                   "Shared message: %s\n"
                   "Active resource users: %d/%d\n"
                   "Resource counter: %d\n"
                   "\nUsage:\n"
                   "cat /proc/sync_demo/mutex    - Test mutex\n"
                   "cat /proc/sync_demo/semaphore - Test semaphore\n",
                   shared_data, shared_message, 
                   atomic_read(&active_users), MAX_RESOURCES,
                   resource_id_counter - 1);
    
    if (copy_to_user(buffer, output, len))
        return -EFAULT;
        
    *pos = len;
    return len;
}

static const struct proc_ops mutex_fops = {
    .proc_read = mutex_demo_read,
};

static const struct proc_ops semaphore_fops = {
    .proc_read = semaphore_demo_read,
};

static const struct proc_ops status_fops = {
    .proc_read = status_read,
};

static int __init mutex_sem_init(void)
{
    // Initialize semaphore with MAX_RESOURCES permits
    sema_init(&resource_pool, MAX_RESOURCES);
    
    // Create proc directory
    proc_dir = proc_mkdir("sync_demo", NULL);
    if (!proc_dir)
        return -ENOMEM;
    
    proc_mutex = proc_create("mutex", 0444, proc_dir, &mutex_fops);
    proc_semaphore = proc_create("semaphore", 0444, proc_dir, &semaphore_fops);
    proc_status = proc_create("status", 0444, proc_dir, &status_fops);
    
    if (!proc_mutex || !proc_semaphore || !proc_status) {
        proc_remove(proc_dir);
        return -ENOMEM;
    }
    
    pr_info("Mutex/Semaphore demo loaded\n");
    pr_info("Interfaces:\n");
    pr_info("  /proc/sync_demo/mutex - Test sleeping locks\n");
    pr_info("  /proc/sync_demo/semaphore - Test resource pool (%d resources)\n", MAX_RESOURCES);
    pr_info("  /proc/sync_demo/status - View current state\n");
    
    return 0;
}

static void __exit mutex_sem_exit(void)
{
    proc_remove(proc_dir);
    pr_info("Mutex/Semaphore demo unloaded\n");
    pr_info("Final state: shared_data=%d, active_users=%d\n",
            shared_data, atomic_read(&active_users));
}

module_init(mutex_sem_init);
module_exit(mutex_sem_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kernel Course");
MODULE_DESCRIPTION("Mutex and Semaphore demonstration");