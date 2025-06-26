#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/cpumask.h>
#include <linux/smp.h>
#include <linux/irq.h>

static struct proc_dir_entry *proc_entry; 

/* Simulate IRQ affinity management */
static int demo_irq_affinity(int irq, int target_cpu)
{
    cpumask_t mask;
    
    if (target_cpu >= num_online_cpus())
        return -EINVAL;
    
    cpumask_clear(&mask);
    cpumask_set_cpu(target_cpu, &mask);
    
    pr_info("Setting IRQ %d affinity to CPU %d\n", irq, target_cpu);
    return 0;  /* Would call irq_set_affinity() on real hardware */
}

/* Inter-processor interrupt simulation */
static void send_ipi_demo(int target_cpu)
{
    pr_info("Sending IPI to CPU %d (simulated)\n", target_cpu);
    /* In real ARM: send_ipi_to_cpu(target_cpu, SGI_ID); */
}

/* Architecture detection */
static const char *get_arch_info(void)
{
#ifdef CONFIG_X86_64
    return "x86_64 (APIC architecture)";
#elif defined(CONFIG_ARM64)
    return "ARM64 (GIC architecture)";
#elif defined(CONFIG_ARM)
    return "ARM (GIC architecture)";
#else
    return "Unknown architecture";
#endif
}

/* Proc interface */
static int arch_show(struct seq_file *m, void *v)
{
    int cpu;
    
    seq_printf(m, "=== Architecture Demo ===\n\n");
    seq_printf(m, "Architecture: %s\n", get_arch_info());
    seq_printf(m, "Number of CPUs: %d\n", num_online_cpus());
    seq_printf(m, "Current CPU: %d\n", smp_processor_id());
    
    seq_printf(m, "\nOnline CPUs: ");
    for_each_online_cpu(cpu) {
        seq_printf(m, "%d ", cpu);
    }
    seq_printf(m, "\n");
    
    seq_printf(m, "\nIRQ Affinity Demo:\n");
    demo_irq_affinity(16, 0);
    demo_irq_affinity(17, 1);
    
    seq_printf(m, "\nIPI Demo:\n");
    for_each_online_cpu(cpu) {
        if (cpu != smp_processor_id())
            send_ipi_demo(cpu);
    }
    
    return 0;
}

static int arch_open(struct inode *inode, struct file *file)
{
    return single_open(file, arch_show, NULL);
}

static const struct proc_ops arch_fops = {
    .proc_open = arch_open,
    .proc_read = seq_read,
    .proc_release = single_release,
};

static int __init arch_demo_init(void)
{
    proc_entry = proc_create("arch_demo", 0444, NULL, &arch_fops);
    if (!proc_entry) {
        pr_err("Failed to create proc entry\n");
        return -ENOMEM;
    }
    pr_info("Architecture demo loaded on %s\n", get_arch_info());
    return 0;
}

static void __exit arch_demo_exit(void)
{
    proc_remove(proc_entry);  

    pr_info("Architecture demo unloaded\n");
}


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Architecture-Specific Interrupt Demo");

module_init(arch_demo_init);
module_exit(arch_demo_exit);