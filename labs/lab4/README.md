# Class 4 Lab Exercises

## Exercise 1: Advanced Synchronization Mechanisms

### Objective
Implement and test different synchronization primitives with concurrent access patterns.

### Implementation (sync_lab.c)
```c
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/rwlock.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>

MODULE_LICENSE("GPL");

static int shared_counter = 0;
static DEFINE_SPINLOCK(counter_lock);
static DEFINE_MUTEX(data_mutex);
static DEFINE_RWLOCK(config_lock);

static int config_value = 100;
static struct task_struct *worker_thread;
static struct proc_dir_entry *proc_entry;

static int worker_function(void *data)
{
    int i;
    
    for (i = 0; i < 1000 && !kthread_should_stop(); i++) {
        /* Test spinlock */
        spin_lock(&counter_lock);
        shared_counter++;
        spin_unlock(&counter_lock);
        
        /* Test rwlock read */
        read_lock(&config_lock);
        if (config_value > 0)
            pr_info("Config value: %d\n", config_value);
        read_unlock(&config_lock);
        
        msleep(10);
    }
    return 0;
}

static int sync_show(struct seq_file *m, void *v)
{
    read_lock(&config_lock);
    seq_printf(m, "=== Synchronization Lab ===\n");
    seq_printf(m, "Counter: %d\n", shared_counter);
    seq_printf(m, "Config: %d\n", config_value);
    seq_printf(m, "Worker thread: %s\n", worker_thread ? "Running" : "Stopped");
    read_unlock(&config_lock);
    return 0;
}

static ssize_t sync_write(struct file *file, const char __user *buffer, 
                          size_t count, loff_t *pos)
{
    int new_value;
    char input[32];
    
    if (count >= sizeof(input))
        return -EINVAL;
    
    if (copy_from_user(input, buffer, count))
        return -EFAULT;
    
    input[count] = '\0';
    
    if (sscanf(input, "%d", &new_value) == 1) {
        write_lock(&config_lock);
        config_value = new_value;
        write_unlock(&config_lock);
        pr_info("Config updated to: %d\n", new_value);
    }
    
    return count;
}

static int sync_open(struct inode *inode, struct file *file)
{
    return single_open(file, sync_show, NULL);
}

static const struct proc_ops sync_fops = {
    .proc_open = sync_open,
    .proc_read = seq_read,
    .proc_write = sync_write,
    .proc_release = single_release,
};

static int __init sync_lab_init(void)
{
    proc_entry = proc_create("sync_lab", 0666, NULL, &sync_fops);
    if (!proc_entry)
        return -ENOMEM;
    
    worker_thread = kthread_run(worker_function, NULL, "sync_worker");
    if (IS_ERR(worker_thread)) {
        proc_remove(proc_entry);
        return PTR_ERR(worker_thread);
    }
    
    pr_info("Sync lab loaded\n");
    return 0;
}

static void __exit sync_lab_exit(void)
{
    if (worker_thread)
        kthread_stop(worker_thread);
    
    proc_remove(proc_entry);
    pr_info("Sync lab unloaded: counter=%d\n", shared_counter);
}

module_init(sync_lab_init);
module_exit(sync_lab_exit);
```

### Testing
```bash
# Monitor synchronization
cat /proc/sync_lab

# Test write lock
echo 200 > /proc/sync_lab

# Check kernel messages
dmesg | tail -20
```

---

## Exercise 2: Kernel Data Structures

### Objective
Create a task management system using linked lists, hash tables, and queues.

