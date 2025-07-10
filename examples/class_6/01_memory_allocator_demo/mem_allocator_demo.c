/* * 
 * Memory Allocator Comparison Demo
 * Demonstrates performance and behavior differences between kmalloc, vmalloc, and slab allocator
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/timekeeping.h>
#include <linux/uaccess.h>

#define PROC_NAME "mem_allocator_demo"
#define TEST_ITERATIONS 1000
#define SMALL_SIZE 256
#define MEDIUM_SIZE (4 * 1024)
#define LARGE_SIZE (128 * 1024)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Linux Kernel Programming Course");
MODULE_DESCRIPTION("Memory allocator performance comparison demo");
MODULE_VERSION("1.0");

/* Custom slab cache for demonstration */
static struct kmem_cache *demo_cache;

/* Test results structure */
struct alloc_test_result {
    u64 kmalloc_time_ns;
    u64 vmalloc_time_ns;
    u64 slab_time_ns;
    int kmalloc_failures;
    int vmalloc_failures;
    int slab_failures;
    size_t test_size;
};

static struct alloc_test_result small_test_results;
static struct alloc_test_result medium_test_results;
static struct alloc_test_result large_test_results;

/* Test structure for slab allocator */
struct demo_object {
    int id;
    char data[SMALL_SIZE - sizeof(int)];
};

static u64 get_time_ns(void)
{
    return ktime_get_ns();
}

/* kmalloc performance test */
static void test_kmalloc_performance(struct alloc_test_result *result, size_t size)
{
    void *ptrs[TEST_ITERATIONS];
    u64 start_time, end_time;
    int i, failures = 0;

    start_time = get_time_ns();
    
    for (i = 0; i < TEST_ITERATIONS; i++) {
        ptrs[i] = kmalloc(size, GFP_KERNEL);
        if (!ptrs[i]) {
            failures++;
        }
    }
    
    /* Free allocated memory */
    for (i = 0; i < TEST_ITERATIONS; i++) {
        if (ptrs[i]) {
            kfree(ptrs[i]);
        }
    }
    
    end_time = get_time_ns();
    
    result->kmalloc_time_ns = end_time - start_time;
    result->kmalloc_failures = failures;
}

/* vmalloc performance test */
static void test_vmalloc_performance(struct alloc_test_result *result, size_t size)
{
    void *ptrs[TEST_ITERATIONS];
    u64 start_time, end_time;
    int i, failures = 0;

    start_time = get_time_ns();
    
    for (i = 0; i < TEST_ITERATIONS; i++) {
        ptrs[i] = vmalloc(size);
        if (!ptrs[i]) {
            failures++;
        }
    }
    
    /* Free allocated memory */
    for (i = 0; i < TEST_ITERATIONS; i++) {
        if (ptrs[i]) {
            vfree(ptrs[i]);
        }
    }
    
    end_time = get_time_ns();
    
    result->vmalloc_time_ns = end_time - start_time;
    result->vmalloc_failures = failures;
}

/* Slab allocator performance test */
static void test_slab_performance(struct alloc_test_result *result)
{
    struct demo_object *ptrs[TEST_ITERATIONS];
    u64 start_time, end_time;
    int i, failures = 0;

    start_time = get_time_ns();
    
    for (i = 0; i < TEST_ITERATIONS; i++) {
        ptrs[i] = kmem_cache_alloc(demo_cache, GFP_KERNEL);
        if (!ptrs[i]) {
            failures++;
        } else {
            /* Initialize object to simulate real usage */
            ptrs[i]->id = i;
            memset(ptrs[i]->data, 0xAA, sizeof(ptrs[i]->data));
        }
    }
    
    /* Free allocated objects */
    for (i = 0; i < TEST_ITERATIONS; i++) {
        if (ptrs[i]) {
            kmem_cache_free(demo_cache, ptrs[i]);
        }
    }
    
    end_time = get_time_ns();
    
    result->slab_time_ns = end_time - start_time;
    result->slab_failures = failures;
}

