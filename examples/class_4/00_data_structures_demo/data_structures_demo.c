#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/kfifo.h>
#include <linux/hashtable.h>
#include <linux/rbtree.h>
#include <linux/slab.h>
#include <linux/spinlock.h>


/* Linked List - Task Manager */
struct task_item {
    int priority;
    char name[32];
    struct list_head list;
};

static LIST_HEAD(task_list);
static DEFINE_SPINLOCK(tasks_lock);  // Renamed to avoid conflict

/* kfifo Queue - Message System */
#define QUEUE_SIZE 32
struct message {
    int id;
    char data[64];
};
static DEFINE_KFIFO(msg_queue, struct message, QUEUE_SIZE);
static DEFINE_SPINLOCK(queue_lock);

/* Hash Table - User Management */
#define USER_HASH_BITS 4  // Renamed to avoid conflict
struct user_entry {
    int user_id;
    char name[32];
    struct hlist_node hash_node;
};
static DEFINE_HASHTABLE(user_table, USER_HASH_BITS);
static DEFINE_SPINLOCK(hash_lock);

/* Red-Black Tree - Interval Management */
struct interval_node {
    unsigned long start;
    unsigned long end;
    struct rb_node rb_node;
};
static struct rb_root interval_tree = RB_ROOT;
static DEFINE_SPINLOCK(tree_lock);

/* Linked List Operations */
static int add_task(int priority, const char *name)
{
    struct task_item *new_task;
    
    new_task = kmalloc(sizeof(*new_task), GFP_KERNEL);
    if (!new_task)
        return -ENOMEM;
    
    new_task->priority = priority;
    strncpy(new_task->name, name, sizeof(new_task->name) - 1);
    new_task->name[sizeof(new_task->name) - 1] = '\0';
    
    spin_lock(&tasks_lock);
    list_add_tail(&new_task->list, &task_list);
    spin_unlock(&tasks_lock);
    
    pr_info("Added task: %s (priority %d)\n", name, priority);
    return 0;
}

/* Queue Operations */
static int send_message(int id, const char *data)
{
    struct message msg;
    int ret;
    
    msg.id = id;
    strncpy(msg.data, data, sizeof(msg.data) - 1);
    msg.data[sizeof(msg.data) - 1] = '\0';
    
    spin_lock(&queue_lock);
    ret = kfifo_in(&msg_queue, &msg, 1);
    spin_unlock(&queue_lock);
    
    if (ret == 1) {
        pr_info("Queued message %d: %s\n", id, data);
        return 0;
    }
    return -ENOSPC;
}

/* Hash Table Operations */
static int add_user(int user_id, const char *name)
{
    struct user_entry *entry;
    
    entry = kmalloc(sizeof(*entry), GFP_KERNEL);
    if (!entry)
        return -ENOMEM;
    
    entry->user_id = user_id;
    strncpy(entry->name, name, sizeof(entry->name) - 1);
    entry->name[sizeof(entry->name) - 1] = '\0';
    
    spin_lock(&hash_lock);
    hash_add(user_table, &entry->hash_node, user_id);
    spin_unlock(&hash_lock);
    
    pr_info("Added user %d: %s\n", user_id, name);
    return 0;
}

/* Red-Black Tree Operations */
static int insert_interval(unsigned long start, unsigned long end)
{
    struct interval_node *new_node, *curr_node;  // Renamed from 'current'
    struct rb_node **link = &interval_tree.rb_node;
    struct rb_node *parent = NULL;
    
    new_node = kmalloc(sizeof(*new_node), GFP_KERNEL);
    if (!new_node)
        return -ENOMEM;
    
    new_node->start = start;
    new_node->end = end;
    
    spin_lock(&tree_lock);
    
    while (*link) {
        parent = *link;
        curr_node = rb_entry(parent, struct interval_node, rb_node);
        
        if (start < curr_node->start)
            link = &(*link)->rb_left;
        else if (start > curr_node->start)
            link = &(*link)->rb_right;
        else {
            spin_unlock(&tree_lock);
            kfree(new_node);
            return -EEXIST;
        }
    }
    
    rb_link_node(&new_node->rb_node, parent, link);
    rb_insert_color(&new_node->rb_node, &interval_tree);
    spin_unlock(&tree_lock);
    
    pr_info("Inserted interval [%lu, %lu]\n", start, end);
    return 0;
}