### Implementation (data_lab.c)
```c
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/list.h>
#include <linux/hashtable.h>
#include <linux/kfifo.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>

MODULE_LICENSE("GPL");

/* Task list */
struct task_entry {
    int id;
    char name[32];
    int priority;
    struct list_head list;
};

static LIST_HEAD(task_list);

/* Hash table for users */
#define USER_HASH_BITS 4
struct user_entry {
    int uid;
    char name[32];
    struct hlist_node node;
};

static DEFINE_HASHTABLE(user_table, USER_HASH_BITS);

/* Message queue */
struct message {
    int type;
    char data[64];
};

static DEFINE_KFIFO(msg_queue, struct message, 16);
static struct proc_dir_entry *proc_entry;

static int data_show(struct seq_file *m, void *v)
{
    struct task_entry *task;
    struct user_entry *user;
    struct message msg;
    int bkt;
    
    seq_printf(m, "=== Data Structures Lab ===\n\n");
    
    seq_printf(m, "Tasks:\n");
    list_for_each_entry(task, &task_list, list) {
        seq_printf(m, "  ID:%d Name:%s Priority:%d\n", 
                   task->id, task->name, task->priority);
    }
    
    seq_printf(m, "\nUsers:\n");
    hash_for_each(user_table, bkt, user, node) {
        seq_printf(m, "  UID:%d Name:%s\n", user->uid, user->name);
    }
    
    seq_printf(m, "\nMessage Queue:\n");
    seq_printf(m, "  Messages: %u/%u\n", kfifo_len(&msg_queue), kfifo_size(&msg_queue));
    
    /* Show a few messages */
    while (kfifo_out_peek(&msg_queue, &msg, 1)) {
        seq_printf(m, "  Type:%d Data:%s\n", msg.type, msg.data);
        break; /* Just show first message */
    }
    
    return 0;
}

static ssize_t data_write(struct file *file, const char __user *buffer,
                          size_t count, loff_t *pos)
{
    char input[128];
    char cmd[16], name[32];
    int id, priority;
    
    if (count >= sizeof(input))
        return -EINVAL;
    
    if (copy_from_user(input, buffer, count))
        return -EFAULT;
    
    input[count] = '\0';
    
    if (sscanf(input, "%s %d %s %d", cmd, &id, name, &priority) == 4) {
        if (strcmp(cmd, "task") == 0) {
            struct task_entry *task = kmalloc(sizeof(*task), GFP_KERNEL);
            if (task) {
                task->id = id;
                task->priority = priority;
                strncpy(task->name, name, sizeof(task->name) - 1);
                task->name[sizeof(task->name) - 1] = '\0';
                list_add_tail(&task->list, &task_list);
                pr_info("Added task: %s\n", name);
            }
        } else if (strcmp(cmd, "user") == 0) {
            struct user_entry *user = kmalloc(sizeof(*user), GFP_KERNEL);
            if (user) {
                user->uid = id;
                strncpy(user->name, name, sizeof(user->name) - 1);
                user->name[sizeof(user->name) - 1] = '\0';
                hash_add(user_table, &user->node, id);
                pr_info("Added user: %s\n", name);
            }
        } else if (strcmp(cmd, "msg") == 0) {
            struct message msg;
            msg.type = id;
            strncpy(msg.data, name, sizeof(msg.data) - 1);
            msg.data[sizeof(msg.data) - 1] = '\0';
            if (kfifo_in(&msg_queue, &msg, 1))
                pr_info("Added message: %s\n", name);
        }
    }
    
    return count;
}

static int data_open(struct inode *inode, struct file *file)
{
    return single_open(file, data_show, NULL);
}

static const struct proc_ops data_fops = {
    .proc_open = data_open,
    .proc_read = seq_read,
    .proc_write = data_write,
    .proc_release = single_release,
};

static int __init data_lab_init(void)
{
    proc_entry = proc_create("data_lab", 0666, NULL, &data_fops);
    if (!proc_entry)
        return -ENOMEM;
    
    pr_info("Data structures lab loaded\n");
    return 0;
}

static void __exit data_lab_exit(void)
{
    struct task_entry *task, *tmp_task;
    struct user_entry *user;
    struct hlist_node *tmp;
    int bkt;
    
    list_for_each_entry_safe(task, tmp_task, &task_list, list) {
        list_del(&task->list);
        kfree(task);
    }
    
    hash_for_each_safe(user_table, bkt, tmp, user, node) {
        hash_del(&user->node);
        kfree(user);
    }
    
    proc_remove(proc_entry);
    pr_info("Data structures lab unloaded\n");
}

module_init(data_lab_init);
module_exit(data_lab_exit);
```

