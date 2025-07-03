# Lab 5: Timer and Interrupt Handling

## Overview
This lab explores advanced interrupt processing, deferred work mechanisms, and timer implementation. You'll implement various bottom-half techniques and analyze timer precision.

## Prerequisites
- Completed Labs 1-4
- Understanding of atomic operations and synchronization
- Familiarity with kernel module development

## Learning Objectives
- Implement different deferred work mechanisms (tasklets, work queues, softirqs)
- Compare timer types and analyze precision
- Build IOCTL interfaces for user-kernel communication
- Measure interrupt latency and system performance

---

## Timer Fundamentals (Extended)

### Timer Types Comparison

| Timer Type | Resolution | Context | Can Sleep | Use Case |
|------------|------------|---------|-----------|----------|
| timer_list | Jiffies (1/HZ) | Softirq | No | General scheduling |
| hrtimer | Nanoseconds | Softirq | No | Precise timing |
| schedule_timeout | Jiffies | Process | Yes | Process delays |

### Key Timer Concepts

**Jiffies**: Global counter incremented HZ times per second
```c
// HZ = 250 means 4ms resolution
unsigned long future = jiffies + msecs_to_jiffies(1000);  // 1 second from now
```

**Timer Callbacks**: Execute in softirq context (atomic)
```c
static void timer_callback(struct timer_list *timer)
{
    // Atomic context - no sleeping!
    atomic_inc(&counter);
    mod_timer(timer, jiffies + HZ);  // Reschedule
}
```

**High-Resolution Timers**: Hardware-based, nanosecond precision
```c
hrtimer_start(&hr_timer, ns_to_ktime(1000000), HRTIMER_MODE_REL);  // 1ms
```

---

## Exercise 1: Timer Precision Analysis

### Objective
Compare timer accuracy and measure jitter under different conditions.

### Implementation Steps

1. **Create Timer Module** (`timer_precision.c`):
```c
#include <linux/hrtimer.h>
#include <linux/ktime.h>

struct timer_stats {
    ktime_t expected_interval;
    ktime_t last_fire;
    s64 max_jitter;
    s64 min_jitter;
    atomic_t fire_count;
};

static void measure_jitter(struct timer_stats *stats, ktime_t now)
{
    if (stats->last_fire != 0) {
        s64 actual = ktime_to_ns(ktime_sub(now, stats->last_fire));
        s64 expected = ktime_to_ns(stats->expected_interval);
        s64 jitter = actual - expected;
        
        if (jitter > stats->max_jitter) stats->max_jitter = jitter;
        if (jitter < stats->min_jitter) stats->min_jitter = jitter;
    }
    stats->last_fire = now;
}
```

2. **Test Different Intervals**:
   - 1ms, 10ms, 100ms, 1000ms
   - Compare regular vs high-resolution timers

3. **Load Testing**:
```bash
# Generate CPU load while testing
stress-ng --cpu 4 --timeout 30s &
cat /proc/timer_precision
```

### Connection to Class Material
- **Jiffies vs HR Timers**: Demonstrates resolution differences from Lesson #5
- **Softirq Context**: Timer callbacks run in TIMER_SOFTIRQ context
- **Atomic Operations**: Safe counter updates in interrupt context

---

## Exercise 2: Bottom-Half Mechanisms

### Objective
Implement and compare all three deferred work mechanisms.

### Implementation Steps

1. **Tasklet Implementation**:
```c
static void tasklet_function(unsigned long data)
{
    struct work_data *work = (struct work_data *)data;
    ktime_t start = ktime_get();
    
    // Simulate work
    udelay(100);  // 100 microseconds
    
    work->tasklet_time = ktime_to_ns(ktime_sub(ktime_get(), start));
    atomic_inc(&work->tasklet_count);
}

DECLARE_TASKLET(my_tasklet, tasklet_function, (unsigned long)&work_data);
```

