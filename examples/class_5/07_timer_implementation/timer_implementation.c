#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/atomic.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kernel Programming Course");
MODULE_DESCRIPTION("Timer Demo");
MODULE_VERSION("1.0");

// Timer structures
static struct timer_list periodic_timer;
static struct hrtimer hr_timer;

// Counters
static atomic_t periodic_count = ATOMIC_INIT(0);
static atomic_t hr_count = ATOMIC_INIT(0);

// Configuration
static int timer_interval_ms = 1000;

/* Periodic timer callback */
static void periodic_timer_callback(struct timer_list *timer)
{
    int count = atomic_inc_return(&periodic_count);
    pr_info("Periodic timer fired: #%d\n", count);
    
    // Reschedule for next interval
    mod_timer(&periodic_timer, jiffies + msecs_to_jiffies(timer_interval_ms));
}

/* High-resolution timer callback */
static enum hrtimer_restart hr_timer_callback(struct hrtimer *timer)
{
    int count = atomic_inc_return(&hr_count);
    pr_info("HR timer fired: #%d\n", count);
    
    // Reschedule
    hrtimer_forward_now(timer, ms_to_ktime(timer_interval_ms));
    return HRTIMER_RESTART;
}

/* Proc interface to show status */
static int timer_proc_show(struct seq_file *m, void *v)
{
    seq_printf(m, "Simple Timer Demo Status\n");
    seq_printf(m, "========================\n");
    seq_printf(m, "Timer interval: %d ms\n", timer_interval_ms);
    seq_printf(m, "Periodic timer fires: %d\n", atomic_read(&periodic_count));
    seq_printf(m, "HR timer fires: %d\n", atomic_read(&hr_count));
    seq_printf(m, "Current jiffies: %lu\n", jiffies);
    seq_printf(m, "HZ value: %d\n", HZ);
    
    return 0;
}

static int timer_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, timer_proc_show, NULL);
}

static const struct proc_ops timer_proc_ops = {
    .proc_open = timer_proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

/* Module initialization */
static int __init simple_timer_init(void)
{
    pr_info("Simple timer module loading...\n");
    
    // Create proc entry
    if (!proc_create("simple_timer", 0444, NULL, &timer_proc_ops)) {
        pr_err("Failed to create proc entry\n");
        return -ENOMEM;
    }
    
    // Initialize periodic timer
    timer_setup(&periodic_timer, periodic_timer_callback, 0);
    mod_timer(&periodic_timer, jiffies + msecs_to_jiffies(timer_interval_ms));
    
    // Initialize high-resolution timer
    hrtimer_init(&hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    hr_timer.function = hr_timer_callback;
    hrtimer_start(&hr_timer, ms_to_ktime(timer_interval_ms), HRTIMER_MODE_REL);
    
    pr_info("Timers started with %d ms interval\n", timer_interval_ms);
    pr_info("Check status: cat /proc/simple_timer\n");
    
    return 0;
}

/* Module cleanup */
static void __exit simple_timer_exit(void)
{
    // Stop timers
    del_timer_sync(&periodic_timer);
    hrtimer_cancel(&hr_timer);
    
    // Remove proc entry
    proc_remove(proc_create("simple_timer", 0444, NULL, &timer_proc_ops));
    
    pr_info("Simple timer module unloaded\n");
    pr_info("Final counts - Periodic: %d, HR: %d\n", 
            atomic_read(&periodic_count), atomic_read(&hr_count));
}

module_init(simple_timer_init);
module_exit(simple_timer_exit);