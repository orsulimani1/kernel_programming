#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/hdreg.h>
#include <linux/bio.h>
#include <linux/blk-mq.h>

#define SIMPLE_BLOCK_MINORS     16
#define KERNEL_SECTOR_SIZE      512
#define SIMPLE_BLOCK_SIZE       (1024 * 1024)  // 1MB device

// Device structure
struct simple_block_dev {
    int size;                       // Device size in bytes
    u8 *data;                      // Device storage
    struct request_queue *queue;    // Request queue
    struct gendisk *gd;            // Generic disk structure
    struct blk_mq_tag_set tag_set; // Multi-queue tag set
};

static struct simple_block_dev *Device = NULL;
static int major_num = 0;

// Transfer function using bio structure
static void simple_transfer(struct simple_block_dev *dev, sector_t sector,
                           unsigned long nsect, char *buffer, int write)
{
    unsigned long offset = sector * KERNEL_SECTOR_SIZE;
    unsigned long nbytes = nsect * KERNEL_SECTOR_SIZE;
    
    if ((offset + nbytes) > dev->size) {
        pr_notice("simple_block: Beyond-end write (%ld %ld)\n", offset, nbytes);
        return;
    }
    
    if (write)
        memcpy(dev->data + offset, buffer, nbytes);
    else
        memcpy(buffer, dev->data + offset, nbytes);
}

// Handle bio requests with I/O vectors
static void simple_handle_bio(struct bio *bio)
{
    struct simple_block_dev *dev = bio->bi_bdev->bd_disk->private_data;
    struct bio_vec bvec;
    struct bvec_iter iter;
    sector_t sector = bio->bi_iter.bi_sector;
    int dir = bio_data_dir(bio);
    char *buffer;
    
    pr_info("simple_block: bio request - sector: %llu, size: %u, dir: %s\n",
            (unsigned long long)sector, bio->bi_iter.bi_size,
            (dir == WRITE) ? "WRITE" : "READ");
    
    // Iterate through all bio_vec segments
    bio_for_each_segment(bvec, bio, iter) {
        // Get kernel address of the page
        buffer = page_address(bvec.bv_page) + bvec.bv_offset;
        
        pr_info("simple_block: Processing bio_vec - page: %p, len: %u, offset: %u\n",
                bvec.bv_page, bvec.bv_len, bvec.bv_offset);
        
        // Transfer data for this segment
        simple_transfer(dev, sector, bvec.bv_len / KERNEL_SECTOR_SIZE, buffer, dir);
        
        // Move to next sector
        sector += bvec.bv_len / KERNEL_SECTOR_SIZE;
    }
    
    bio_endio(bio);
}

// Multi-queue block driver queue function
static blk_status_t simple_queue_rq(struct blk_mq_hw_ctx *hctx,
                                   const struct blk_mq_queue_data *bd)
{
    struct request *req = bd->rq;
    struct bio *bio;
    
    blk_mq_start_request(req);
    
    // Process all bio structures in the request
    __rq_for_each_bio(bio, req) {
        simple_handle_bio(bio);
    }
    
    blk_mq_end_request(req, BLK_STS_OK);
    return BLK_STS_OK;
}

// Block device operations
static int simple_open(struct block_device *bdev, fmode_t mode)
{
    pr_info("simple_block: Device opened\n");
    return 0;
}

static void simple_release(struct gendisk *disk, fmode_t mode)
{
    pr_info("simple_block: Device released\n");
}

static int simple_ioctl(struct block_device *bdev, fmode_t mode,
                       unsigned int cmd, unsigned long arg)
{
    pr_info("simple_block: ioctl called with cmd: %u\n", cmd);
    return -ENOTTY;
}

// Block device operations structure
static const struct block_device_operations simple_ops = {
    .owner      = THIS_MODULE,
    .open       = simple_open,
    .release    = simple_release,
    .ioctl      = simple_ioctl
};

