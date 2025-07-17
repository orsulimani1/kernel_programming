#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <errno.h>

#define DEVICE_PATH "/dev/simple_block"
#define SECTOR_SIZE 512
#define TEST_DATA "This is test data for our simple block device!"

int main()
{
    int fd, ret;
    char write_buf[SECTOR_SIZE];
    char read_buf[SECTOR_SIZE];
    unsigned long long size;
    
    printf("=== Block Device Test ===\n");
    
    // Open block device
    printf("Opening block device %s...\n", DEVICE_PATH);
    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("open");
        printf("Make sure simple_block module is loaded and device exists\n");
        return 1;
    }
    
    printf("Block device opened successfully\n");
    
    // Get device size
    ret = ioctl(fd, BLKGETSIZE64, &size);
    if (ret == 0) {
        printf("Device size: %llu bytes (%llu sectors)\n", 
               size, size / SECTOR_SIZE);
    }
    
    // Prepare test data
    memset(write_buf, 0, sizeof(write_buf));
    strncpy(write_buf, TEST_DATA, strlen(TEST_DATA));
    
    printf("\nTesting block I/O operations:\n");
    
    // Write to first sector
    printf("Writing to sector 0...\n");
    ret = write(fd, write_buf, SECTOR_SIZE);
    if (ret < 0) {
        perror("write");
        close(fd);
        return 1;
    }
    printf("Wrote %d bytes to block device\n", ret);
    
    // Seek back to beginning
    lseek(fd, 0, SEEK_SET);
    
    // Read from first sector
    printf("Reading from sector 0...\n");
    memset(read_buf, 0, sizeof(read_buf));
    ret = read(fd, read_buf, SECTOR_SIZE);
    if (ret < 0) {
        perror("read");
        close(fd);
        return 1;
    }
    printf("Read %d bytes from block device\n", ret);
    printf("Data: %s\n", read_buf);
    
    // Verify data
    if (strncmp(write_buf, read_buf, strlen(TEST_DATA)) == 0) {
        printf("✓ Data verification successful\n");
    } else {
        printf("✗ Data verification failed\n");
    }
    
    // Test writing to different sectors
    printf("\nTesting multiple sector writes...\n");
    for (int i = 1; i < 5; i++) {
        lseek(fd, i * SECTOR_SIZE, SEEK_SET);
        snprintf(write_buf, sizeof(write_buf), "Sector %d data", i);
        ret = write(fd, write_buf, SECTOR_SIZE);
        if (ret > 0) {
            printf("Wrote to sector %d: %s\n", i, write_buf);
        }
    }
    
    // Read back multiple sectors
    printf("\nReading back multiple sectors...\n");
    for (int i = 1; i < 5; i++) {
        lseek(fd, i * SECTOR_SIZE, SEEK_SET);
        memset(read_buf, 0, sizeof(read_buf));
        ret = read(fd, read_buf, SECTOR_SIZE);
        if (ret > 0) {
            printf("Read from sector %d: %s\n", i, read_buf);
        }
    }
    
    // Test large I/O (multiple sectors)
    printf("\nTesting large I/O (4KB write)...\n");
    char large_buf[4096];
    memset(large_buf, 0x55, sizeof(large_buf));
    strcpy(large_buf, "Large I/O test - 4KB of data");
    
    lseek(fd, 10 * SECTOR_SIZE, SEEK_SET);
    ret = write(fd, large_buf, sizeof(large_buf));
    if (ret > 0) {
        printf("Large write successful: %d bytes\n", ret);
    }
    
    close(fd);
    printf("\nBlock device test completed successfully\n");
    return 0;
}
