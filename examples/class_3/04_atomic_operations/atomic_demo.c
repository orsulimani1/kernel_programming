#include <linux/module.h>
#include <linux/atomic.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>

static atomic_t counter = ATOMIC_INIT(0);
static atomic_t operations_count = ATOMIC_INIT(0);
static atomic64_t large_counter = ATOMIC64_INIT(0);
static unsigned long bit_flags = 0;
static atomic_t ref_count = ATOMIC_INIT(1);

static struct proc_dir_entry *proc_dir;
static struct proc_dir_entry *proc_basic, *proc_advanced, *proc_bits, *proc_status;

// Bit flag definitions
#define FLAG_SYSTEM_READY     0
#define FLAG_HIGH_LOAD        1
#define FLAG_MAINTENANCE      2
#define FLAG_DEBUG_MODE       3

// Basic atomic operations
static ssize_t atomic_basic_write(struct file *file, const char __user *buffer,
                                 size_t count, loff_t *pos)
{
    char input[32];
    char operation[16];
    int value = 0;
    int result;
    
    if (count > sizeof(input) - 1)
        return -EINVAL;
        
    if (copy_from_user(input, buffer, count))
        return -EFAULT;
        
    input[count] = '\0';
    
    // Parse command: "operation [value]"
    if (sscanf(input, "%15s %d", operation, &value) < 1)
        return -EINVAL;
    
    atomic_inc(&operations_count);
    
    if (strcmp(operation, "inc") == 0) {
        result = atomic_inc_return(&counter);
        pr_info("atomic_inc: counter = %d\n", result);
    } else if (strcmp(operation, "dec") == 0) {
        result = atomic_dec_return(&counter);
        pr_info("atomic_dec: counter = %d\n", result);
    } else if (strcmp(operation, "add") == 0) {
        result = atomic_add_return(value, &counter);
        pr_info("atomic_add(%d): counter = %d\n", value, result);
    } else if (strcmp(operation, "sub") == 0) {
        result = atomic_sub_return(value, &counter);
        pr_info("atomic_sub(%d): counter = %d\n", value, result);
    } else if (strcmp(operation, "set") == 0) {
        atomic_set(&counter, value);
        pr_info("atomic_set: counter = %d\n", value);
    } else if (strcmp(operation, "xchg") == 0) {
        result = atomic_xchg(&counter, value);
        pr_info("atomic_xchg: old = %d, new = %d\n", result, value);
    } else if (strcmp(operation, "cmpxchg") == 0) {
        int old_val = atomic_read(&counter);
        result = atomic_cmpxchg(&counter, old_val, value);
        if (result == old_val) {
            pr_info("atomic_cmpxchg: SUCCESS - changed %d to %d\n", old_val, value);
        } else {
            pr_info("atomic_cmpxchg: FAILED - expected %d, found %d\n", old_val, result);
        }
    } else {
        pr_warn("Unknown operation: %s\n", operation);
        return -EINVAL;
    }
    
    return count;
}

static int atomic_basic_show(struct seq_file *m, void *v)
{
    int counter_val = atomic_read(&counter);
    int ops_val = atomic_read(&operations_count);
    long long large_val = atomic64_read(&large_counter);
    int ref_val = atomic_read(&ref_count);
    
    seq_printf(m, "=== Basic Atomic Operations ===\n");
    seq_printf(m, "Counter: %d\n", counter_val);
    seq_printf(m, "Operations performed: %d\n", ops_val);
    seq_printf(m, "Large counter (64-bit): %lld\n", large_val);
    seq_printf(m, "Reference count: %d\n", ref_val);
    seq_printf(m, "\nSupported operations:\n");
    seq_printf(m, "  echo 'inc' > /proc/atomic_demo/basic\n");
    seq_printf(m, "  echo 'dec' > /proc/atomic_demo/basic\n");
    seq_printf(m, "  echo 'add 5' > /proc/atomic_demo/basic\n");
    seq_printf(m, "  echo 'sub 3' > /proc/atomic_demo/basic\n");
    seq_printf(m, "  echo 'set 10' > /proc/atomic_demo/basic\n");
    seq_printf(m, "  echo 'xchg 20' > /proc/atomic_demo/basic\n");
    seq_printf(m, "  echo 'cmpxchg 25' > /proc/atomic_demo/basic\n");
    
    return 0;
}