// Multi-queue operations
static const struct blk_mq_ops simple_mq_ops = {
    .queue_rq = simple_queue_rq,
};

static int __init simple_block_init(void)
{
    int ret;
    
    // Allocate device structure
    Device = kzalloc(sizeof(struct simple_block_dev), GFP_KERNEL);
    if (!Device)
        return -ENOMEM;
    
    // Allocate device storage
    Device->size = SIMPLE_BLOCK_SIZE;
    Device->data = vmalloc(Device->size);
    if (!Device->data) {
        ret = -ENOMEM;
        goto out_free_dev;
    }
    
    // Initialize device data with pattern
    memset(Device->data, 0xAA, Device->size);
    
    // Register block device
    major_num = register_blkdev(0, "simple_block");
    if (major_num < 0) {
        ret = major_num;
        goto out_free_data;
    }
    
    // Initialize tag set for multi-queue
    Device->tag_set.ops = &simple_mq_ops;
    Device->tag_set.nr_hw_queues = 1;
    Device->tag_set.queue_depth = 128;
    Device->tag_set.numa_node = NUMA_NO_NODE;
    Device->tag_set.cmd_size = 0;
    Device->tag_set.flags = BLK_MQ_F_SHOULD_MERGE;
    Device->tag_set.driver_data = Device;
    
    ret = blk_mq_alloc_tag_set(&Device->tag_set);
    if (ret)
        goto out_unreg_blk;
    
    // Allocate request queue
    Device->queue = blk_mq_init_queue(&Device->tag_set);
    if (IS_ERR(Device->queue)) {
        ret = PTR_ERR(Device->queue);
        goto out_free_tag_set;
    }
    
    // Set queue properties
    blk_queue_logical_block_size(Device->queue, KERNEL_SECTOR_SIZE);
    Device->queue->queuedata = Device;
    
    // Allocate and configure gendisk
    Device->gd = blk_mq_alloc_disk(&Device->tag_set, Device);
    if (IS_ERR(Device->gd)) {
        ret = PTR_ERR(Device->gd);
        goto out_free_queue;
    }
    
    Device->gd->major = major_num;
    Device->gd->first_minor = 0;
    Device->gd->minors = SIMPLE_BLOCK_MINORS;
    Device->gd->fops = &simple_ops;
    Device->gd->private_data = Device;
    strcpy(Device->gd->disk_name, "simple_block");
    
    set_capacity(Device->gd, SIMPLE_BLOCK_SIZE / KERNEL_SECTOR_SIZE);
    
    // Add disk to system
    ret = add_disk(Device->gd);
    if (ret)
        goto out_cleanup_disk;
    
    pr_info("simple_block: Registered device with major number %d\n", major_num);
    pr_info("simple_block: Device size: %d bytes (%d sectors)\n",
            Device->size, Device->size / KERNEL_SECTOR_SIZE);
    
    return 0;
    
out_cleanup_disk:
    put_disk(Device->gd);
out_free_queue:
    blk_cleanup_queue(Device->queue);
out_free_tag_set:
    blk_mq_free_tag_set(&Device->tag_set);
out_unreg_blk:
    unregister_blkdev(major_num, "simple_block");
out_free_data:
    vfree(Device->data);
out_free_dev:
    kfree(Device);
    return ret;
}

static void __exit simple_block_exit(void)
{
    if (Device) {
        del_gendisk(Device->gd);
        put_disk(Device->gd);
        blk_cleanup_queue(Device->queue);
        blk_mq_free_tag_set(&Device->tag_set);
        unregister_blkdev(major_num, "simple_block");
        vfree(Device->data);
        kfree(Device);
    }
    
    pr_info("simple_block: Module unloaded\n");
}

module_init(simple_block_init);
module_exit(simple_block_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kernel Programming Course");
MODULE_DESCRIPTION("Simple Block Device with bio and I/O Vectors");
MODULE_VERSION("1.0");