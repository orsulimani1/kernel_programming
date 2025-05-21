/**
 * process_info.c - A kernel module to display process information
 *
 * This module creates a /proc/process_info entry that displays
 * information about all running processes when read.
 * It also logs process information to the kernel log (dmesg).
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/mm.h>

#define PROCFS_NAME "process_info"

static struct proc_dir_entry *proc_file;

/* Function to get scheduler policy as string */
static const char *get_policy_name(int policy)
{
    switch (policy) {
    case SCHED_NORMAL:
        return "SCHED_NORMAL/OTHER";
    case SCHED_FIFO:
        return "SCHED_FIFO";
    case SCHED_RR:
        return "SCHED_RR";
    case SCHED_BATCH:
        return "SCHED_BATCH";
    case SCHED_IDLE:
        return "SCHED_IDLE";
    case SCHED_DEADLINE:
        return "SCHED_DEADLINE";
    default:
        return "unknown";
    }
}

/* Function to get process state as string */
static const char *get_state_name(long state)
{
    /* Only checking the basic states */
    if (state & TASK_RUNNING)
        return "TASK_RUNNING";
    if (state & TASK_INTERRUPTIBLE)
        return "TASK_INTERRUPTIBLE";
    if (state & TASK_UNINTERRUPTIBLE)
        return "TASK_UNINTERRUPTIBLE";
    if (state & TASK_STOPPED)
        return "TASK_STOPPED";
    if (state & TASK_TRACED)
        return "TASK_TRACED";
    if (state & EXIT_ZOMBIE)
        return "EXIT_ZOMBIE";
    if (state & EXIT_DEAD)
        return "EXIT_DEAD";
    return "unknown";
}

/* Function to print all processes to the kernel log */
static void print_process_info_to_dmesg(void)
{
    struct task_struct *task;
    
    pr_info("======= Process Information (dmesg output) =======\n");
    pr_info("PID\tPPID\tSTATE\t\tPOLICY\t\tNAME\n");
    
    /* For each process */
    for_each_process(task) {
        pr_info("%d\t%d\t%-15s\t%-15s\t%s\n",
                task->pid,
                task->parent->pid,
                get_state_name(task->__state),
                get_policy_name(task->policy),
                task->comm);
    }
    
    pr_info("=================================================\n");
}

/* seq_file operations for /proc file */
static int proc_show(struct seq_file *m, void *v)
{
    struct task_struct *task;
    
    seq_printf(m, "======= Process Information =======\n");
    seq_printf(m, "%-5s %-5s %-15s %-15s %-5s %-5s %s\n",
               "PID", "PPID", "STATE", "POLICY", "PRIO", "NICE", "NAME");
    
    /* For each process */
    for_each_process(task) {
        seq_printf(m, "%-5d %-5d %-15s %-15s %-5d %-5d %s\n",
                   task->pid,
                   task->parent->pid,
                   get_state_name(task->__state),
                   get_policy_name(task->policy),
                   task->prio,
                   task_nice(task),
                   task->comm);
    }
    
    seq_printf(m, "=================================\n");
    return 0;
}

static int proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, proc_show, NULL);
}

static const struct proc_ops proc_fops = {
    .proc_open = proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static int __init process_info_init(void)
{
    /* Create proc entry */
    proc_file = proc_create(PROCFS_NAME, 0644, NULL, &proc_fops);
    if (proc_file == NULL) {
        pr_err("Failed to create /proc/%s\n", PROCFS_NAME);
        return -ENOMEM;
    }
    
    /* Print process information to kernel log on module load */
    print_process_info_to_dmesg();
    
    pr_info("Process info module loaded. Check /proc/%s\n", PROCFS_NAME);
    return 0;
}

static void __exit process_info_exit(void)
{
    /* Remove proc entry */
    proc_remove(proc_file);
    pr_info("Process info module unloaded\n");
}

module_init(process_info_init);
module_exit(process_info_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Process information display module");
MODULE_VERSION("1.0");