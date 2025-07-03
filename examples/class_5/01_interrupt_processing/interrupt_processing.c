#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/ktime.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/delay.h>


static struct work_struct bottom_half_work;
static struct timer_list irq_simulator;
static int top_count = 0, bottom_count = 0;
static ktime_t top_half_time, bottom_half_time;
static struct proc_dir_entry *proc_entry;

/* TOP-HALF: Fast processing with timing */
static irqreturn_t demo_top_half(int irq, void *dev_id)
{
    ktime_t start = ktime_get();
    
    pr_info("TOP-HALF: count %d (interrupt context)\n", ++top_count);
    
    // Simulate hardware acknowledgment
    if (in_interrupt())
        pr_info("TOP-HALF: Confirmed in interrupt context\n");
    
    schedule_work(&bottom_half_work);
    
    top_half_time = ktime_sub(ktime_get(), start);
    return IRQ_HANDLED;
}

/* BOTTOM-HALF: Can sleep with timing */
static void demo_bottom_half(struct work_struct *work)
{
    ktime_t start = ktime_get();
    
    pr_info("BOTTOM-HALF: count %d (process context PID %d)\n", 
            ++bottom_count, current->pid);
    
    // Demonstrate process context capabilities
    if (!in_interrupt())
        pr_info("BOTTOM-HALF: Confirmed in process context\n");
    
    // Can perform blocking operations
    msleep(10);  // Would crash in top-half!
    pr_info("BOTTOM-HALF: Completed sleep operation\n");
    
    bottom_half_time = ktime_sub(ktime_get(), start);
}

/* Timer to simulate interrupts */
static void irq_simulator_callback(struct timer_list *timer)
{
    demo_top_half(99, NULL);  // Simulate IRQ 99
    mod_timer(&irq_simulator, jiffies + 3 * HZ);
}

/* Proc interface */
static int demo_proc_show(struct seq_file *m, void *v)
{
    seq_printf(m, "Top-half vs Bottom-half Demo\n");
    seq_printf(m, "Top-half executions: %d (last: %lld ns)\n", 
               top_count, ktime_to_ns(top_half_time));
    seq_printf(m, "Bottom-half executions: %d (last: %lld ns)\n", 
               bottom_count, ktime_to_ns(bottom_half_time));
    seq_printf(m, "Speed ratio: %lld:1 (bottom vs top)\n",
               top_half_time ? ktime_to_ns(bottom_half_time) / ktime_to_ns(top_half_time) : 0);
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

static int __init demo_init(void)
{
    INIT_WORK(&bottom_half_work, demo_bottom_half);
    
    timer_setup(&irq_simulator, irq_simulator_callback, 0);
    mod_timer(&irq_simulator, jiffies + 2 * HZ);
    
    proc_entry = proc_create("top_bottom_demo", 0444, NULL, &demo_proc_ops);
    
    pr_info("Top-half vs Bottom-half demo loaded\n");
    return 0;
}

static void __exit demo_exit(void)
{
    del_timer_sync(&irq_simulator);
    flush_work(&bottom_half_work);
    proc_remove(proc_entry);
    pr_info("Demo unloaded. Top: %d, Bottom: %d\n", top_count, bottom_count);
}

MODULE_LICENSE("GPL");
module_init(demo_init);
module_exit(demo_exit);