### Testing
```bash
# Add tasks and users
echo "task 1 high_priority 10" > /proc/data_lab
echo "user 1001 alice 0" > /proc/data_lab
echo "msg 1 hello_world 0" > /proc/data_lab

# View data structures
cat /proc/data_lab
```

---

## Exercise 3: Interrupt Change Counter

### Objective
Create an interrupt monitor that tracks changes in `/proc/interrupts` and reports the difference.

### Implementation (irq_change_lab.c)
```c
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/string.h>

MODULE_LICENSE("GPL");

#define MAX_IRQS 256
static unsigned long prev_interrupts[MAX_IRQS];
static unsigned long curr_interrupts[MAX_IRQS];
static struct timer_list check_timer;
static int total_changes = 0;
static int last_check_changes = 0;
static struct proc_dir_entry *proc_entry;

static int parse_proc_interrupts(void)
{
    struct file *file;
    char *buffer;
    loff_t pos = 0;
    ssize_t ret;
    int changes = 0;
    char *line, *next_line;
    int irq;
    unsigned long count;
    
    file = filp_open("/proc/interrupts", O_RDONLY, 0);
    if (IS_ERR(file))
        return PTR_ERR(file);
    
    buffer = kmalloc(PAGE_SIZE, GFP_KERNEL);
    if (!buffer) {
        filp_close(file, NULL);
        return -ENOMEM;
    }
    
    /* Copy previous to compare */
    memcpy(prev_interrupts, curr_interrupts, sizeof(prev_interrupts));
    memset(curr_interrupts, 0, sizeof(curr_interrupts));
    
    ret = kernel_read(file, buffer, PAGE_SIZE - 1, &pos);
    if (ret > 0) {
        buffer[ret] = '\0';
        
        /* Parse each line */
        line = buffer;
        while (line && *line) {
            next_line = strchr(line, '\n');
            if (next_line) {
                *next_line = '\0';
                next_line++;
            }
            
            /* Try to parse IRQ number and first CPU count */
            if (sscanf(line, " %d: %lu", &irq, &count) == 2) {
                if (irq >= 0 && irq < MAX_IRQS) {
                    curr_interrupts[irq] = count;
                    if (count != prev_interrupts[irq]) {
                        changes++;
                    }
                }
            }
            
            line = next_line;
        }
    }
    
    kfree(buffer);
    filp_close(file, NULL);
    return changes;
}

static void check_interrupts(struct timer_list *t)
{
    last_check_changes = parse_proc_interrupts();
    if (last_check_changes > 0) {
        total_changes += last_check_changes;
        pr_info("Interrupt changes detected: %d (total: %d)\n", 
                last_check_changes, total_changes);
    }
    
    mod_timer(&check_timer, jiffies + msecs_to_jiffies(5000));
}

static int irq_change_show(struct seq_file *m, void *v)
{
    int i, active_irqs = 0;
    
    seq_printf(m, "=== Interrupt Change Monitor ===\n\n");
    seq_printf(m, "Last check changes: %d\n", last_check_changes);
    seq_printf(m, "Total changes detected: %d\n", total_changes);
    seq_printf(m, "Monitoring interval: 5 seconds\n\n");
    
    seq_printf(m, "Active IRQs:\n");
    for (i = 0; i < MAX_IRQS; i++) {
        if (curr_interrupts[i] > 0) {
            seq_printf(m, "  IRQ %d: %lu", i, curr_interrupts[i]);
            if (curr_interrupts[i] != prev_interrupts[i])
                seq_printf(m, " (changed from %lu)", prev_interrupts[i]);
            seq_printf(m, "\n");
            active_irqs++;
        }
    }
    
    if (active_irqs == 0)
        seq_printf(m, "  No active IRQs found\n");
    
    return 0;
}

static int irq_change_open(struct inode *inode, struct file *file)
{
    return single_open(file, irq_change_show, NULL);
}

static const struct proc_ops irq_change_fops = {
    .proc_open = irq_change_open,
    .proc_read = seq_read,
    .proc_release = single_release,
};

static int __init irq_change_init(void)
{
    /* Initial parse to set baseline */
    parse_proc_interrupts();
    
    timer_setup(&check_timer, check_interrupts, 0);
    mod_timer(&check_timer, jiffies + msecs_to_jiffies(1000));
    
    proc_entry = proc_create("irq_changes", 0444, NULL, &irq_change_fops);
    if (!proc_entry) {
        del_timer_sync(&check_timer);
        return -ENOMEM;
    }
    
    pr_info("IRQ change monitor loaded\n");
    return 0;
}

static void __exit irq_change_exit(void)
{
    del_timer_sync(&check_timer);
    proc_remove(proc_entry);
    pr_info("IRQ change monitor unloaded: %d total changes\n", total_changes);
}

module_init(irq_change_init);
module_exit(irq_change_exit);
```

