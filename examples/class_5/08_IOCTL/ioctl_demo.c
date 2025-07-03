// interrupt_timer_demo.c
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kernel Programming Course");
MODULE_DESCRIPTION("Interrupt handling and timer management demo");
MODULE_VERSION("1.0");

// Global variables for demonstration
static struct timer_list demo_timer;
static struct tasklet_struct demo_tasklet;
static struct work_struct demo_work;
static struct workqueue_struct *demo_wq;
static struct hrtimer hr_timer;

// Statistics
static atomic_t timer_count = ATOMIC_INIT(0);
static atomic_t tasklet_count = ATOMIC_INIT(0);
static atomic_t work_count = ATOMIC_INIT(0);
static atomic_t hrtimer_count = ATOMIC_INIT(0);

// Proc interface
static struct proc_dir_entry *proc_entry;

/* Top-half: Simulated interrupt handler */
static irqreturn_t demo_interrupt_handler(int irq, void *dev_id)
{
    // Top-half: minimal processing
    pr_info("Timer Demo: Top-half interrupt handler\n");
    
    // Schedule bottom-half processing
    tasklet_schedule(&demo_tasklet);
    queue_work(demo_wq, &demo_work);
    
    return IRQ_HANDLED;
}

/* Bottom-half: Tasklet handler */
static void demo_tasklet_handler(unsigned long data)
{
    // Bottom-half: more complex processing allowed
    atomic_inc(&tasklet_count);
    pr_info("Timer Demo: Tasklet executed (count: %d)\n", 
            atomic_read(&tasklet_count));
    
    // Tasklet context: no sleeping, atomic context only
    // Can use spinlocks, atomic operations
}

/* Bottom-half: Work queue handler */
static void demo_work_handler(struct work_struct *work)
{
    // Process context: can sleep, allocate memory, etc.
    atomic_inc(&work_count);
    pr_info("Timer Demo: Work queue executed (count: %d)\n",
            atomic_read(&work_count));
    
    // Can perform blocking operations here
    msleep(10);  // Demonstrate sleeping capability
}

/* Traditional timer callback */
static void demo_timer_callback(struct timer_list *timer)
{
    atomic_inc(&timer_count);
    pr_info("Timer Demo: Traditional timer fired (count: %d)\n",
            atomic_read(&timer_count));
    
    // Simulate interrupt for bottom-half demonstration
    demo_interrupt_handler(0, NULL);
    
    // Reschedule timer for 2 seconds
    mod_timer(&demo_timer, jiffies + 2 * HZ);
}

/* High-resolution timer callback */
static enum hrtimer_restart hr_timer_callback(struct hrtimer *timer)
{
    atomic_inc(&hrtimer_count);
    pr_info("Timer Demo: HR timer fired (count: %d)\n",
            atomic_read(&hrtimer_count));
    
    // Restart timer with 500ms interval
    hrtimer_forward_now(timer, ms_to_ktime(500));
    return HRTIMER_RESTART;
}

/* Proc interface for viewing statistics */
static int demo_proc_show(struct seq_file *m, void *v)
{
    seq_printf(m, "Timer and Interrupt Demo Statistics\n");
    seq_printf(m, "====================================\n");
    seq_printf(m, "Current jiffies: %lu\n", jiffies);
    seq_printf(m, "HZ value: %d\n", HZ);
    seq_printf(m, "Traditional timer count: %d\n", atomic_read(&timer_count));
    seq_printf(m, "High-res timer count: %d\n", atomic_read(&hrtimer_count));
    seq_printf(m, "Tasklet count: %d\n", atomic_read(&tasklet_count));
    seq_printf(m, "Work queue count: %d\n", atomic_read(&work_count));
    
    // Demonstrate time conversion
    seq_printf(m, "\nTime Conversions:\n");
    seq_printf(m, "1000ms = %lu jiffies\n", msecs_to_jiffies(1000));
    seq_printf(m, "Current uptime: %u seconds\n", jiffies_to_msecs(jiffies) / 1000);
    
    return 0;
}

static int demo_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, demo_proc_show, NULL);
}

static const struct proc_ops demo_proc_ops = {
    .proc_open = demo_proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static int __init timer_demo_init(void)
{
    pr_info("Timer Demo: Module loaded\n");
    
    // Create work queue
    demo_wq = create_singlethread_workqueue("demo_wq");
    if (!demo_wq) {
        pr_err("Timer Demo: Failed to create workqueue\n");
        return -ENOMEM;
    }
    
    // Initialize tasklet
    tasklet_init(&demo_tasklet, demo_tasklet_handler, 0);
    
    // Initialize work
    INIT_WORK(&demo_work, demo_work_handler);
    
    // Initialize traditional timer
    timer_setup(&demo_timer, demo_timer_callback, 0);
    demo_timer.expires = jiffies + HZ;  // 1 second from now
    add_timer(&demo_timer);
    
    // Initialize high-resolution timer
    hrtimer_init(&hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    hr_timer.function = hr_timer_callback;
    hrtimer_start(&hr_timer, ms_to_ktime(500), HRTIMER_MODE_REL);
    
    // Create proc interface
    proc_entry = proc_create("timer_demo", 0444, NULL, &demo_proc_ops);
    if (!proc_entry) {
        pr_err("Timer Demo: Failed to create proc entry\n");
        goto cleanup_timers;
    }
    
    pr_info("Timer Demo: All timers started, check /proc/timer_demo\n");
    return 0;

cleanup_timers:
    del_timer_sync(&demo_timer);
    hrtimer_cancel(&hr_timer);
    destroy_workqueue(demo_wq);
    return -ENOMEM;
}

static void __exit timer_demo_exit(void)
{
    pr_info("Timer Demo: Module unloading\n");
    
    // Remove proc interface
    proc_remove(proc_entry);
    
    // Stop timers
    del_timer_sync(&demo_timer);
    hrtimer_cancel(&hr_timer);
    
    // Cleanup tasklet and work
    tasklet_kill(&demo_tasklet);
    flush_workqueue(demo_wq);
    destroy_workqueue(demo_wq);
    
    pr_info("Timer Demo: Module unloaded\n");
}

module_init(timer_demo_init);
module_exit(timer_demo_exit);