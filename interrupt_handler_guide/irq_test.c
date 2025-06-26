#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/atomic.h>

MODULE_LICENSE("GPL");

#define TEST_IRQ 15
static atomic_t irq_count = ATOMIC_INIT(0);
static struct timer_list test_timer;
static struct proc_dir_entry *proc_entry;

static irqreturn_t test_handler(int irq, void *dev_id)
{
    atomic_inc(&irq_count);
    pr_info("Test IRQ %d: count=%d\n", irq, atomic_read(&irq_count));
    return IRQ_HANDLED;
}

static void timer_func(struct timer_list *t)
{
    test_handler(TEST_IRQ, NULL);
    mod_timer(&test_timer, jiffies + HZ);
}

static int proc_show(struct seq_file *m, void *v)
{
    seq_printf(m, "IRQ count: %d\n", atomic_read(&irq_count));
    return 0;
}

static int proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, proc_show, NULL);
}

static const struct proc_ops proc_fops = {
    .proc_open = proc_open,
    .proc_read = seq_read,
    .proc_release = single_release,
};

static int __init test_init(void)
{
    timer_setup(&test_timer, timer_func, 0);
    mod_timer(&test_timer, jiffies + HZ);
    
    proc_entry = proc_create("irq_test", 0444, NULL, &proc_fops);
    pr_info("IRQ test module loaded\n");
    return 0;
}

static void __exit test_exit(void)
{
    del_timer_sync(&test_timer);
    proc_remove(proc_entry);
    pr_info("IRQ test module unloaded: %d interrupts\n", 
            atomic_read(&irq_count));
}

module_init(test_init);
module_exit(test_exit);