/* Run all performance tests */
static void run_performance_tests(void)
{
    pr_info("mem_allocator_demo: Starting performance tests...\n");
    
    /* Small allocation tests */
    small_test_results.test_size = SMALL_SIZE;
    test_kmalloc_performance(&small_test_results, SMALL_SIZE);
    test_vmalloc_performance(&small_test_results, SMALL_SIZE);
    test_slab_performance(&small_test_results);
    
    /* Medium allocation tests */
    medium_test_results.test_size = MEDIUM_SIZE;
    test_kmalloc_performance(&medium_test_results, MEDIUM_SIZE);
    test_vmalloc_performance(&medium_test_results, MEDIUM_SIZE);
    /* Slab cache is fixed size, so we don't test medium/large with slab */
    medium_test_results.slab_time_ns = 0;
    medium_test_results.slab_failures = 0;
    
    /* Large allocation tests */
    large_test_results.test_size = LARGE_SIZE;
    test_kmalloc_performance(&large_test_results, LARGE_SIZE);
    test_vmalloc_performance(&large_test_results, LARGE_SIZE);
    large_test_results.slab_time_ns = 0;
    large_test_results.slab_failures = 0;
    
    pr_info("mem_allocator_demo: Performance tests completed\n");
}

/* Memory fragmentation test */
static void test_memory_fragmentation(void)
{
    void *large_ptrs[10];
    void *small_ptrs[100];
    int i;
    
    pr_info("mem_allocator_demo: Testing memory fragmentation...\n");
    
    /* Allocate large chunks to create fragmentation */
    for (i = 0; i < 10; i++) {
        large_ptrs[i] = kmalloc(64 * 1024, GFP_KERNEL);
        if (!large_ptrs[i]) {
            pr_warn("Failed to allocate large chunk %d\n", i);
        }
    }
    
    /* Free every other large chunk to create holes */
    for (i = 0; i < 10; i += 2) {
        if (large_ptrs[i]) {
            kfree(large_ptrs[i]);
            large_ptrs[i] = NULL;
        }
    }
    
    /* Try to allocate medium-sized chunks (should fit in holes) */
    for (i = 0; i < 100; i++) {
        small_ptrs[i] = kmalloc(1024, GFP_KERNEL);
        if (!small_ptrs[i]) {
            pr_warn("Failed to allocate small chunk %d after fragmentation\n", i);
        }
    }
    
    /* Clean up */
    for (i = 0; i < 10; i++) {
        if (large_ptrs[i]) {
            kfree(large_ptrs[i]);
        }
    }
    
    for (i = 0; i < 100; i++) {
        if (small_ptrs[i]) {
            kfree(small_ptrs[i]);
        }
    }
    
    pr_info("mem_allocator_demo: Fragmentation test completed\n");
}

/* Failure scenario test */
static void test_allocation_failures(void)
{
    void *ptrs[1000];
    int i, allocated = 0;
    
    pr_info("mem_allocator_demo: Testing allocation failure scenarios...\n");
    
    /* Try to allocate many large chunks until failure */
    for (i = 0; i < 1000; i++) {
        ptrs[i] = kmalloc(1024 * 1024, GFP_KERNEL | __GFP_NOWARN);
        if (ptrs[i]) {
            allocated++;
        } else {
            break;
        }
    }
    
    pr_info("Successfully allocated %d MB before failure\n", allocated);
    
    /* Clean up */
    for (i = 0; i < allocated; i++) {
        kfree(ptrs[i]);
    }
    
    pr_info("mem_allocator_demo: Failure test completed\n");
}

