/**
 * task_monitor.c - Kernel module to monitor a specific process
 *
 * This module monitors a specific process identified by its PID
 * and prints detailed information about it to the kernel log.
 *
 * Usage: insmod task_monitor.ko pid=<target_pid>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/moduleparam.h>
#include <linux/mm.h>
#include <linux/cred.h>

/* Module parameters */
static int pid = 1; /* Default to init process */
module_param(pid, int, 0644);
MODULE_PARM_DESC(pid, "Process ID to monitor");

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

/* Function to monitor and print process information */
static int monitor_process(int target_pid)
{
    struct task_struct *task;
    struct task_struct *task_child;
    struct list_head *list;
    int child_count = 0;
    
    /* Find the task with the given PID */
    task = pid_task(find_vpid(target_pid), PIDTYPE_PID);
    if (!task) {
        pr_err("Process with PID %d not found\n", target_pid);
        return -ESRCH;
    }
    
    /* Print basic process information */
    pr_info("==== Detailed Process Information for PID %d ====\n", target_pid);
    pr_info("Name: %s\n", task->comm);
    pr_info("State: %s (0x%lx)\n", get_state_name(task->__state), (unsigned long)task->__state);
    pr_info("Process group: %d\n", task_pgrp_nr(task));
    pr_info("Session ID: %d\n", task_tgid_nr(task));
    
    /* Parent information */
    if (task->parent) {
        pr_info("Parent: %s (PID: %d)\n", task->parent->comm, task->parent->pid);
    }
    
    /* Child processes */
    pr_info("\nChild processes:\n");
    list_for_each(list, &task->children) {
        task_child = list_entry(list, struct task_struct, sibling);
        pr_info("  Child %d: %s (PID: %d, State: %s)\n", 
               ++child_count, task_child->comm, task_child->pid,
               get_state_name(task_child->__state));
    }
    if (child_count == 0) {
        pr_info("  No child processes\n");
    }
    
    /* Scheduling information */
    pr_info("\nScheduling Information:\n");
    pr_info("  Policy: %s\n", get_policy_name(task->policy));
    pr_info("  Priority: %d\n", task->prio);
    pr_info("  Static priority: %d\n", task->static_prio);
    pr_info("  Normal priority: %d\n", task->normal_prio);
    pr_info("  RT priority: %d\n", task->rt_priority);
    pr_info("  Nice value: %d\n", task_nice(task));
    
    /* Memory information */
    pr_info("\nMemory Information:\n");
    if (task->mm) {
        pr_info("  Total virtual memory: %lu KB\n", task->mm->total_vm * (PAGE_SIZE / 1024));
        pr_info("  Stack size: %lu KB\n", task->mm->stack_vm * (PAGE_SIZE / 1024));
    } else {
        pr_info("  Kernel thread (no mm_struct)\n");
    }
    
    /* Security information */
    pr_info("\nSecurity Information:\n");
    pr_info("  User ID: %d\n", __kuid_val(task->cred->uid));
    pr_info("  Group ID: %d\n", __kgid_val(task->cred->gid));
    
    pr_info("==== End of Process Information ====\n");
    return 0;
}

static int __init task_monitor_init(void)
{
    pr_info("Task Monitor: Module loaded\n");
    return monitor_process(pid);
}

static void __exit task_monitor_exit(void)
{
    pr_info("Task Monitor: Module unloaded\n");
}

module_init(task_monitor_init);
module_exit(task_monitor_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Task monitoring module");
MODULE_VERSION("1.0");