2. **Work Queue Implementation**:
```c
static void work_function(struct work_struct *work)
{
    struct work_data *data = container_of(work, struct work_data, work);
    ktime_t start = ktime_get();
    
    // Can sleep in work queue context
    msleep(1);  // 1 millisecond
    
    data->workqueue_time = ktime_to_ns(ktime_sub(ktime_get(), start));
    atomic_inc(&data->workqueue_count);
}

DECLARE_WORK(my_work, work_function);
```

3. **Softirq Usage** (using existing NET_TX_SOFTIRQ):
```c
// Raise softirq from interrupt handler
static irqreturn_t my_interrupt_handler(int irq, void *dev_id)
{
    // Schedule our tasklet (built on softirq)
    tasklet_schedule(&my_tasklet);
    return IRQ_HANDLED;
}
```

### Performance Comparison
Create `/proc/bottomhalf_stats` showing:
- Execution times for each mechanism
- CPU usage patterns
- Context switch overhead

### Connection to Class Material
- **Top-Half vs Bottom-Half**: Interrupt handler vs deferred processing
- **Context Restrictions**: Tasklets (atomic) vs work queues (process context)
- **Performance Trade-offs**: Speed vs flexibility from Lesson #5

---

## Exercise 3: IOCTL Interface

### Objective
Build comprehensive device control interface.

### Implementation Steps

1. **Define IOCTL Commands**:
```c
#define TIMER_IOC_MAGIC 't'
#define TIMER_SET_INTERVAL    _IOW(TIMER_IOC_MAGIC, 1, int)
#define TIMER_GET_STATS       _IOR(TIMER_IOC_MAGIC, 2, struct timer_stats)
#define TIMER_BULK_CONFIG     _IOWR(TIMER_IOC_MAGIC, 3, struct bulk_config)
#define TIMER_RESET_COUNTERS  _IO(TIMER_IOC_MAGIC, 4)

struct bulk_config {
    int timer_count;
    int intervals[10];
    int results[10];
};
```

2. **IOCTL Handler**:
```c
static long timer_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    switch (cmd) {
    case TIMER_SET_INTERVAL:
        if (copy_from_user(&interval, (int __user *)arg, sizeof(int)))
            return -EFAULT;
        if (interval < 1 || interval > 10000)
            return -EINVAL;
        // Update timer interval
        break;
        
    case TIMER_GET_STATS:
        if (copy_to_user((void __user *)arg, &stats, sizeof(stats)))
            return -EFAULT;
        break;
    }
    return 0;
}
```

3. **User Space Test Program**:
```c
#include <sys/ioctl.h>

int main() {
    int fd = open("/dev/timer_control", O_RDWR);
    int interval = 500;  // 500ms
    
    // Set timer interval
    if (ioctl(fd, TIMER_SET_INTERVAL, &interval) < 0) {
        perror("ioctl");
    }
    
    // Get statistics
    struct timer_stats stats;
    ioctl(fd, TIMER_GET_STATS, &stats);
    printf("Timer fired %d times\n", stats.fire_count);
}
```

### Connection to Class Material
- **User-Kernel Communication**: IOCTL as alternative to proc/sysfs
- **Copy Functions**: Safe data transfer between user/kernel space
- **Parameter Validation**: Security considerations from system call lessons

---

## Exercise 4: Real-time Timer Testing

### Objective
Measure timer precision under high-frequency operation and system load.

### Implementation Steps

1. **High-Frequency Timer Setup**:
```c
static enum hrtimer_restart precision_timer_callback(struct hrtimer *timer)
{
    ktime_t now = ktime_get();
    struct precision_stats *stats = container_of(timer, struct precision_stats, timer);
    
    // Measure jitter
    if (stats->last_time != 0) {
        s64 interval = ktime_to_ns(ktime_sub(now, stats->last_time));
        s64 jitter = interval - stats->expected_ns;
        
        // Update statistics
        stats->total_jitter += abs(jitter);
        if (abs(jitter) > stats->max_jitter)
            stats->max_jitter = abs(jitter);
    }
    
    stats->last_time = now;
    atomic_inc(&stats->count);
    
    hrtimer_forward_now(timer, ns_to_ktime(stats->expected_ns));
    return HRTIMER_RESTART;
}
```

