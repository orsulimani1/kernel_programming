#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

static struct tasklet_struct normal_tasklet;
static struct tasklet_struct hi_tasklet;
static struct timer_list tasklet_timer;
static int normal_count = 0, hi_count = 0;
static struct proc_dir_entry *proc_entry;

/* Normal priority tasklet */
static void normal_tasklet_handler(unsigned long data)
{
    pr_info("NORMAL TASKLET: count %d on CPU %d\n", 
            ++normal_count, smp_processor_id());
    
    // Demonstrate serialization - same tasklet can't run on multiple CPUs
    pr_info("NORMAL TASKLET: This tasklet is serialized\n");
}

/* High priority tasklet */
static void hi_tasklet_handler(unsigned long data)
{
    pr_info("HI-PRIORITY TASKLET: count %d on CPU %d\n", 
            ++hi_count, smp_processor_id());
    
    pr_info("HI-PRIORITY TASKLET: Runs before normal tasklets\n");
}

static void schedule_tasklet_timer(struct timer_list *timer)
{
    static int counter = 0;
    counter++;
    
    pr_info("TIMER: Scheduling tasklets (round %d)\n", counter);
    
    // Schedule both types to show priority
    if (counter % 2 == 0) {
        tasklet_hi_schedule(&hi_tasklet);
        pr_info("TIMER: High priority tasklet scheduled\n");
    }
    
    tasklet_schedule(&normal_tasklet);
    pr_info("TIMER: Normal tasklet scheduled\n");
    
    // Show tasklet states
    if (tasklet_is_scheduled(&normal_tasklet))
        pr_info("TIMER: Normal tasklet is in scheduled state\n");
    
    mod_timer(&tasklet_timer, jiffies + 3 * HZ);
}

/* Proc interface with manual control */
static int tasklet_proc_show(struct seq_file *m, void *v)
{
    seq_printf(m, "Tasklet Demo\n");
    seq_printf(m, "Normal tasklets: %d\n", normal_count);
    seq_printf(m, "Hi-priority tasklets: %d\n", hi_count);
    seq_printf(m, "\nTasklet States:\n");
    seq_printf(m, "Normal scheduled: %s\n", 
               tasklet_is_scheduled(&normal_tasklet) ? "YES" : "NO");
    seq_printf(m, "Hi-priority scheduled: %s\n",
               tasklet_is_scheduled(&hi_tasklet) ? "YES" : "NO");
    seq_printf(m, "\nManual control:\n");
    seq_printf(m, "echo 1 > /proc/tasklet_demo  # Schedule normal\n");
    seq_printf(m, "echo 2 > /proc/tasklet_demo  # Schedule hi-priority\n");
    return 0;
}

static ssize_t tasklet_proc_write(struct file *file, const char __user *buffer,
                                 size_t count, loff_t *pos)
{
    char input[8];
    int value;
    
    if (count >= sizeof(input)) return -EINVAL;
    if (copy_from_user(input, buffer, count)) return -EFAULT;
    input[count] = '\0';
    if (kstrtoint(input, 10, &value) != 0) return -EINVAL;
    
    switch (value) {
    case 1:
        tasklet_schedule(&normal_tasklet);
        pr_info("USER: Manual normal tasklet scheduled\n");
        break;
    case 2:
        tasklet_hi_schedule(&hi_tasklet);
        pr_info("USER: Manual hi-priority tasklet scheduled\n");
        break;
    default:
        return -EINVAL;
    }
    
    return count;
}

static int tasklet_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, tasklet_proc_show, NULL);
}

static const struct proc_ops tasklet_proc_ops = {
    .proc_open = tasklet_proc_open,
    .proc_read = seq_read,
    .proc_write = tasklet_proc_write,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static int __init tasklet_init(void)
{
    tasklet_init(&normal_tasklet, normal_tasklet_handler, 0);
    tasklet_init(&hi_tasklet, hi_tasklet_handler, 0);
    
    timer_setup(&tasklet_timer, schedule_tasklet_timer, 0);
    mod_timer(&tasklet_timer, jiffies + HZ);
    
    proc_entry = proc_create("tasklet_demo", 0666, NULL, &tasklet_proc_ops);
    
    pr_info("Tasklet demo loaded\n");
    return 0;
}

static void __exit tasklet_exit(void)
{
    del_timer_sync(&tasklet_timer);
    tasklet_kill(&normal_tasklet);
    tasklet_kill(&hi_tasklet);
    proc_remove(proc_entry);
    pr_info("Tasklet demo unloaded. Normal: %d, Hi: %d\n", normal_count, hi_count);
}

MODULE_LICENSE("GPL");
module_init(tasklet_init);
module_exit(tasklet_exit);