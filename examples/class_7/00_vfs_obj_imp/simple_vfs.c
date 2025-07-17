#include <linux/module.h>
#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/mount.h>
#include <linux/namei.h>

#define SIMPLE_MAGIC 0x19980122

// Forward declarations
static struct inode *simple_alloc_inode(struct super_block *sb);
static void simple_destroy_inode(struct inode *inode);
static int simple_statfs(struct dentry *dentry, struct kstatfs *buf);

// 1. Superblock Operations
static const struct super_operations simple_super_ops = {
    .alloc_inode    = simple_alloc_inode,
    .destroy_inode  = simple_destroy_inode,
    .statfs         = simple_statfs,
};

// Simple inode structure
struct simple_inode_info {
    struct inode vfs_inode;
    char data[64];  // Simple data storage
};

// 2. Inode Operations
static struct dentry *simple_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags);
static int simple_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl);
static int simple_unlink(struct inode *dir, struct dentry *dentry);

static const struct inode_operations simple_dir_inode_ops = {
    .lookup = simple_lookup,
    .create = simple_create,
    .unlink = simple_unlink,
};

// 3. File Operations
static ssize_t simple_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);
static ssize_t simple_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos);
static int simple_open(struct inode *inode, struct file *file);

static const struct file_operations simple_file_ops = {
    .open    = simple_open,
    .read    = simple_read,
    .write   = simple_write,
    .llseek  = default_llseek,
};

// Directory file operations
static int simple_readdir(struct file *file, struct dir_context *ctx);

static const struct file_operations simple_dir_ops = {
    .open       = dcache_dir_open,
    .release    = dcache_dir_close,
    .llseek     = dcache_dir_lseek,
    .read       = generic_read_dir,
    .iterate    = simple_readdir,
};

// Helper function to get simple_inode_info from inode
static inline struct simple_inode_info *SIMPLE_I(struct inode *inode)
{
    return container_of(inode, struct simple_inode_info, vfs_inode);
}

// Superblock Operations Implementation
static struct inode *simple_alloc_inode(struct super_block *sb)
{
    struct simple_inode_info *ei;
    
    ei = kmalloc(sizeof(struct simple_inode_info), GFP_KERNEL);
    if (!ei)
        return NULL;
    
    memset(ei->data, 0, sizeof(ei->data));
    return &ei->vfs_inode;
}

static void simple_destroy_inode(struct inode *inode)
{
    kfree(SIMPLE_I(inode));
}

static int simple_statfs(struct dentry *dentry, struct kstatfs *buf)
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
static struct inode *simple_get_inode(struct super_block *sb, umode_t mode)
{
    struct inode *inode = new_inode(sb);
    
    if (inode) {
        inode->i_ino = get_next_ino();
        inode->i_mode = mode;
        inode->i_uid = current_fsuid();
        inode->i_gid = current_fsgid();
        inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode);
        
        switch (mode & S_IFMT) {
        case S_IFREG:
            inode->i_op = &simple_page_symlink_inode_operations;
            inode->i_fop = &simple_file_ops;
            break;
        case S_IFDIR:
            inode->i_op = &simple_dir_inode_ops;
            inode->i_fop = &simple_dir_ops;
            inc_nlink(inode);
            break;
        }
    }
    return inode;
}

static struct dentry *simple_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags)
{
    // For demonstration, we'll create a simple file on lookup
    if (strcmp(dentry->d_name.name, "testfile") == 0) {
        struct inode *inode = simple_get_inode(dir->i_sb, S_IFREG | 0644);
        if (inode) {
            d_add(dentry, inode);
            return NULL;
        }
    }
    
    // File not found
    d_add(dentry, NULL);
    return NULL;
}

static int simple_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl)
{
    struct inode *inode = simple_get_inode(dir->i_sb, mode | S_IFREG);
    
    if (!inode)
        return -ENOSPC;
    
    d_instantiate(dentry, inode);
    dget(dentry);
    dir->i_mtime = dir->i_ctime = current_time(dir);
    
    return 0;
}

static int simple_unlink(struct inode *dir, struct dentry *dentry)
{
    struct inode *inode = d_inode(dentry);
    
    inode->i_ctime = dir->i_ctime = dir->i_mtime = current_time(inode);
    drop_nlink(inode);
    dput(dentry);
    
    return 0;
}

// File Operations Implementation
static int simple_open(struct inode *inode, struct file *file)
{
    pr_info("simple_vfs: File opened\n");
    return 0;
}

static ssize_t simple_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    struct inode *inode = file_inode(file);
    struct simple_inode_info *ei = SIMPLE_I(inode);
    size_t len = strlen(ei->data);
    
    if (*ppos >= len)
        return 0;
    
    if (*ppos + count > len)
        count = len - *ppos;
    
    if (copy_to_user(buf, ei->data + *ppos, count))
        return -EFAULT;
    
    *ppos += count;
    return count;
}

static ssize_t simple_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    struct inode *inode = file_inode(file);
    struct simple_inode_info *ei = SIMPLE_I(inode);
    
    if (count > sizeof(ei->data) - 1)
        count = sizeof(ei->data) - 1;
    
    if (copy_from_user(ei->data, buf, count))
        return -EFAULT;
    
    ei->data[count] = '\0';
    inode->i_size = count;
    
    return count;
}

// Directory Operations Implementation
static int simple_readdir(struct file *file, struct dir_context *ctx)
{
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
static int simple_fill_super(struct super_block *sb, void *data, int silent)
{
    struct inode *root;
    
    sb->s_blocksize = PAGE_SIZE;
    sb->s_blocksize_bits = PAGE_SHIFT;
    sb->s_magic = SIMPLE_MAGIC;
    sb->s_op = &simple_super_ops;
    
    root = simple_get_inode(sb, S_IFDIR | 0755);
    if (!root)
        return -ENOMEM;
    
    sb->s_root = d_make_root(root);
    if (!sb->s_root)
        return -ENOMEM;
    
    return 0;
}

static struct dentry *simple_mount(struct file_system_type *fs_type, int flags,
                                  const char *dev_name, void *data)
{
    return mount_nodev(fs_type, flags, data, simple_fill_super);
}

static struct file_system_type simple_fs_type = {
    .owner      = THIS_MODULE,
    .name       = "simple_vfs",
    .mount      = simple_mount,
    .kill_sb    = kill_litter_super,
};

static int __init simple_vfs_init(void)
{
    int ret = register_filesystem(&simple_fs_type);
    if (ret)
        pr_err("simple_vfs: Failed to register filesystem\n");
    else
        pr_info("simple_vfs: Filesystem registered\n");
    
    return ret;
}

static void __exit simple_vfs_exit(void)
{
    unregister_filesystem(&simple_fs_type);
    pr_info("simple_vfs: Filesystem unregistered\n");
}

module_init(simple_vfs_init);
module_exit(simple_vfs_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kernel Programming Course");
MODULE_DESCRIPTION("Simple VFS Objects Example");
MODULE_VERSION("1.0");