static struct proc_dir_entry *proc_entry;

/* Proc interface */
static int data_structures_show(struct seq_file *m, void *v)
{
    struct task_item *task;
    struct user_entry *user;
    struct interval_node *interval;
    struct rb_node *node;
    int bkt;
    
    seq_printf(m, "=== Data Structures Demo ===\n\n");
    
    /* Show tasks */
    seq_printf(m, "Tasks:\n");
    spin_lock(&tasks_lock);
    list_for_each_entry(task, &task_list, list) {
        seq_printf(m, "  %s (priority %d)\n", task->name, task->priority);
    }
    spin_unlock(&tasks_lock);
    
    /* Show queue status */
    seq_printf(m, "\nQueue: %u/%u used\n", 
               kfifo_len(&msg_queue), kfifo_size(&msg_queue));
    
    /* Show users */
    seq_printf(m, "\nUsers:\n");
    spin_lock(&hash_lock);
    hash_for_each(user_table, bkt, user, hash_node) {
        seq_printf(m, "  %d: %s\n", user->user_id, user->name);
    }
    spin_unlock(&hash_lock);
    
    /* Show intervals */
    seq_printf(m, "\nIntervals:\n");
    spin_lock(&tree_lock);
    for (node = rb_first(&interval_tree); node; node = rb_next(node)) {
        interval = rb_entry(node, struct interval_node, rb_node);
        seq_printf(m, "  [%lu, %lu]\n", interval->start, interval->end);
    }
    spin_unlock(&tree_lock);
    
    return 0;
}

static int data_structures_open(struct inode *inode, struct file *file)
{
    return single_open(file, data_structures_show, NULL);
}

static const struct proc_ops data_structures_fops = {
    .proc_open = data_structures_open,
    .proc_read = seq_read,
    .proc_release = single_release,
};

static int __init data_structures_init(void)
{
    /* Test data */
    add_task(10, "high_priority");
    add_task(5, "normal_task");
    send_message(1, "Hello");
    send_message(2, "World");
    add_user(1001, "alice");
    add_user(1002, "bob");
    insert_interval(100, 200);
    insert_interval(300, 400);
    
    proc_entry = proc_create("data_structures", 0444, NULL, &data_structures_fops);
    if (!proc_entry) {
        pr_err("Failed to create proc entry\n");
        return -ENOMEM;
    }
    
    pr_info("Data structures demo loaded\n");
    return 0;
}

static void __exit data_structures_exit(void)
{
    struct task_item *task, *tmp_task;
    struct user_entry *user;
    struct interval_node *interval;
    struct rb_node *node;
    struct hlist_node *tmp_hash;
    int bkt;
    
    proc_remove(proc_entry);  // Fixed function name
    
    /* Cleanup lists */
    list_for_each_entry_safe(task, tmp_task, &task_list, list) {
        list_del(&task->list);
        kfree(task);
    }
    
    /* Cleanup hash table */
    hash_for_each_safe(user_table, bkt, tmp_hash, user, hash_node) {
        hash_del(&user->hash_node);
        kfree(user);
    }
    
    /* Cleanup tree */
    while ((node = rb_first(&interval_tree))) {
        interval = rb_entry(node, struct interval_node, rb_node);
        rb_erase(node, &interval_tree);
        kfree(interval);
    }
    
    pr_info("Data structures demo unloaded\n");
}


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Combined Data Structures Demo");

module_init(data_structures_init);
module_exit(data_structures_exit);