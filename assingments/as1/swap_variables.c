#include <linux/module.h>
#include <linux/kernel.h>

static void swap_asm(unsigned long *a, unsigned long *b)
{
    // TODO: Implement using inline assembly
    // Requirements:
    // 1. Use addresses of variables
    // 2. Swap values without temporary C variables
    // 3. Use proper clobber list
    // 4. Handle memory operands correctly
    
    asm volatile (
        // Your code here
        // Hint: Load values, swap them, store back
        :                    // outputs
        :                    // inputs  
        :                    // clobbers
    );
}

static int __init swap_init(void)
{
    unsigned long x = 42;
    unsigned long y = 84;
    
    pr_info("Before swap: x=%lu, y=%lu\n", x, y);
    swap_asm(&x, &y);
    pr_info("After swap: x=%lu, y=%lu\n", x, y);
    
    return 0;
}

static void __exit swap_exit(void)
{
    pr_info("Swap module unloaded\n");
}

module_init(swap_init);
module_exit(swap_exit);
MODULE_LICENSE("GPL");