// Advanced atomic operations (test operations)
static ssize_t atomic_advanced_write(struct file *file, const char __user *buffer,
                                    size_t count, loff_t *pos)
{
    char input[32];
    char operation[16];
    int value = 0;
    bool test_result;
    
    if (count > sizeof(input) - 1)
        return -EINVAL;
        
    if (copy_from_user(input, buffer, count))
        return -EFAULT;
        
    input[count] = '\0';
    
    if (sscanf(input, "%15s %d", operation, &value) < 1)
        return -EINVAL;
    
    atomic_inc(&operations_count);
    
    if (strcmp(operation, "dec_and_test") == 0) {
        test_result = atomic_dec_and_test(&counter);
        pr_info("atomic_dec_and_test: result = %s, counter = %d\n",
                test_result ? "ZERO" : "NON-ZERO", atomic_read(&counter));
    } else if (strcmp(operation, "inc_and_test") == 0) {
        test_result = atomic_inc_and_test(&counter);
        pr_info("atomic_inc_and_test: result = %s, counter = %d\n",
                test_result ? "ZERO" : "NON-ZERO", atomic_read(&counter));
    } else if (strcmp(operation, "sub_and_test") == 0) {
        test_result = atomic_sub_and_test(value, &counter);
        pr_info("atomic_sub_and_test(%d): result = %s, counter = %d\n",
                value, test_result ? "ZERO" : "NON-ZERO", atomic_read(&counter));
    } else if (strcmp(operation, "add_negative") == 0) {
        test_result = atomic_add_negative(value, &counter);
        pr_info("atomic_add_negative(%d): result = %s, counter = %d\n",
                value, test_result ? "NEGATIVE" : "NON-NEGATIVE", atomic_read(&counter));
    } else if (strcmp(operation, "large_inc") == 0) {
        long long result = atomic64_inc_return(&large_counter);
        pr_info("atomic64_inc: large_counter = %lld\n", result);
    } else if (strcmp(operation, "large_add") == 0) {
        long long result = atomic64_add_return(value, &large_counter);
        pr_info("atomic64_add(%d): large_counter = %lld\n", value, result);
    } else if (strcmp(operation, "ref_get") == 0) {
        atomic_inc(&ref_count);
        pr_info("Reference acquired: ref_count = %d\n", atomic_read(&ref_count));
    } else if (strcmp(operation, "ref_put") == 0) {
        if (atomic_dec_and_test(&ref_count)) {
            pr_info("Reference count reached zero - would free resource\n");
            atomic_set(&ref_count, 1); // Reset for demo purposes
        } else {
            pr_info("Reference released: ref_count = %d\n", atomic_read(&ref_count));
        }
    } else {
        pr_warn("Unknown advanced operation: %s\n", operation);
        return -EINVAL;
    }
    
    return count;
}

static int atomic_advanced_show(struct seq_file *m, void *v)
{
    seq_printf(m, "=== Advanced Atomic Operations ===\n");
    seq_printf(m, "Current counter: %d\n", atomic_read(&counter));
    seq_printf(m, "Large counter: %lld\n", atomic64_read(&large_counter));
    seq_printf(m, "Reference count: %d\n", atomic_read(&ref_count));
    seq_printf(m, "\nTest operations:\n");
    seq_printf(m, "  echo 'dec_and_test' > /proc/atomic_demo/advanced\n");
    seq_printf(m, "  echo 'inc_and_test' > /proc/atomic_demo/advanced\n");
    seq_printf(m, "  echo 'sub_and_test 5' > /proc/atomic_demo/advanced\n");
    seq_printf(m, "  echo 'add_negative -3' > /proc/atomic_demo/advanced\n");
    seq_printf(m, "64-bit operations:\n");
    seq_printf(m, "  echo 'large_inc' > /proc/atomic_demo/advanced\n");
    seq_printf(m, "  echo 'large_add 1000' > /proc/atomic_demo/advanced\n");
    seq_printf(m, "Reference counting:\n");
    seq_printf(m, "  echo 'ref_get' > /proc/atomic_demo/advanced\n");
    seq_printf(m, "  echo 'ref_put' > /proc/atomic_demo/advanced\n");
    
    return 0;
}

