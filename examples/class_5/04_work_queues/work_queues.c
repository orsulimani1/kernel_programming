#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>

static struct work_struct demo_work;
static struct delayed_work delayed_work;
static struct workqueue_struct *custom_wq;
static struct timer_list work_timer;
static int work_count = 0, delayed_count = 0;
static struct proc_dir_entry *proc_entry;

/* Normal work handler */
static void demo_work_handler(struct work_struct *work)
{
    char *buffer;
    
    pr_info("WORK: processing count %d PID %d COMM %s\n", 
            ++work_count, current->pid, current->comm);
    
    // Demonstrate process context capabilities
    buffer = kmalloc(256, GFP_KERNEL);  // Can allocate with GFP_KERNEL
    if (buffer) {
        snprintf(buffer, 256, "Work item %d allocated memory", work_count);
        pr_info("WORK: %s\n", buffer);
        kfree(buffer);
    }
    
    // Can sleep in work context
    msleep(100);
    pr_info("WORK: Completed sleep operation\n");
    
    // Voluntary preemption
    cond_resched();
}

/* Delayed work handler */
static void delayed_work_handler(struct work_struct *work)
{
    pr_info("DELAYED WORK: count %d after delay (PID %d)\n", 
            ++delayed_count, current->pid);
    
    // Reschedule delayed work
    schedule_delayed_work(&delayed_work, 5 * HZ);
}

static void schedule_work_timer(struct timer_list *timer)
{
    static int counter = 0;
    counter++;
    
    pr_info("TIMER: Scheduling work items (round %d)\n", counter);
    
    // Alternate between system and custom workqueue
    if (counter % 2 == 0) {
        schedule_work(&demo_work);  // System workqueue
        pr_info("TIMER: Scheduled on system workqueue\n");
    } else {
        queue_work(custom_wq, &demo_work);  // Custom workqueue
        pr_info("TIMER: Scheduled on custom workqueue\n");
    }
    
    mod_timer(&work_timer, jiffies + 4 * HZ);
}

/* Proc interface */
static int work_proc_show(struct seq_file *m, void *v)
{
    seq_printf(m, "Work Queue Demo\n");
    seq_printf(m, "Normal work executions: %d\n", work_count);
    seq_printf(m, "Delayed work executions: %d\n", delayed_count);
    seq_printf(m, "\nWork Queue Features:\n");
    seq_printf(m, "- Runs in process context\n");
    seq_printf(m, "- Can sleep and allocate memory\n");
    seq_printf(m, "- Can be delayed/scheduled\n");
    seq_printf(m, "- Custom workqueues available\n");
    
    return 0;
}

static int work_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, work_proc_show, NULL);
}

static const struct proc_ops work_proc_ops = {
    .proc_open = work_proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static int __init work_init(void)
{
    INIT_WORK(&demo_work, demo_work_handler);
    INIT_DELAYED_WORK(&delayed_work, delayed_work_handler);
    
    // Create custom workqueue
    custom_wq = create_singlethread_workqueue("demo_wq");
    if (!custom_wq) {
        pr_err("Failed to create custom workqueue\n");
        return -ENOMEM;
    }
    
    // Start delayed work
    schedule_delayed_work(&delayed_work, 3 * HZ);
    
    timer_setup(&work_timer, schedule_work_timer, 0);
    mod_timer(&work_timer, jiffies + 2 * HZ);
    
    proc_entry = proc_create("work_demo", 0444, NULL, &work_proc_ops);
    
    pr_info("Work queue demo loaded\n");
    return 0;
}

static void __exit work_exit(void)
{
    del_timer_sync(&work_timer);
    cancel_work_sync(&demo_work);
    cancel_delayed_work_sync(&delayed_work);
    destroy_workqueue(custom_wq);
    proc_remove(proc_entry);
    pr_info("Work demo unloaded. Work: %d, Delayed: %d\n", work_count, delayed_count);
}

MODULE_LICENSE("GPL");
module_init(work_init);
module_exit(work_exit);