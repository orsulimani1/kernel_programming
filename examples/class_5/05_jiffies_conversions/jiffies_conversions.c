#include <linux/module.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/ktime.h>

static unsigned long start_jiffies;
static ktime_t start_ktime;
static struct timer_list time_timer;
static struct proc_dir_entry *proc_entry;
static int timer_fires = 0;

static void time_callback(struct timer_list *timer)
{
    unsigned long uptime = jiffies - start_jiffies;
    unsigned long future_time = jiffies + 5 * HZ;
    
    timer_fires++;
    
    pr_info("TIME: Fire #%d - Uptime %lu jiffies (%u ms)\n",
            timer_fires, uptime, jiffies_to_msecs(uptime));
    
    // Demonstrate time comparison functions
    if (time_after(jiffies, start_jiffies + 10 * HZ)) {
        pr_info("TIME: More than 10 seconds since start\n");
    }
    
    pr_info("TIME: Future time (+5s): %lu jiffies\n", future_time);
    pr_info("TIME: time_before(now, future): %s\n",
            time_before(jiffies, future_time) ? "true" : "false");
    
    mod_timer(&time_timer, jiffies + 2 * HZ);
}

/* Comprehensive proc interface */
static int time_proc_show(struct seq_file *m, void *v)
{
    unsigned long current_jiffies = jiffies;
    unsigned long uptime_jiffies = current_jiffies - start_jiffies;
    ktime_t current_ktime = ktime_get();
    ktime_t uptime_ktime = ktime_sub(current_ktime, start_ktime);
    
    seq_printf(m, "Jiffies and Time Demo\n");
    seq_printf(m, "====================\n");
    
    seq_printf(m, "System Configuration:\n");
    seq_printf(m, "HZ (timer freq): %d\n", HZ);
    seq_printf(m, "Timer resolution: %d ms\n", 1000 / HZ);
    
    seq_printf(m, "\nCurrent Values:\n");
    seq_printf(m, "Current jiffies: %lu\n", current_jiffies);
    seq_printf(m, "Start jiffies: %lu\n", start_jiffies);
    seq_printf(m, "Uptime jiffies: %lu\n", uptime_jiffies);
    
    seq_printf(m, "\nTime Conversions:\n");
    seq_printf(m, "Uptime in ms: %u\n", jiffies_to_msecs(uptime_jiffies));
    seq_printf(m, "Uptime in seconds: %u\n", jiffies_to_msecs(uptime_jiffies) / 1000);
    seq_printf(m, "1 second = %lu jiffies\n", HZ);
    seq_printf(m, "100ms = %lu jiffies\n", msecs_to_jiffies(100));
    seq_printf(m, "1000μs = %lu jiffies\n", usecs_to_jiffies(1000));
    
    seq_printf(m, "\nHigh-Resolution Time:\n");
    seq_printf(m, "ktime uptime: %lld ns\n", ktime_to_ns(uptime_ktime));
    seq_printf(m, "ktime uptime: %lld ms\n", ktime_to_ms(uptime_ktime));
    
    seq_printf(m, "\nTimer Statistics:\n");
    seq_printf(m, "Timer fires: %d\n", timer_fires);
    
    // Demonstrate overflow-safe comparisons
    unsigned long future = current_jiffies + 100;
    seq_printf(m, "\nTime Comparison Demo:\n");
    seq_printf(m, "time_after(future, now): %s\n",
               time_after(future, current_jiffies) ? "true" : "false");
    
    return 0;
}

static int time_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, time_proc_show, NULL);
}

static const struct proc_ops time_proc_ops = {
    .proc_open = time_proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static int __init time_init(void)
{
    start_jiffies = jiffies;
    start_ktime = ktime_get();
    
    pr_info("TIME: Demo loaded at jiffies %lu (HZ=%d)\n", start_jiffies, HZ);
    pr_info("TIME: Timer resolution: %d ms\n", 1000/HZ);
    
    timer_setup(&time_timer, time_callback, 0);
    mod_timer(&time_timer, jiffies + HZ);
    
    proc_entry = proc_create("time_demo", 0444, NULL, &time_proc_ops);
    
    return 0;
}

static void __exit time_exit(void)
{
    unsigned long total_uptime = jiffies - start_jiffies;
    
    del_timer_sync(&time_timer);
    proc_remove(proc_entry);
    
    pr_info("TIME: Demo unloaded after %lu jiffies (%u ms)\n",
            total_uptime, jiffies_to_msecs(total_uptime));
}

MODULE_LICENSE("GPL");
module_init(time_init);
module_exit(time_exit);