#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mman.h>

#define DEVICE_NAME "simple_vma"
#define BUFFER_SIZE (PAGE_SIZE * 4)  // 4 pages

struct simple_vma_dev {
    struct cdev cdev;
    struct class *class;
    struct device *device;
    char *buffer;
    struct mutex mutex;
};

static struct simple_vma_dev *vma_dev;
static dev_t dev_number;
static int major_number;

// VMA operations for our device
static void example_vma_open(struct vm_area_struct *vma)
{
    pr_info("simple_vma: VMA opened - start: 0x%lx, end: 0x%lx, size: %lu\n",
            vma->vm_start, vma->vm_end, vma->vm_end - vma->vm_start);
}

static void example_vma_close(struct vm_area_struct *vma)
{
    pr_info("simple_vma: VMA closed - start: 0x%lx, end: 0x%lx\n",
            vma->vm_start, vma->vm_end);
}

static vm_fault_t example_vma_fault(struct vm_fault *vmf)
{
    struct vm_area_struct *vma = vmf->vma;
    struct page *page;
    unsigned long offset;
    void *page_ptr;
    
    pr_info("simple_vma: Page fault at address: 0x%lx, page offset: %lu\n",
            vmf->address, vmf->pgoff);
    
    // Calculate offset in our buffer
    offset = (vmf->address - vma->vm_start) + (vma->vm_pgoff << PAGE_SHIFT);
    
    if (offset >= BUFFER_SIZE) {
        pr_err("simple_vma: Fault beyond buffer size\n");
        return VM_FAULT_SIGBUS;
    }
    
    // Get the page for this offset
    page_ptr = vma_dev->buffer + (vmf->pgoff * PAGE_SIZE);
    page = vmalloc_to_page(page_ptr);
    
    if (!page) {
        pr_err("simple_vma: No page found\n");
        return VM_FAULT_SIGBUS;
    }
    
    // Set up the page
    get_page(page);
    vmf->page = page;
    
    pr_info("simple_vma: Page fault handled successfully\n");
    return 0;
}

static const struct vm_operations_struct example_vm_ops = {
    .open = example_vma_open,
    .close = example_vma_close,
    .fault = example_vma_fault,
};

// Device file operations
static int example_char_open(struct inode *inode, struct file *file)
{
    pr_info("simple_vma: Device opened\n");
    return 0;
}

static int example_char_release(struct inode *inode, struct file *file)
{
    pr_info("simple_vma: Device released\n");
    return 0;
}

static ssize_t example_char_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    ssize_t ret = 0;
    
    if (mutex_lock_interruptible(&vma_dev->mutex))
        return -ERESTARTSYS;
    
    if (*ppos >= BUFFER_SIZE)
        goto out;
    
    if (*ppos + count > BUFFER_SIZE)
        count = BUFFER_SIZE - *ppos;
    
    if (copy_to_user(buf, vma_dev->buffer + *ppos, count)) {
        ret = -EFAULT;
        goto out;
    }
    
    *ppos += count;
    ret = count;
    
out:
    mutex_unlock(&vma_dev->mutex);
    return ret;
}

static ssize_t example_char_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    ssize_t ret = 0;
    
    if (mutex_lock_interruptible(&vma_dev->mutex))
        return -ERESTARTSYS;
    
    if (*ppos >= BUFFER_SIZE) {
        ret = -ENOSPC;
        goto out;
    }
    
    if (*ppos + count > BUFFER_SIZE)
        count = BUFFER_SIZE - *ppos;
    
    if (copy_from_user(vma_dev->buffer + *ppos, buf, count)) {
        ret = -EFAULT;
        goto out;
    }
    
    *ppos += count;
    ret = count;
    
out:
    mutex_unlock(&vma_dev->mutex);
    return ret;
}

