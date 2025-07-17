#include <linux/module.h>
#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/statfs.h>

#define SIMPLE_MAGIC 0x19980122

// Forward declarations
static struct inode *example_alloc_inode(struct super_block *sb);
static void example_destroy_inode(struct inode *inode);
static int example_statfs(struct dentry *dentry, struct kstatfs *buf);

// 1. Superblock Operations
static const struct super_operations example_super_ops = {
    .alloc_inode    = example_alloc_inode,
    .destroy_inode  = example_destroy_inode,
    .statfs         = example_statfs,
};

// Simple inode structure
struct example_inode_info {
    struct inode vfs_inode;
    char data[64];  // Simple data storage
};

// Forward declarations for inode operations
static struct dentry *example_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags);
static int example_create(struct user_namespace *mnt_userns, struct inode *dir, 
                         struct dentry *dentry, umode_t mode, bool excl);
static int example_unlink(struct inode *dir, struct dentry *dentry);

// Forward declarations for file operations
static ssize_t example_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);
static ssize_t example_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos);
static int example_open(struct inode *inode, struct file *file);
static int example_readdir(struct file *file, struct dir_context *ctx);

// 2. Inode Operations
static const struct inode_operations example_dir_inode_ops = {
    .lookup = example_lookup,
    .create = example_create,
    .unlink = example_unlink,
};

static const struct inode_operations example_file_inode_ops = {
    // Simple file inode operations - can be empty for basic functionality
};

// 3. File Operations
static const struct file_operations example_file_ops = {
    .open    = example_open,
    .read    = example_read,
    .write   = example_write,
    .llseek  = default_llseek,
};

// Directory file operations
static const struct file_operations example_dir_ops = {
    .open       = dcache_dir_open,
    .release    = dcache_dir_close,
    .llseek     = dcache_dir_lseek,
    .read       = generic_read_dir,
    .iterate    = example_readdir,
};

// Helper function to get example_inode_info from inode
static inline struct example_inode_info *EXAMPLE_I(struct inode *inode)
{
    return container_of(inode, struct example_inode_info, vfs_inode);
}

// Superblock Operations Implementation
static struct inode *example_alloc_inode(struct super_block *sb)
{
    struct example_inode_info *ei;
    
    ei = kmalloc(sizeof(struct example_inode_info), GFP_KERNEL);
    if (!ei)
        return NULL;
    
    memset(ei->data, 0, sizeof(ei->data));
    
    // Initialize the VFS inode properly
    inode_init_once(&ei->vfs_inode);
    
    return &ei->vfs_inode;
}

static void example_destroy_inode(struct inode *inode)
{
    kfree(EXAMPLE_I(inode));
}

static int example_statfs(struct dentry *dentry, struct kstatfs *buf)
{
    buf->f_type = SIMPLE_MAGIC;
    buf->f_bsize = PAGE_SIZE;
    buf->f_blocks = 0;
    buf->f_bfree = 0;
    buf->f_bavail = 0;
    buf->f_files = 0;
    buf->f_ffree = 0;
    buf->f_namelen = 255;
    return 0;
}

// Inode Operations Implementation
static struct inode *example_get_inode(struct super_block *sb, umode_t mode)
{
    struct inode *inode = new_inode(sb);
    
    if (inode) {
        inode->i_ino = get_next_ino();
        inode->i_mode = mode;
        inode->i_uid = current_fsuid();
        inode->i_gid = current_fsgid();
        inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode);
        
        // Initialize i_mapping properly
        inode->i_mapping->a_ops = &empty_aops;
        
        switch (mode & S_IFMT) {
        case S_IFREG:
            inode->i_op = &example_file_inode_ops;
            inode->i_fop = &example_file_ops;
            break;
        case S_IFDIR:
            inode->i_op = &example_dir_inode_ops;
            inode->i_fop = &example_dir_ops;
            inc_nlink(inode);
            break;
        }
    }
    return inode;
}

static struct dentry *example_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags)
{
    pr_info("example_vfs: lookup called for '%s'\n", dentry->d_name.name);
    
    // For demonstration, we'll create a simple file on lookup
    if (strcmp(dentry->d_name.name, "testfile") == 0) {
        struct inode *inode = example_get_inode(dir->i_sb, S_IFREG | 0644);
        if (inode) {
            d_add(dentry, inode);
            return NULL;
        }
    }
    
    // File not found
    d_add(dentry, NULL);
    return NULL;
}

static int example_create(struct user_namespace *mnt_userns, struct inode *dir, 
                         struct dentry *dentry, umode_t mode, bool excl)
{
    struct inode *inode;
    