/* Proc file show function */
static int mem_demo_show(struct seq_file *m, void *v)
{
    seq_printf(m, "Memory Allocator Performance Comparison\n");
    seq_printf(m, "========================================\n\n");
    
    seq_printf(m, "Test Parameters:\n");
    seq_printf(m, "- Iterations per test: %d\n", TEST_ITERATIONS);
    seq_printf(m, "- Small size: %d bytes\n", SMALL_SIZE);
    seq_printf(m, "- Medium size: %d bytes\n", MEDIUM_SIZE);
    seq_printf(m, "- Large size: %d bytes\n\n", LARGE_SIZE);
    
    /* Small allocation results */
    seq_printf(m, "Small Allocations (%zu bytes):\n", small_test_results.test_size);
    seq_printf(m, "  kmalloc: %llu ns (failures: %d)\n", 
               small_test_results.kmalloc_time_ns, small_test_results.kmalloc_failures);
    seq_printf(m, "  vmalloc: %llu ns (failures: %d)\n", 
               small_test_results.vmalloc_time_ns, small_test_results.vmalloc_failures);
    seq_printf(m, "  slab:    %llu ns (failures: %d)\n\n", 
               small_test_results.slab_time_ns, small_test_results.slab_failures);
    
    /* Medium allocation results */
    seq_printf(m, "Medium Allocations (%zu bytes):\n", medium_test_results.test_size);
    seq_printf(m, "  kmalloc: %llu ns (failures: %d)\n", 
               medium_test_results.kmalloc_time_ns, medium_test_results.kmalloc_failures);
    seq_printf(m, "  vmalloc: %llu ns (failures: %d)\n\n", 
               medium_test_results.vmalloc_time_ns, medium_test_results.vmalloc_failures);
    
    /* Large allocation results */
    seq_printf(m, "Large Allocations (%zu bytes):\n", large_test_results.test_size);
    seq_printf(m, "  kmalloc: %llu ns (failures: %d)\n", 
               large_test_results.kmalloc_time_ns, large_test_results.kmalloc_failures);
    seq_printf(m, "  vmalloc: %llu ns (failures: %d)\n\n", 
               large_test_results.vmalloc_time_ns, large_test_results.vmalloc_failures);
    
    /* Performance analysis */
    if (small_test_results.kmalloc_time_ns && small_test_results.vmalloc_time_ns) {
        u64 ratio = small_test_results.vmalloc_time_ns / small_test_results.kmalloc_time_ns;
        seq_printf(m, "Performance Analysis (Small Allocations):\n");
        seq_printf(m, "  vmalloc is %llu times slower than kmalloc\n", ratio);
        
        if (small_test_results.slab_time_ns) {
            ratio = small_test_results.kmalloc_time_ns / small_test_results.slab_time_ns;
            seq_printf(m, "  slab is %llu times faster than kmalloc\n", ratio);
        }
        seq_printf(m, "\n");
    }
    
    seq_printf(m, "Recommendations:\n");
    seq_printf(m, "- Use kmalloc for small-medium allocations (< 128KB)\n");
    seq_printf(m, "- Use vmalloc for large allocations (> 128KB)\n");
    seq_printf(m, "- Use slab cache for frequently allocated fixed-size objects\n");
    seq_printf(m, "- Always check for allocation failures\n");
    
    return 0;
}

static int mem_demo_open(struct inode *inode, struct file *file)
{
    return single_open(file, mem_demo_show, NULL);
}

static ssize_t mem_demo_write(struct file *file, const char __user *buffer, 
                              size_t count, loff_t *pos)
{
    char cmd[16];
    
    if (count >= sizeof(cmd))
        return -EINVAL;
    
    if (copy_from_user(cmd, buffer, count))
        return -EFAULT;
    
    cmd[count] = '\0';
    
    if (strncmp(cmd, "test", 4) == 0) {
        run_performance_tests();
    } else if (strncmp(cmd, "frag", 4) == 0) {
        test_memory_fragmentation();
    } else if (strncmp(cmd, "fail", 4) == 0) {
        test_allocation_failures();
    } else {
        pr_info("Available commands: test, frag, fail\n");
    }
    
    return count;
}

static const struct proc_ops mem_demo_proc_ops = {
    .proc_open = mem_demo_open,
    .proc_read = seq_read,
    .proc_write = mem_demo_write,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static int __init mem_allocator_demo_init(void)
{
    struct proc_dir_entry *proc_entry;
    
    pr_info("mem_allocator_demo: Module loaded\n");
    
    /* Create slab cache */
    demo_cache = kmem_cache_create("demo_object_cache",
                                   sizeof(struct demo_object),
                                   0,
                                   SLAB_HWCACHE_ALIGN,
                                   NULL);
    if (!demo_cache) {
        pr_err("Failed to create slab cache\n");
        return -ENOMEM;
    }
    
    /* Create proc entry */
    proc_entry = proc_create(PROC_NAME, 0666, NULL, &mem_demo_proc_ops);
    if (!proc_entry) {
        pr_err("Failed to create proc entry\n");
        kmem_cache_destroy(demo_cache);
        return -ENOMEM;
    }
    
    /* Run initial tests */
    run_performance_tests();
    
    pr_info("mem_allocator_demo: Use 'cat /proc/%s' to view results\n", PROC_NAME);
    pr_info("mem_allocator_demo: Echo 'test', 'frag', or 'fail' to /proc/%s to run tests\n", PROC_NAME);
    
    return 0;
}

static void __exit mem_allocator_demo_exit(void)
{
    remove_proc_entry(PROC_NAME, NULL);
    
    if (demo_cache) {
        kmem_cache_destroy(demo_cache);
    }
    
    pr_info("mem_allocator_demo: Module unloaded\n");
}

module_init(mem_allocator_demo_init);
module_exit(mem_allocator_demo_exit);