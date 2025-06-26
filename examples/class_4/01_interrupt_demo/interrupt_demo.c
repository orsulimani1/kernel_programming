#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/atomic.h>

MODULE_LICENSE("GPL");

static struct timer_list my_timer;
static atomic_t interrupt_count = ATOMIC_INIT(0);
static struct proc_dir_entry *proc_entry;

/* Interrupt handler that catches the timer */
static irqreturn_t timer_irq_handler(int irq, void *dev_id)
{
    atomic_inc(&interrupt_count);
    pr_info("Timer interrupt caught! Count: %d\n", atomic_read(&interrupt_count));
    return IRQ_HANDLED;
}

/* Timer callback that simulates interrupt */
static void timer_callback(struct timer_list *t)
{
    timer_irq_handler(0, NULL);  /* Simulate interrupt */
}

static int proc_show(struct seq_file *m, void *v)
{
    seq_printf(m, "Interrupt count: %d\n", atomic_read(&interrupt_count));
    seq_printf(m, "Timer active: %s\n", timer_pending(&my_timer) ? "Yes" : "No");
    return 0;
}

static int proc_open(struct inode *inode, struct file *file)
{
    /* Set timer when proc file is opened */
    mod_timer(&my_timer, jiffies + msecs_to_jiffies(2000));
    pr_info("Timer set for 2 seconds from proc_open\n");
    
    return single_open(file, proc_show, NULL);
}

static const struct proc_ops proc_fops = {
    .proc_open = proc_open,
    .proc_read = seq_read,
    .proc_release = single_release,
};

static int __init timer_irq_init(void)
{
    timer_setup(&my_timer, timer_callback, 0);
    proc_entry = proc_create("timer_irq", 0444, NULL, &proc_fops);
    
    pr_info("Timer interrupt demo loaded\n");
    return proc_entry ? 0 : -ENOMEM;
}

static void __exit timer_irq_exit(void)
{
    del_timer_sync(&my_timer);
    proc_remove(proc_entry);
    pr_info("Timer interrupt demo unloaded. Total: %d\n", 
            atomic_read(&interrupt_count));
}

module_init(timer_irq_init);
module_exit(timer_irq_exit);