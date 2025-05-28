#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

// Module initialization function
static int __init hello_init(void)
{
    pr_info("Hello from kernel module!\n");
    pr_info("Module loaded successfully\n");
    return 0;  // Return 0 for success
}

// Module cleanup function
static void __exit hello_exit(void)
{
    pr_info("Goodbye from kernel module!\n");
}

// Register init and exit functions
module_init(hello_init);
module_exit(hello_exit);

// Module information
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A simple hello world kernel module");
MODULE_VERSION("1.0");