// Bit operations
static ssize_t atomic_bits_write(struct file *file, const char __user *buffer,
                                size_t count, loff_t *pos)
{
    char input[32];
    char operation[16];
    int bit_num = 0;
    bool old_val;
    
    if (count > sizeof(input) - 1)
        return -EINVAL;
        
    if (copy_from_user(input, buffer, count))
        return -EFAULT;
        
    input[count] = '\0';
    
    if (sscanf(input, "%15s %d", operation, &bit_num) != 2)
        return -EINVAL;
    
    if (bit_num < 0 || bit_num >= BITS_PER_LONG)
        return -EINVAL;
    
    atomic_inc(&operations_count);
    
    if (strcmp(operation, "set") == 0) {
        set_bit(bit_num, &bit_flags);
        pr_info("set_bit(%d): flags = 0x%lx\n", bit_num, bit_flags);
    } else if (strcmp(operation, "clear") == 0) {
        clear_bit(bit_num, &bit_flags);
        pr_info("clear_bit(%d): flags = 0x%lx\n", bit_num, bit_flags);
    } else if (strcmp(operation, "change") == 0) {
        change_bit(bit_num, &bit_flags);
        pr_info("change_bit(%d): flags = 0x%lx\n", bit_num, bit_flags);
    } else if (strcmp(operation, "test_and_set") == 0) {
        old_val = test_and_set_bit(bit_num, &bit_flags);
        pr_info("test_and_set_bit(%d): old = %d, flags = 0x%lx\n", 
                bit_num, old_val, bit_flags);
    } else if (strcmp(operation, "test_and_clear") == 0) {
        old_val = test_and_clear_bit(bit_num, &bit_flags);
        pr_info("test_and_clear_bit(%d): old = %d, flags = 0x%lx\n", 
                bit_num, old_val, bit_flags);
    } else if (strcmp(operation, "test_and_change") == 0) {
        old_val = test_and_change_bit(bit_num, &bit_flags);
        pr_info("test_and_change_bit(%d): old = %d, flags = 0x%lx\n", 
                bit_num, old_val, bit_flags);
    } else {
        pr_warn("Unknown bit operation: %s\n", operation);
        return -EINVAL;
    }
    
    return count;
}

static int atomic_bits_show(struct seq_file *m, void *v)
{
    seq_printf(m, "=== Atomic Bit Operations ===\n");
    seq_printf(m, "Bit flags: 0x%lx (binary: ", bit_flags);
    
    // Show binary representation
    for (int i = BITS_PER_LONG - 1; i >= 0; i--) {
        if (i < 8 || (i % 4 == 3)) // Show only lower 8 bits with grouping
            seq_printf(m, "%d", test_bit(i, &bit_flags) ? 1 : 0);
        if (i > 0 && i < 8 && (i % 4 == 0))
            seq_printf(m, " ");
    }
    seq_printf(m, ")\n\n");
    
    // Show named flags
    seq_printf(m, "Named flags:\n");
    seq_printf(m, "  SYSTEM_READY:  %s (bit %d)\n", 
               test_bit(FLAG_SYSTEM_READY, &bit_flags) ? "SET" : "CLEAR", FLAG_SYSTEM_READY);
    seq_printf(m, "  HIGH_LOAD:     %s (bit %d)\n", 
               test_bit(FLAG_HIGH_LOAD, &bit_flags) ? "SET" : "CLEAR", FLAG_HIGH_LOAD);
    seq_printf(m, "  MAINTENANCE:   %s (bit %d)\n", 
               test_bit(FLAG_MAINTENANCE, &bit_flags) ? "SET" : "CLEAR", FLAG_MAINTENANCE);
    seq_printf(m, "  DEBUG_MODE:    %s (bit %d)\n", 
               test_bit(FLAG_DEBUG_MODE, &bit_flags) ? "SET" : "CLEAR", FLAG_DEBUG_MODE);
    
    seq_printf(m, "\nBit operations:\n");
    seq_printf(m, "  echo 'set 1' > /proc/atomic_demo/bits\n");
    seq_printf(m, "  echo 'clear 1' > /proc/atomic_demo/bits\n");
    seq_printf(m, "  echo 'change 2' > /proc/atomic_demo/bits\n");
    seq_printf(m, "  echo 'test_and_set 3' > /proc/atomic_demo/bits\n");
    seq_printf(m, "  echo 'test_and_clear 3' > /proc/atomic_demo/bits\n");
    seq_printf(m, "  echo 'test_and_change 0' > /proc/atomic_demo/bits\n");
    
    return 0;
}

