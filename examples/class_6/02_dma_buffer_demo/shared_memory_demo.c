/*
 * examples/class_6/02_shared_memory_demo/shared_memory_demo.c
 * 
 * Simple Shared Memory Demo - User-Kernel Communication
 * No DMA - just shared memory via mmap
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <asm/io.h>


#define DEVICE_NAME "shared_mem"
#define BUFFER_SIZE (8 * 1024)  /* 8KB total */
#define MSG_SIZE (4 * 1024)     /* 4KB per area */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Linux Kernel Programming Course");
MODULE_DESCRIPTION("Simple shared memory demo");
MODULE_VERSION("1.0");

/* IOCTL commands */
#define SHARED_MEM_IOC_MAGIC 'S'
#define SHARED_MEM_SEND_MSG _IO(SHARED_MEM_IOC_MAGIC, 1)

/* Device structure */
struct shared_mem_device {
    struct cdev cdev;
    struct device *device;
    struct class *class;
    dev_t devt;
    
    /* Shared buffer */
    void *shared_buffer;
    
    /* Message areas */
    char *user_to_kernel;   /* First 4KB */
    char *kernel_to_user;   /* Second 4KB */
    
    struct mutex mutex;
    unsigned long msg_count;
};

static struct shared_mem_device *demo_device;

/* Process message from user */
static void process_message(void)
{
    char *input = demo_device->user_to_kernel;
    char *output = demo_device->kernel_to_user;
    int len;
    
    mutex_lock(&demo_device->mutex);
    
    /* Get input length */
    len = strnlen(input, MSG_SIZE - 1);
    input[len] = '\0';
    
    /* Log the message */
    pr_info("shared_mem: Received message #%lu: \"%s\"\n", 
            ++demo_device->msg_count, input);
    
    /* Create echo response */
    snprintf(output, MSG_SIZE, "Echo #%lu: %s", demo_device->msg_count, input);
    
    mutex_unlock(&demo_device->mutex);
}

/* File operations */
static int shared_mem_open(struct inode *inode, struct file *filp)
{
    filp->private_data = demo_device;
    pr_debug("shared_mem: Device opened\n");
    return 0;
}

static int shared_mem_release(struct inode *inode, struct file *filp)
{
    pr_debug("shared_mem: Device closed\n");
    return 0;
}

static long shared_mem_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    if (_IOC_TYPE(cmd) != SHARED_MEM_IOC_MAGIC)
        return -ENOTTY;
    
    switch (cmd) {
    case SHARED_MEM_SEND_MSG:
        process_message();
        break;
    default:
        return -ENOTTY;
    }
    
    return 0;
}

static int shared_mem_mmap(struct file *filp, struct vm_area_struct *vma)
{
    unsigned long size = vma->vm_end - vma->vm_start;
    unsigned long pfn;
    
    if (size > BUFFER_SIZE) {
        pr_err("mmap size too large: %lu > %d\n", size, BUFFER_SIZE);
        return -EINVAL;
    }
    
    /* Get page frame number from virtual address */
    pfn = virt_to_phys(demo_device->shared_buffer) >> PAGE_SHIFT;
    
    /* Map buffer to user space */
    if (remap_pfn_range(vma, vma->vm_start, pfn, size, vma->vm_page_prot)) {
        pr_err("Failed to mmap buffer\n");
        return -EAGAIN;
    }
    
    pr_debug("Buffer mapped to user space: 0x%lx - 0x%lx\n",
             vma->vm_start, vma->vm_end);
    
    return 0;
}

static const struct file_operations shared_mem_fops = {
    .owner = THIS_MODULE,
    .open = shared_mem_open,
    .release = shared_mem_release,
    .unlocked_ioctl = shared_mem_ioctl,
    .mmap = shared_mem_mmap,
};

static int __init shared_memory_demo_init(void)
{
    int ret;
    
    pr_info("shared_mem: Loading module\n");
    
    /* Allocate device structure */
    demo_device = kzalloc(sizeof(*demo_device), GFP_KERNEL);
    if (!demo_device)
        return -ENOMEM;
    
    mutex_init(&demo_device->mutex);
    
    /* Allocate shared buffer */
    demo_device->shared_buffer = kzalloc(BUFFER_SIZE, GFP_KERNEL);
    if (!demo_device->shared_buffer) {
        ret = -ENOMEM;
        goto err_free_device;
    }
    
    /* Setup message areas */
    demo_device->user_to_kernel = (char *)demo_device->shared_buffer;
    demo_device->kernel_to_user = (char *)demo_device->shared_buffer + MSG_SIZE;
    
    /* Get device number */
    ret = alloc_chrdev_region(&demo_device->devt, 0, 1, DEVICE_NAME);
    if (ret < 0)
        goto err_free_buffer;
    
    /* Setup cdev */
    cdev_init(&demo_device->cdev, &shared_mem_fops);
    demo_device->cdev.owner = THIS_MODULE;
    
    ret = cdev_add(&demo_device->cdev, demo_device->devt, 1);
    if (ret < 0)
        goto err_unregister_chrdev;
    
    /* Create device class */
    demo_device->class = class_create(THIS_MODULE, DEVICE_NAME);
    if (IS_ERR(demo_device->class)) {
        ret = PTR_ERR(demo_device->class);
        goto err_cdev_del;
    }
    
    /* Create device */
    demo_device->device = device_create(demo_device->class, NULL, 
                                        demo_device->devt, NULL, DEVICE_NAME);
    if (IS_ERR(demo_device->device)) {
        ret = PTR_ERR(demo_device->device);
        goto err_class_destroy;
    }
    
    pr_info("shared_mem: Device /dev/%s created\n", DEVICE_NAME);
    pr_info("shared_mem: Buffer: %d bytes (user->kernel: 0-%d, kernel->user: %d-%d)\n",
            BUFFER_SIZE, MSG_SIZE-1, MSG_SIZE, BUFFER_SIZE-1);
    
    return 0;

err_class_destroy:
    class_destroy(demo_device->class);
err_cdev_del:
    cdev_del(&demo_device->cdev);
err_unregister_chrdev:
    unregister_chrdev_region(demo_device->devt, 1);
err_free_buffer:
    kfree(demo_device->shared_buffer);
err_free_device:
    kfree(demo_device);
    return ret;
}

static void __exit shared_memory_demo_exit(void)
{
    pr_info("shared_mem: Unloading module\n");
    
    device_destroy(demo_device->class, demo_device->devt);
    class_destroy(demo_device->class);
    cdev_del(&demo_device->cdev);
    unregister_chrdev_region(demo_device->devt, 1);
    kfree(demo_device->shared_buffer);
    kfree(demo_device);
    
    pr_info("shared_mem: Module unloaded\n");
}

module_init(shared_memory_demo_init);
module_exit(shared_memory_demo_exit);