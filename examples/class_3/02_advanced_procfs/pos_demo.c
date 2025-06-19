#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/jiffies.h>

#define MAX_LOG_ENTRIES 100
#define PROC_NAME "virtual_log"

struct log_entry {
   unsigned long timestamp;
   int entry_id;
   char message[64];
};

static struct log_entry log_buffer[MAX_LOG_ENTRIES];
static int log_count = 0;
static int next_id = 1;
static struct proc_dir_entry *proc_entry;

// Add a log entry (simulate system activity)
static void add_log_entry(const char *message)
{
   if (log_count < MAX_LOG_ENTRIES) {
       log_buffer[log_count].timestamp = jiffies;
       log_buffer[log_count].entry_id = next_id++;
       strncpy(log_buffer[log_count].message, message, 
               sizeof(log_buffer[log_count].message) - 1);
       log_buffer[log_count].message[sizeof(log_buffer[log_count].message) - 1] = '\0';
       log_count++;
   } else {
       // Circular buffer: overwrite oldest entry
       memmove(&log_buffer[0], &log_buffer[1], 
               sizeof(struct log_entry) * (MAX_LOG_ENTRIES - 1));
       log_buffer[MAX_LOG_ENTRIES - 1].timestamp = jiffies;
       log_buffer[MAX_LOG_ENTRIES - 1].entry_id = next_id++;
       strncpy(log_buffer[MAX_LOG_ENTRIES - 1].message, message,
               sizeof(log_buffer[MAX_LOG_ENTRIES - 1].message) - 1);
       log_buffer[MAX_LOG_ENTRIES - 1].message[sizeof(log_buffer[MAX_LOG_ENTRIES - 1].message) - 1] = '\0';
   }
}

// seq_file operations demonstrating *pos usage
static void *log_seq_start(struct seq_file *m, loff_t *pos)
{
   pr_info("log_seq_start called with pos=%lld, log_count=%d\n", *pos, log_count);
   
   // *pos represents which log entry the user wants to start reading from
   if (*pos >= log_count) {
       pr_info("  pos beyond log_count, returning NULL (EOF)\n");
       return NULL;  // EOF - position beyond available data
   }
   
   pr_info("  returning log entry at index %lld\n", *pos);
   return &log_buffer[*pos];  // Return pointer to log entry at position *pos
}

static void *log_seq_next(struct seq_file *m, void *v, loff_t *pos)
{
   pr_info("log_seq_next called with pos=%lld\n", *pos);
   
   // Increment position to next entry
   (*pos)++;
   
   pr_info("  incremented pos to %lld\n", *pos);
   
   // Check if we've reached the end
   if (*pos >= log_count) {
       pr_info("  reached end of log, returning NULL\n");
       return NULL;  // EOF
   }
   
   pr_info("  returning log entry at index %lld\n", *pos);
   return &log_buffer[*pos];  // Return next log entry
}

static void log_seq_stop(struct seq_file *m, void *v)
{
   pr_info("log_seq_stop called\n");
   // Nothing to cleanup in this example
}

static int log_seq_show(struct seq_file *m, void *v)
{
   struct log_entry *entry = (struct log_entry *)v;
   
   pr_info("log_seq_show called for entry_id=%d\n", entry->entry_id);
   
   // Format and output the log entry
   seq_printf(m, "[%lu] Entry #%d: %s\n", 
              entry->timestamp, entry->entry_id, entry->message);
   
   return 0;
}

// seq_file operations structure
static const struct seq_operations log_seq_ops = {
   .start = log_seq_start,
   .next  = log_seq_next,
   .stop  = log_seq_stop,
   .show  = log_seq_show,
};

// Open function for procfs
static int log_proc_open(struct inode *inode, struct file *file)
{
   pr_info("log_proc_open called\n");
   return seq_open(file, &log_seq_ops);
}

// Write function to add new log entries
static ssize_t log_proc_write(struct file *file, const char __user *buffer,
                            size_t count, loff_t *pos)
{
   char *kernel_buffer;
   
   if (count > 63)  // Max message size - 1 for null terminator
       return -EINVAL;
       
   kernel_buffer = kmalloc(count + 1, GFP_KERNEL);
   if (!kernel_buffer)
       return -ENOMEM;
       
   if (copy_from_user(kernel_buffer, buffer, count)) {
       kfree(kernel_buffer);
       return -EFAULT;
   }
   
   kernel_buffer[count] = '\0';
   
   // Remove newline if present
   if (kernel_buffer[count - 1] == '\n')
       kernel_buffer[count - 1] = '\0';
   
   add_log_entry(kernel_buffer);
   pr_info("Added log entry: %s\n", kernel_buffer);
   
   kfree(kernel_buffer);
   return count;
}

// procfs file operations
static const struct proc_ops log_proc_ops = {
   .proc_open    = log_proc_open,
   .proc_read    = seq_read,      // Use seq_file's read function
   .proc_write   = log_proc_write,
   .proc_lseek   = seq_lseek,     // Use seq_file's seek function
   .proc_release = seq_release,   // Use seq_file's release function
};

static int __init pos_demo_init(void)
{
   // Create some initial log entries
   add_log_entry("System started");
   add_log_entry("Module loading");
   add_log_entry("Initialization complete");
   add_log_entry("Ready for operations");
   add_log_entry("First user access");
   
   // Create procfs entry
   proc_entry = proc_create(PROC_NAME, 0666, NULL, &log_proc_ops);
   if (!proc_entry) {
       pr_err("Failed to create /proc/%s\n", PROC_NAME);
       return -ENOMEM;
   }
   
   pr_info("Position demo module loaded\n");
   pr_info("Usage:\n");
   pr_info("  Read log: cat /proc/%s\n", PROC_NAME);
   pr_info("  Add entry: echo 'message' > /proc/%s\n", PROC_NAME);
   pr_info("  Partial read: dd if=/proc/%s bs=1 count=50\n", PROC_NAME);
   pr_info("  Seek and read: dd if=/proc/%s bs=1 skip=100 count=50\n", PROC_NAME);
   
   return 0;
}

static void __exit pos_demo_exit(void)
{
   proc_remove(proc_entry);
   pr_info("Position demo module unloaded\n");
}

module_init(pos_demo_init);
module_exit(pos_demo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kernel Course");
MODULE_DESCRIPTION("Demonstration of loff_t *pos usage in seq_file");