2. **Latency Measurement**:
```c
// Measure interrupt latency
static irqreturn_t latency_test_handler(int irq, void *dev_id)
{
    ktime_t handler_start = ktime_get();
    // Compare with hardware timestamp if available
    s64 latency = ktime_to_ns(ktime_sub(handler_start, irq_timestamp));
    update_latency_stats(latency);
    return IRQ_HANDLED;
}
```

3. **Load Testing Framework**:
```c
// Generate different types of system load
static void generate_cpu_load(void)
{
    // CPU-intensive work
    volatile int dummy = 0;
    for (int i = 0; i < 1000000; i++) dummy++;
}

static void generate_io_load(void)
{
    // Schedule work that does I/O
    schedule_work(&io_work);
}
```

### Test Scenarios
1. **Baseline**: No system load
2. **CPU Load**: High CPU utilization
3. **I/O Load**: Heavy disk/network activity
4. **Mixed Load**: Combined CPU and I/O

### Connection to Class Material
- **Real-time Considerations**: Deterministic timing challenges
- **Interrupt Latency**: Time to respond to hardware events
- **High-Resolution Timers**: Nanosecond precision requirements

---

## Using Class Examples

### Repository Structure
```
examples/class_5/
├── timer_demo/          # Basic timer implementation
├── interrupt_demo/      # Interrupt handling examples  
├── workqueue_demo/      # Work queue usage
├── tasklet_demo/        # Tasklet implementation
└── ioctl_demo/          # IOCTL interface example
```

### Running Examples

1. **Timer Demo**:
```bash
cd examples/class_5/timer_demo
make
sudo insmod timer_demo.ko
cat /proc/timer_demo
echo "periodic_interval 100" > /dev/timer_control
```

2. **Interrupt Demo**:
```bash
cd examples/class_5/interrupt_demo
make
sudo insmod interrupt_demo.ko
cat /proc/interrupts | grep timer_irq
```

3. **Work Queue Demo**:
```bash
cd examples/class_5/workqueue_demo
make
sudo insmod workqueue_demo.ko
echo "schedule_work" > /proc/workqueue_control
dmesg | tail -10
```

### Example Modifications
- **Timer Demo**: Add jitter measurement
- **Interrupt Demo**: Implement shared interrupt handling
- **Work Queue Demo**: Compare delayed vs immediate work
- **Tasklet Demo**: Add priority testing (HI_SOFTIRQ vs TASKLET_SOFTIRQ)

---

## Testing and Analysis

### Performance Metrics
- **Timer Accuracy**: Compare expected vs actual intervals
- **Jitter Analysis**: Maximum, minimum, average deviation
- **CPU Usage**: Impact of different mechanisms
- **Latency**: Response time under load

### Monitoring Commands
```bash
# Watch interrupt statistics
watch -n 1 'cat /proc/interrupts'

# Monitor softirq activity  
watch -n 1 'cat /proc/softirqs'

# Check timer statistics
cat /proc/timer_list | head -20

# System load monitoring
vmstat 1
```

### Expected Results
- HR timers should show better precision than regular timers
- Work queues have higher latency but can perform complex operations
- Tasklets provide good performance for atomic operations
- System load affects timer precision

---

## Troubleshooting

### Common Issues
- **Timer not firing**: Check timer initialization and scheduling
- **Kernel panic**: Ensure proper cleanup in module exit
- **High CPU usage**: Verify timer intervals aren't too aggressive
- **IOCTL failures**: Check command encoding and permissions

### Debugging Tips
```c
// Add debug prints with timing
#define DEBUG_TIMER(fmt, ...) \
    pr_info("[%lu] " fmt, jiffies, ##__VA_ARGS__)

// Check timer state
if (timer_pending(&my_timer))
    pr_info("Timer is pending\n");

// Verify interrupt registration
if (request_irq(irq, handler, 0, "test", NULL))
    pr_err("Failed to register IRQ\n");
```

