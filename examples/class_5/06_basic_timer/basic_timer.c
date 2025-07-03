#include <linux/module.h>
#include <linux/timer.h>

static struct timer_list demo_timer;
static int timer_count = 0;

static void timer_callback(struct timer_list *timer)
{
    pr_info("Timer fired #%d at jiffies %lu\n", ++timer_count, jiffies);
    
    if (timer_count < 5) {
        mod_timer(&demo_timer, jiffies + HZ);  // Reschedule
    }
}

static int __init timer_init(void)
{
    timer_setup(&demo_timer, timer_callback, 0);
    mod_timer(&demo_timer, jiffies + 2 * HZ);  // Start in 2 seconds
    return 0;
}

static void __exit timer_exit(void)
{
    del_timer_sync(&demo_timer);
    pr_info("Timer stopped. Total fires: %d\n", timer_count);
}

MODULE_LICENSE("GPL");
module_init(timer_init);
module_exit(timer_exit);