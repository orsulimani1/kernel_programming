#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/mutex.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kernel Programming Course");
MODULE_DESCRIPTION("IOCTL Interface Demo");
MODULE_VERSION("1.0");

#define DEVICE_NAME "ioctl_demo"
#define CLASS_NAME "ioctl_demo_class"

// IOCTL command definitions (must match user space)
#define IOCTL_DEMO_MAGIC 'D'
#define IOCTL_SET_VALUE    _IOW(IOCTL_DEMO_MAGIC, 1, int)
#define IOCTL_GET_VALUE    _IOR(IOCTL_DEMO_MAGIC, 2, int)
#define IOCTL_SET_STRING   _IOW(IOCTL_DEMO_MAGIC, 3, char*)
#define IOCTL_GET_STRING   _IOR(IOCTL_DEMO_MAGIC, 4, char*)
#define IOCTL_RESET        _IO(IOCTL_DEMO_MAGIC, 5)

#define MAX_STRING_SIZE 256

// Device data structure
struct ioctl_device {
    int value;
    char string[MAX_STRING_SIZE];
    struct mutex lock;
};

// Global variables
static dev_t dev_number;
static struct class *ioctl_class;
static struct device *ioctl_device_ptr;
static struct cdev ioctl_cdev;
static struct ioctl_device device_data;

// Default values
#define DEFAULT_VALUE 0
#define DEFAULT_STRING "default_string"

/* Device file operations */
static int ioctl_demo_open(struct inode *inode, struct file *file)
{
    pr_info("IOCTL Demo: Device opened\n");
    return 0;
}

static int ioctl_demo_release(struct inode *inode, struct file *file)
{
    pr_info("IOCTL Demo: Device closed\n");
    return 0;
}

static long ioctl_demo_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    int value;
    char *user_string;
    char kernel_buffer[MAX_STRING_SIZE];

    // Verify magic number
    if (_IOC_TYPE(cmd) != IOCTL_DEMO_MAGIC) {
        pr_err("IOCTL Demo: Invalid magic number\n");
        return -ENOTTY;
    }

    // Verify command number
    if (_IOC_NR(cmd) > 5) {
        pr_err("IOCTL Demo: Invalid command number\n");
        return -ENOTTY;
    }

    // Verify access permissions
    if (_IOC_DIR(cmd) & _IOC_READ) {
        ret = !access_ok((void __user *)arg, _IOC_SIZE(cmd));
        if (ret) {
            pr_err("IOCTL Demo: Read access verification failed\n");
            return -EFAULT;
        }
    }
    if (_IOC_DIR(cmd) & _IOC_WRITE) {
        ret = !access_ok((void __user *)arg, _IOC_SIZE(cmd));
        if (ret) {
            pr_err("IOCTL Demo: Write access verification failed\n");
            return -EFAULT;
        }
    }

    mutex_lock(&device_data.lock);

    switch (cmd) {
    case IOCTL_SET_VALUE:
        pr_info("IOCTL Demo: SET_VALUE command\n");
        
        if (copy_from_user(&value, (int __user *)arg, sizeof(int))) {
            pr_err("IOCTL Demo: Failed to copy value from user\n");
            ret = -EFAULT;
            break;
        }
        
        device_data.value = value;
        pr_info("IOCTL Demo: Value set to %d\n", value);
        break;

    case IOCTL_GET_VALUE:
        pr_info("IOCTL Demo: GET_VALUE command\n");
        
        if (copy_to_user((int __user *)arg, &device_data.value, sizeof(int))) {
            pr_err("IOCTL Demo: Failed to copy value to user\n");
            ret = -EFAULT;
            break;
        }
        
        pr_info("IOCTL Demo: Returned value %d to user\n", device_data.value);
        break;

    case IOCTL_SET_STRING:
        pr_info("IOCTL Demo: SET_STRING command\n");
        
        user_string = (char __user *)arg;
        
        // Copy string from user space with bounds checking
        if (strncpy_from_user(kernel_buffer, user_string, MAX_STRING_SIZE - 1) < 0) {
            pr_err("IOCTL Demo: Failed to copy string from user\n");
            ret = -EFAULT;
            break;
        }
        
        kernel_buffer[MAX_STRING_SIZE - 1] = '\0';  // Ensure null termination
        strcpy(device_data.string, kernel_buffer);
        pr_info("IOCTL Demo: String set to '%s'\n", device_data.string);
        break;

    case IOCTL_GET_STRING:
        pr_info("IOCTL Demo: GET_STRING command\n");
        
        user_string = (char __user *)arg;
        
        if (copy_to_user(user_string, device_data.string, strlen(device_data.string) + 1)) {
            pr_err("IOCTL Demo: Failed to copy string to user\n");
            ret = -EFAULT;
            break;
        }
        
        pr_info("IOCTL Demo: Returned string '%s' to user\n", device_data.string);
        break;

    case IOCTL_RESET:
        pr_info("IOCTL Demo: RESET command\n");
        
        device_data.value = DEFAULT_VALUE;
        strcpy(device_data.string, DEFAULT_STRING);
        pr_info("IOCTL Demo: Device reset to defaults (value=%d, string='%s')\n",
                device_data.value, device_data.string);
        break;

    default:
        pr_err("IOCTL Demo: Unknown command 0x%x\n", cmd);
        ret = -ENOTTY;
        break;
    }

    mutex_unlock(&device_data.lock);
    return ret;
}