    pr_info("example_vfs: create called for '%s'\n", dentry->d_name.name);
    
    inode = example_get_inode(dir->i_sb, mode | S_IFREG);
    if (!inode)
        return -ENOSPC;
    
    d_instantiate(dentry, inode);
    dget(dentry);
    dir->i_mtime = dir->i_ctime = current_time(dir);
    
    return 0;
}

static int example_unlink(struct inode *dir, struct dentry *dentry)
{
    struct inode *inode = d_inode(dentry);
    
    pr_info("example_vfs: unlink called for '%s'\n", dentry->d_name.name);
    
    inode->i_ctime = dir->i_ctime = dir->i_mtime = current_time(inode);
    drop_nlink(inode);
    dput(dentry);
    
    return 0;
}

// File Operations Implementation
static int example_open(struct inode *inode, struct file *file)
{
    pr_info("example_vfs: File opened\n");
    return 0;
}

static ssize_t example_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    struct inode *inode = file_inode(file);
    struct example_inode_info *ei = EXAMPLE_I(inode);
    size_t len = strlen(ei->data);
    
    pr_info("example_vfs: read called, pos=%lld, count=%zu\n", *ppos, count);
    
    if (*ppos >= len)
        return 0;
    
    if (*ppos + count > len)
        count = len - *ppos;
    
    if (copy_to_user(buf, ei->data + *ppos, count))
        return -EFAULT;
    
    *ppos += count;
    return count;
}

static ssize_t example_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    struct inode *inode = file_inode(file);
    struct example_inode_info *ei = EXAMPLE_I(inode);
    
    pr_info("example_vfs: write called, pos=%lld, count=%zu\n", *ppos, count);
    
    if (count > sizeof(ei->data) - 1)
        count = sizeof(ei->data) - 1;
    
    if (copy_from_user(ei->data, buf, count))
        return -EFAULT;
    
    ei->data[count] = '\0';
    inode->i_size = count;
    
    return count;
}

// Directory Operations Implementation
static int example_readdir(struct file *file, struct dir_context *ctx)
{
    pr_info("example_vfs: readdir called, pos=%lld\n", ctx->pos);
    
    if (ctx->pos == 0) {
        if (!dir_emit_dot(file, ctx))
            return 0;
        ctx->pos = 1;
    }
    
    if (ctx->pos == 1) {
        if (!dir_emit_dotdot(file, ctx))
            return 0;
        ctx->pos = 2;
    }
    
    if (ctx->pos == 2) {
        if (!dir_emit(ctx, "testfile", 8, 1000, DT_REG))
            return 0;
        ctx->pos = 3;
    }
    
    return 0;
}

// Filesystem mount and unmount
static int example_fill_super(struct super_block *sb, void *data, int silent)
{
    struct inode *root;
    
    pr_info("example_vfs: fill_super called\n");
    
    sb->s_blocksize = PAGE_SIZE;
    sb->s_blocksize_bits = PAGE_SHIFT;
    sb->s_magic = SIMPLE_MAGIC;
    sb->s_op = &example_super_ops;
    sb->s_time_gran = 1;
    
    root = example_get_inode(sb, S_IFDIR | 0755);
    if (!root)
        return -ENOMEM;
    
    sb->s_root = d_make_root(root);
    if (!sb->s_root)
        return -ENOMEM;
    
    pr_info("example_vfs: superblock created successfully\n");
    return 0;
}

static struct dentry *example_mount(struct file_system_type *fs_type, int flags,
                                   const char *dev_name, void *data)
{
    pr_info("example_vfs: mount called\n");
    return mount_nodev(fs_type, flags, data, example_fill_super);
}

static struct file_system_type example_fs_type = {
    .owner      = THIS_MODULE,
    .name       = "example_vfs",
    .mount      = example_mount,
    .kill_sb    = kill_litter_super,
};

static int __init example_vfs_init(void)
{
    int ret = register_filesystem(&example_fs_type);
    if (ret)
        pr_err("example_vfs: Failed to register filesystem\n");
    else
        pr_info("example_vfs: Filesystem registered\n");
    
    return ret;
}

static void __exit example_vfs_exit(void)
{
    unregister_filesystem(&example_fs_type);
    pr_info("example_vfs: Filesystem unregistered\n");
}

module_init(example_vfs_init);
module_exit(example_vfs_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kernel Programming Course");
MODULE_DESCRIPTION("VFS Objects Example");
MODULE_VERSION("1.0");