### Testing
```bash
# Monitor interrupt changes
watch -n 1 "cat /proc/irq_changes"

# Generate activity to see changes
cat /proc/interrupts
dd if=/dev/zero of=/dev/null bs=1M count=100

# Check results
dmesg | grep "Interrupt changes"
```

---

## Lab Makefile
```makefile
obj-m += sync_lab.o
obj-m += data_lab.o  
obj-m += irq_change_lab.o

KDIR := ~/course/kernel_programming/linux

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

test: all
	@echo "=== Testing Lab Exercises ==="
	
	@echo "Loading modules..."
	sudo insmod sync_lab.ko
	sudo insmod data_lab.ko  
	sudo insmod irq_change_lab.ko
	
	@echo "Testing synchronization..."
	cat /proc/sync_lab
	echo 150 > /proc/sync_lab
	sleep 2
	cat /proc/sync_lab
	
	@echo "Testing data structures..."
	echo "task 1 high_priority 10" > /proc/data_lab
	echo "user 1001 alice 0" > /proc/data_lab
	echo "msg 1 hello_world 0" > /proc/data_lab
	cat /proc/data_lab
	
	@echo "Testing interrupt changes..."
	cat /proc/irq_changes
	
	@echo "Cleaning up..."
	sudo rmmod irq_change_lab
	sudo rmmod data_lab
	sudo rmmod sync_lab
	
	@echo "Lab test complete!"

.PHONY: all clean test
```

## Lab Instructions

### Exercise 1: Synchronization
1. Load the module: `sudo insmod sync_lab.ko`
2. Monitor the counter: `cat /proc/sync_lab`
3. Change config value: `echo 200 > /proc/sync_lab`
4. Observe thread behavior in dmesg

### Exercise 2: Data Structures
1. Load the module: `sudo insmod data_lab.ko`
2. Add data using the format: `echo "type id name priority" > /proc/data_lab`
3. View structures: `cat /proc/data_lab`
4. Test with different data types

### Exercise 3: Interrupt Monitoring
1. Load the module: `sudo insmod irq_change_lab.ko`
2. Monitor changes: `watch -n 1 "cat /proc/irq_changes"`
3. Generate activity to trigger interrupts
4. Observe the change detection

### Expected Results
- Synchronization: Counter increments, config changes are visible
- Data structures: Items are stored and displayed correctly
- Interrupts: Changes in `/proc/interrupts` are detected and reported

Each exercise demonstrates key kernel programming concepts and provides hands-on experience with the topics covered in Class 4.