#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

static int example = 0;
module_param(example, int, 0644);
MODULE_PARM_DESC(example, "Example number to run (0-7)");

// Example 0: CPUID
static void example_cpuid(void)
{
    unsigned int eax, ebx, ecx, edx;
    
    asm volatile ("cpuid"
                 : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                 : "a"(0));
    
    pr_info("CPUID Example - CPU Vendor: %.4s%.4s%.4s\n", 
           (char*)&ebx, (char*)&edx, (char*)&ecx);
}

// Example 1: RDTSC
static void example_rdtsc(void)
{
    unsigned long low, high;
    unsigned long long tsc;
    
    asm volatile ("rdtsc" : "=a"(low), "=d"(high));
    tsc = ((unsigned long long)high << 32) | low;
    
    pr_info("RDTSC Example - TSC: %llu\n", tsc);
}

// Example 2: Simple register operations
static void example_register_ops(void)
{
    unsigned long result;
    unsigned long input = 42;
    
    asm volatile ("movq %1, %%rax\n\t"
                 "addq $10, %%rax\n\t"
                 "movq %%rax, %0"
                 : "=m"(result)
                 : "m"(input)
                 : "rax");
    
    pr_info("Register Ops Example - Input: %lu, Result: %lu\n", input, result);
}

// Example 3: Multiple commands
static void example_multi_commands(void)
{
    unsigned long rax, rbx, rcx;
    
    asm volatile ("movq $100, %%rax\n\t"
                 "movq $200, %%rbx\n\t"
                 "addq %%rbx, %%rax\n\t"
                 "movq %%rax, %0\n\t"
                 "movq %%rbx, %1\n\t"
                 "movq $300, %%rcx\n\t"
                 "movq %%rcx, %2"
                 : "=m"(rax), "=m"(rbx), "=m"(rcx)
                 :
                 : "rax", "rbx", "rcx");
    
    pr_info("Multi Commands Example - Results: rax=%lu, rbx=%lu, rcx=%lu\n", 
           rax, rbx, rcx);
}

// Example 4: Reading parameters
static void example_read_params(void)
{
    unsigned long input1 = 42;
    unsigned long input2 = 84;
    unsigned long result;
    
    asm volatile ("movq %1, %%rax\n\t"
                 "movq %2, %%rbx\n\t"
                 "addq %%rbx, %%rax\n\t"
                 "movq %%rax, %0"
                 : "=m"(result)
                 : "m"(input1), "m"(input2)
                 : "rax", "rbx");
    
    pr_info("Read Params Example - Sum of %lu + %lu = %lu\n", 
           input1, input2, result);
}

// Example 5: Writing parameters
static void example_write_params(void)
{
    unsigned long output1, output2;
    
    asm volatile ("movq $123, %%rax\n\t"
                 "movq $456, %%rbx\n\t"
                 "movq %%rax, %0\n\t"
                 "movq %%rbx, %1"
                 : "=m"(output1), "=m"(output2)
                 :
                 : "rax", "rbx");
    
    pr_info("Write Params Example - Outputs: %lu, %lu\n", output1, output2);
}

// Example 6: Address usage
static void example_address_usage(void)
{
    unsigned long array[3] = {10, 20, 30};
    unsigned long sum;
    
    asm volatile ("movq %1, %%rsi\n\t"
                 "movq 0(%%rsi), %%rax\n\t"
                 "addq 8(%%rsi), %%rax\n\t"
                 "addq 16(%%rsi), %%rax\n\t"
                 "movq %%rax, %0"
                 : "=m"(sum)
                 : "m"(array)
                 : "rax", "rsi");
    
    pr_info("Address Usage Example - Array sum: %lu\n", sum);
}

// Example 7: Variable swap
static void example_swap_variables(void)
{
    unsigned long x = 42;
    unsigned long y = 84;
    
    pr_info("Swap Example - Before: x=%lu, y=%lu\n", x, y);
    
    asm volatile ("movq %2, %%rax\n\t"      // Load &x into RAX
                 "movq %3, %%rbx\n\t"      // Load &y into RBX  
                 "movq (%%rax), %%rcx\n\t" // Load value of x
                 "movq (%%rbx), %%rdx\n\t" // Load value of y
                 "movq %%rdx, (%%rax)\n\t" // Store y into x
                 "movq %%rcx, (%%rbx)"     // Store x into y
                 : "=m"(x), "=m"(y)        // Outputs
                 : "r"(&x), "r"(&y)        // Inputs (use "r" for addresses)
                 : "rax", "rbx", "rcx", "rdx", "memory");
    
    pr_info("Swap Example - After: x=%lu, y=%lu\n", x, y);
}

static int __init inline_asm_init(void)
{
    pr_info("Inline Assembly Examples Module Loaded\n");
    pr_info("Running example %d\n", example);
    
    switch (example) {
    case 0:
        example_cpuid();
        break;
    case 1:
        example_rdtsc();
        break;
    case 2:
        example_register_ops();
        break;
    case 3:
        example_multi_commands();
        break;
    case 4:
        example_read_params();
        break;
    case 5:
        example_write_params();
        break;
    case 6:
        example_address_usage();
        break;
    case 7:
        example_swap_variables();
        break;
    default:
        pr_warn("Invalid example number %d (valid: 0-7)\n", example);
        pr_info("Available examples:\n");
        pr_info("  0: CPUID\n");
        pr_info("  1: RDTSC\n");
        pr_info("  2: Register Operations\n");
        pr_info("  3: Multiple Commands\n");
        pr_info("  4: Reading Parameters\n");
        pr_info("  5: Writing Parameters\n");
        pr_info("  6: Address Usage\n");
        pr_info("  7: Variable Swap\n");
        return -EINVAL;
    }
    
    return 0;
}

static void __exit inline_asm_exit(void)
{
    pr_info("Inline Assembly Examples Module Unloaded\n");
}

module_init(inline_asm_init);
module_exit(inline_asm_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kernel Programming Course");
MODULE_DESCRIPTION("Unified inline assembly examples");
MODULE_VERSION("1.0");