// Status overview
static int atomic_status_show(struct seq_file *m, void *v)
{
    seq_printf(m, "=== Atomic Operations Status ===\n");
    seq_printf(m, "32-bit counter: %d\n", atomic_read(&counter));
    seq_printf(m, "64-bit counter: %lld\n", atomic64_read(&large_counter));
    seq_printf(m, "Operations count: %d\n", atomic_read(&operations_count));
    seq_printf(m, "Reference count: %d\n", atomic_read(&ref_count));
    seq_printf(m, "Bit flags: 0x%lx\n", bit_flags);
    seq_printf(m, "\nMemory ordering guarantees:\n");
    seq_printf(m, "  All atomic operations provide memory barriers\n");
    seq_printf(m, "  Safe for SMP systems\n");
    seq_printf(m, "  No lost updates under high contention\n");
    seq_printf(m, "\nPerformance characteristics:\n");
    seq_printf(m, "  Fastest: simple inc/dec operations\n");
    seq_printf(m, "  Moderate: read-modify-write operations\n");
    seq_printf(m, "  Consider per-CPU variables for high contention\n");
    
    return 0;
}

// File operation wrappers
DEFINE_PROC_SHOW_ATTRIBUTE(atomic_basic);
DEFINE_PROC_SHOW_ATTRIBUTE(atomic_advanced);
DEFINE_PROC_SHOW_ATTRIBUTE(atomic_bits);
DEFINE_PROC_SHOW_ATTRIBUTE(atomic_status);

static const struct proc_ops basic_fops = {
    .proc_open = atomic_basic_open,
    .proc_read = seq_read,
    .proc_write = atomic_basic_write,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static const struct proc_ops advanced_fops = {
    .proc_open = atomic_advanced_open,
    .proc_read = seq_read,
    .proc_write = atomic_advanced_write,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static const struct proc_ops bits_fops = {
    .proc_open = atomic_bits_open,
    .proc_read = seq_read,
    .proc_write = atomic_bits_write,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static const struct proc_ops status_fops = {
    .proc_open = atomic_status_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static int __init atomic_comprehensive_init(void)
{
    // Set initial system ready flag
    set_bit(FLAG_SYSTEM_READY, &bit_flags);
    
    // Create proc directory
    proc_dir = proc_mkdir("atomic_demo", NULL);
    if (!proc_dir)
        return -ENOMEM;
    
    proc_basic = proc_create("basic", 0666, proc_dir, &basic_fops);
    proc_advanced = proc_create("advanced", 0666, proc_dir, &advanced_fops);
    proc_bits = proc_create("bits", 0666, proc_dir, &bits_fops);
    proc_status = proc_create("status", 0444, proc_dir, &status_fops);
    
    if (!proc_basic || !proc_advanced || !proc_bits || !proc_status) {
        proc_remove(proc_dir);
        return -ENOMEM;
    }
    
    pr_info("Comprehensive atomic operations demo loaded\n");
    pr_info("Available interfaces:\n");
    pr_info("  /proc/atomic_demo/basic    - Basic atomic operations\n");
    pr_info("  /proc/atomic_demo/advanced - Advanced atomic operations\n");
    pr_info("  /proc/atomic_demo/bits     - Atomic bit operations\n");
    pr_info("  /proc/atomic_demo/status   - Status overview\n");
    
    return 0;
}

static void __exit atomic_comprehensive_exit(void)
{
    proc_remove(proc_dir);
    
    pr_info("Atomic operations demo unloaded\n");
    pr_info("Final values:\n");
    pr_info("  Counter: %d\n", atomic_read(&counter));
    pr_info("  Large counter: %lld\n", atomic64_read(&large_counter));
    pr_info("  Operations: %d\n", atomic_read(&operations_count));
    pr_info("  Bit flags: 0x%lx\n", bit_flags);
}

module_init(atomic_comprehensive_init);
module_exit(atomic_comprehensive_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kernel Course");
MODULE_DESCRIPTION("Comprehensive atomic operations demonstration");