static const struct file_operations ioctl_fops = {
    .owner = THIS_MODULE,
    .open = ioctl_demo_open,
    .release = ioctl_demo_release,
    .unlocked_ioctl = ioctl_demo_ioctl,
};

static int __init ioctl_demo_init(void)
{
    int ret;

    pr_info("IOCTL Demo: Module loading...\n");

    // Initialize device data
    device_data.value = DEFAULT_VALUE;
    strcpy(device_data.string, DEFAULT_STRING);
    mutex_init(&device_data.lock);

    // Allocate device number
    ret = alloc_chrdev_region(&dev_number, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        pr_err("IOCTL Demo: Failed to allocate device number\n");
        return ret;
    }

    pr_info("IOCTL Demo: Allocated device number %d:%d\n", 
            MAJOR(dev_number), MINOR(dev_number));

    // Create device class
    ioctl_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(ioctl_class)) {
        pr_err("IOCTL Demo: Failed to create device class\n");
        unregister_chrdev_region(dev_number, 1);
        return PTR_ERR(ioctl_class);
    }

    // Initialize and add character device
    cdev_init(&ioctl_cdev, &ioctl_fops);
    ioctl_cdev.owner = THIS_MODULE;

    ret = cdev_add(&ioctl_cdev, dev_number, 1);
    if (ret < 0) {
        pr_err("IOCTL Demo: Failed to add character device\n");
        class_destroy(ioctl_class);
        unregister_chrdev_region(dev_number, 1);
        return ret;
    }

    // Create device file
    ioctl_device_ptr = device_create(ioctl_class, NULL, dev_number, NULL, DEVICE_NAME);
    if (IS_ERR(ioctl_device_ptr)) {
        pr_err("IOCTL Demo: Failed to create device file\n");
        cdev_del(&ioctl_cdev);
        class_destroy(ioctl_class);
        unregister_chrdev_region(dev_number, 1);
        return PTR_ERR(ioctl_device_ptr);
    }

    pr_info("IOCTL Demo: Module loaded successfully\n");
    pr_info("IOCTL Demo: Device created at /dev/%s\n", DEVICE_NAME);
    pr_info("IOCTL Demo: Initial values - value=%d, string='%s'\n",
            device_data.value, device_data.string);

    return 0;
}

static void __exit ioctl_demo_exit(void)
{
    pr_info("IOCTL Demo: Module unloading...\n");

    // Clean up in reverse order
    if (ioctl_device_ptr) {
        device_destroy(ioctl_class, dev_number);
    }

    cdev_del(&ioctl_cdev);

    if (ioctl_class) {
        class_destroy(ioctl_class);
    }

    unregister_chrdev_region(dev_number, 1);

    pr_info("IOCTL Demo: Module unloaded\n");
    pr_info("IOCTL Demo: Final values - value=%d, string='%s'\n",
            device_data.value, device_data.string);
}

module_init(ioctl_demo_init);
module_exit(ioctl_demo_exit);