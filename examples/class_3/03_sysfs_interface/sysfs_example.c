#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/string.h>

static struct kobject *example_kobj;
static int example_value = 0;
static char example_string[64] = "default";

// Show function for integer attribute
static ssize_t value_show(struct kobject *kobj, 
                         struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", example_value);
}

// Store function for integer attribute
static ssize_t value_store(struct kobject *kobj, 
                          struct kobj_attribute *attr,
                          const char *buf, size_t count)
{
    int ret;
    
    ret = kstrtoint(buf, 10, &example_value);
    if (ret < 0)
        return ret;
        
    pr_info("Value set to: %d\n", example_value);
    return count;
}

// String attribute functions
static ssize_t string_show(struct kobject *kobj,
                          struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "%s\n", example_string);
}

static ssize_t string_store(struct kobject *kobj,
                           struct kobj_attribute *attr,
                           const char *buf, size_t count)
{
    if (count >= sizeof(example_string))
        return -EINVAL;
        
    strncpy(example_string, buf, count);
    example_string[count - 1] = '\0';  // Remove newline
    
    pr_info("String set to: %s\n", example_string);
    return count;
}

// Define attributes
static struct kobj_attribute value_attribute = 
    __ATTR(value, 0664, value_show, value_store);
static struct kobj_attribute string_attribute = 
    __ATTR(string, 0664, string_show, string_store);

// Group attributes
static struct attribute *attrs[] = {
    &value_attribute.attr,
    &string_attribute.attr,
    NULL,   // NULL terminated
};

static struct attribute_group attr_group = {
    .attrs = attrs,
};

static int __init sysfs_example_init(void)
{
    int retval;
    
    // Create kobject
    example_kobj = kobject_create_and_add("kernel_course", kernel_kobj);
    if (!example_kobj)
        return -ENOMEM;
        
    // Create sysfs group
    retval = sysfs_create_group(example_kobj, &attr_group);
    if (retval) {
        kobject_put(example_kobj);
        return retval;
    }
    
    pr_info("sysfs interface created at /sys/kernel/kernel_course/\n");
    return 0;
}

static void __exit sysfs_example_exit(void)
{
    sysfs_remove_group(example_kobj, &attr_group);
    kobject_put(example_kobj);
    pr_info("sysfs interface removed\n");
}

module_init(sysfs_example_init);
module_exit(sysfs_example_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kernel Course");
MODULE_DESCRIPTION("sysfs interface example");