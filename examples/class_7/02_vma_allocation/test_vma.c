#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>

#define DEVICE_PATH "/dev/simple_vma"
#define BUFFER_SIZE (4096 * 4)  // 4 pages

int main()
{
    int fd, ret;
    char *mapped_mem;
    char write_buf[] = "Hello from userspace via mmap!";
    char read_buf[256];
    
    printf("=== VMA Memory Mapping Test ===\n");
    
    // Open device
    printf("Opening device %s...\n", DEVICE_PATH);
    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("open");
        printf("Make sure simple_vma module is loaded and device exists\n");
        return 1;
    }
    
    printf("Device opened successfully\n");
    
    // Test regular read/write first
    printf("\nTesting regular file operations:\n");
    
    // Write via regular write system call
    printf("Writing via write() system call...\n");
    ret = write(fd, write_buf, strlen(write_buf));
    if (ret < 0) {
        perror("write");
        close(fd);
        return 1;
    }
    printf("Wrote %d bytes: %s\n", ret, write_buf);
    
    // Read back via regular read
    lseek(fd, 0, SEEK_SET);
    memset(read_buf, 0, sizeof(read_buf));
    ret = read(fd, read_buf, sizeof(read_buf) - 1);
    if (ret > 0) {
        read_buf[ret] = '\0';
        printf("Read %d bytes: %s\n", ret, read_buf);
    }
    
    // Test memory mapping
    printf("\nTesting memory mapping:\n");
    
    // Map device memory
    printf("Mapping device memory...\n");
    mapped_mem = mmap(NULL, BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapped_mem == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return 1;
    }
    
    printf("Memory mapped successfully at address: %p\n", mapped_mem);
    
    // Read initial content via mmap
    printf("Initial content via mmap: %.50s\n", mapped_mem);
    
    // Write via memory mapping
    printf("Writing via memory mapping...\n");
    strcpy(mapped_mem, "Data written via mmap - direct memory access!");
    printf("Wrote: %s\n", mapped_mem);
    
    // Test page fault by accessing different pages
    printf("\nTesting page faults across multiple pages:\n");
    for (int i = 0; i < 4; i++) {
        char *page_addr = mapped_mem + (i * 4096);
        sprintf(page_addr, "Page %d content via mmap", i);
        printf("Page %d: %s\n", i, page_addr);
    }
    
    // Read back via regular read to verify
    printf("\nVerifying via regular read:\n");
    lseek(fd, 0, SEEK_SET);
    memset(read_buf, 0, sizeof(read_buf));
    ret = read(fd, read_buf, sizeof(read_buf) - 1);
    if (ret > 0) {
        read_buf[ret] = '\0';
        printf("Read back: %s\n", read_buf);
    }
    
    // Test writing large data
    printf("\nTesting large data write via mmap:\n");
    char large_data[1024];
    snprintf(large_data, sizeof(large_data), 
             "Large data block: %s. This tests the VMA fault handler with larger writes.",
             "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    
    if (strlen(large_data) < BUFFER_SIZE) {
        strcpy(mapped_mem, large_data);
        printf("Large write successful: %.80s...\n", mapped_mem);
    }
    
    // Cleanup
    printf("\nCleaning up...\n");
    ret = munmap(mapped_mem, BUFFER_SIZE);
    if (ret < 0) {
        perror("munmap");
    } else {
        printf("Memory unmapped successfully\n");
    }
    
    close(fd);
    printf("VMA test completed successfully\n");
    return 0;
}