#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/smp.h>

static struct timer_list softirq_timer;
static int count = 0;
static DEFINE_PER_CPU(int, softirq_cpu_count);
static struct proc_dir_entry *proc_entry;

static void raise_softirq_timer(struct timer_list *timer)
{
    int cpu = smp_processor_id();
    
    pr_info("CPU %d: Raising NET_RX_SOFTIRQ #%d\n", cpu, ++count);
    
    // Track per-CPU statistics
    this_cpu_inc(softirq_cpu_count);
    
    // Raise different softirqs to show variety
    switch (count % 3) {
    case 0:
        raise_softirq(NET_RX_SOFTIRQ);
        pr_info("Raised NET_RX_SOFTIRQ\n");
        break;
    case 1:
        raise_softirq(TASKLET_SOFTIRQ);
        pr_info("Raised TASKLET_SOFTIRQ\n");
        break;
    case 2:
        raise_softirq(TIMER_SOFTIRQ);
        pr_info("Raised TIMER_SOFTIRQ\n");
        break;
    }
    
    mod_timer(&softirq_timer, jiffies + 2 * HZ);
}

/* Proc interface to show softirq activity */
static int softirq_proc_show(struct seq_file *m, void *v)
{
    int cpu;
    
    seq_printf(m, "Softirq Raising Demo\n");
    seq_printf(m, "Total softirqs raised: %d\n", count);
    seq_printf(m, "\nPer-CPU raises:\n");
    
    for_each_online_cpu(cpu) {
        seq_printf(m, "CPU %d: %d raises\n", cpu, per_cpu(softirq_cpu_count, cpu));
    }
    
    seq_printf(m, "\nCheck system softirq stats:\n");
    seq_printf(m, "cat /proc/softirqs\n");
    
    return 0;
}

static int softirq_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, softirq_proc_show, NULL);
}

static const struct proc_ops softirq_proc_ops = {
    .proc_open = softirq_proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static int __init demo_softirq_init(void)
{
    timer_setup(&softirq_timer, raise_softirq_timer, 0);
    mod_timer(&softirq_timer, jiffies + HZ);
    
    proc_entry = proc_create("softirq_demo", 0444, NULL, &softirq_proc_ops);
    
    pr_info("Softirq demo loaded. Monitor: cat /proc/softirq_demo\n");
    pr_info("Also check: cat /proc/softirqs\n");
    return 0;
}

static void __exit demo_softirq_exit(void)
{
    del_timer_sync(&softirq_timer);
    proc_remove(proc_entry);
    pr_info("Softirq demo unloaded. Total raised: %d\n", count);
}

MODULE_LICENSE("GPL");
module_init(demo_softirq_init);
module_exit(demo_softirq_exit);