// Memory mapping implementation
static int example_char_mmap(struct file *file, struct vm_area_struct *vma)
{
    unsigned long size = vma->vm_end - vma->vm_start;
    
    pr_info("simple_vma: mmap called - start: 0x%lx, end: 0x%lx, size: %lu\n",
            vma->vm_start, vma->vm_end, size);
    
    // Check size
    if (size > BUFFER_SIZE) {
        pr_err("simple_vma: Requested size too large: %lu > %d\n", size, BUFFER_SIZE);
        return -EINVAL;
    }
    
    // Set VMA flags
    vma->vm_flags |= VM_DONTEXPAND | VM_DONTDUMP;
    vma->vm_private_data = vma_dev;
    vma->vm_ops = &example_vm_ops;
    
    // Call open to initialize
    example_vma_open(vma);
    
    pr_info("simple_vma: mmap completed successfully\n");
    return 0;
}

static const struct file_operations example_fops = {
    .owner = THIS_MODULE,
    .open = example_char_open,
    .release = example_char_release,
    .read = example_char_read,
    .write = example_char_write,
    .mmap = example_char_mmap,
};

static int __init simple_vma_init(void)
{
    int ret;
    
    pr_info("simple_vma: Initializing VMA example module\n");
    
    // Allocate device structure
    vma_dev = kzalloc(sizeof(*vma_dev), GFP_KERNEL);
    if (!vma_dev)
        return -ENOMEM;
    
    mutex_init(&vma_dev->mutex);
    
    // Allocate buffer using vmalloc for demonstration
    vma_dev->buffer = vmalloc(BUFFER_SIZE);
    if (!vma_dev->buffer) {
        ret = -ENOMEM;
        goto err_free_dev;
    }
    
    // Initialize buffer with pattern
    memset(vma_dev->buffer, 0x42, BUFFER_SIZE);
    sprintf(vma_dev->buffer, "Hello from kernel VMA example!\n");
    
    // Get device number
    ret = alloc_chrdev_region(&dev_number, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        pr_err("simple_vma: Failed to allocate device number\n");
        goto err_free_buffer;
    }
    
    major_number = MAJOR(dev_number);
    
    // Initialize and add character device
    cdev_init(&vma_dev->cdev, &example_fops);
    vma_dev->cdev.owner = THIS_MODULE;
    
    ret = cdev_add(&vma_dev->cdev, dev_number, 1);
    if (ret < 0) {
        pr_err("simple_vma: Failed to add character device\n");
        goto err_unreg_chrdev;
    }
    
    // Create device class
    vma_dev->class = class_create(THIS_MODULE, DEVICE_NAME);
    if (IS_ERR(vma_dev->class)) {
        ret = PTR_ERR(vma_dev->class);
        pr_err("simple_vma: Failed to create device class\n");
        goto err_del_cdev;
    }
    
    // Create device node
    vma_dev->device = device_create(vma_dev->class, NULL, dev_number, NULL, DEVICE_NAME);
    if (IS_ERR(vma_dev->device)) {
        ret = PTR_ERR(vma_dev->device);
        pr_err("simple_vma: Failed to create device\n");
        goto err_destroy_class;
    }
    
    pr_info("simple_vma: Module loaded successfully\n");
    pr_info("simple_vma: Device created at /dev/%s (major: %d)\n", DEVICE_NAME, major_number);
    pr_info("simple_vma: Buffer size: %d bytes (%d pages)\n", BUFFER_SIZE, BUFFER_SIZE / PAGE_SIZE);
    
    return 0;
    
err_destroy_class:
    class_destroy(vma_dev->class);
err_del_cdev:
    cdev_del(&vma_dev->cdev);
err_unreg_chrdev:
    unregister_chrdev_region(dev_number, 1);
err_free_buffer:
    vfree(vma_dev->buffer);
err_free_dev:
    kfree(vma_dev);
    return ret;
}

static void __exit simple_vma_exit(void)
{
    if (vma_dev) {
        device_destroy(vma_dev->class, dev_number);
        class_destroy(vma_dev->class);
        cdev_del(&vma_dev->cdev);
        unregister_chrdev_region(dev_number, 1);
        vfree(vma_dev->buffer);
        kfree(vma_dev);
    }
    
    pr_info("simple_vma: Module unloaded\n");
}

module_init(simple_vma_init);
module_exit(simple_vma_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kernel Programming Course");
MODULE_DESCRIPTION("Simple VMA Memory Allocation Example");
MODULE_VERSION("1.0");