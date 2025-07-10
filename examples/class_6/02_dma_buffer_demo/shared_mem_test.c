/*
 * examples/class_6/02_shared_memory_demo/shared_mem_test.c
 * 
 * User space test for shared memory demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#define DEVICE_PATH "/dev/shared_mem"
#define BUFFER_SIZE (8 * 1024)
#define MSG_SIZE (4 * 1024)

/* IOCTL command */
#define SHARED_MEM_IOC_MAGIC 'S'
#define SHARED_MEM_SEND_MSG _IO(SHARED_MEM_IOC_MAGIC, 1)

int main(int argc, char *argv[])
{
    int fd;
    void *buffer;
    char *user_to_kernel;
    char *kernel_to_user;
    char message[512];
    
    if (argc < 2) {
        printf("Usage: %s <message>\n", argv[0]);
        printf("   or: %s -i  (interactive mode)\n", argv[0]);
        return 1;
    }
    
    /* Open device */
    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("Failed to open device");
        return 1;
    }
    
    /* Map shared buffer */
    buffer = mmap(NULL, BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (buffer == MAP_FAILED) {
        perror("mmap failed");
        close(fd);
        return 1;
    }
    
    /* Setup pointers */
    user_to_kernel = (char *)buffer;
    kernel_to_user = (char *)buffer + MSG_SIZE;
    
    if (strcmp(argv[1], "-i") == 0) {
        /* Interactive mode */
        printf("Interactive shared memory demo. Type 'quit' to exit.\n");
        while (1) {
            printf("Enter message: ");
            if (!fgets(message, sizeof(message), stdin))
                break;
            
            message[strcspn(message, "\n")] = '\0';
            
            if (strcmp(message, "quit") == 0)
                break;
            
            /* Clear response area */
            memset(kernel_to_user, 0, MSG_SIZE);
            
            /* Write message */
            strncpy(user_to_kernel, message, MSG_SIZE - 1);
            user_to_kernel[MSG_SIZE - 1] = '\0';
            
            /* Send to kernel */
            if (ioctl(fd, SHARED_MEM_SEND_MSG) < 0) {
                perror("ioctl failed");
                break;
            }
            
            /* Read response */
            printf("Response: %s\n\n", kernel_to_user);
        }
    } else {
        /* Single message mode */
        strncpy(message, argv[1], sizeof(message) - 1);
        message[sizeof(message) - 1] = '\0';
        
        printf("Sending: \"%s\"\n", message);
        
        /* Clear response area */
        memset(kernel_to_user, 0, MSG_SIZE);
        
        /* Write message */
        strncpy(user_to_kernel, message, MSG_SIZE - 1);
        user_to_kernel[MSG_SIZE - 1] = '\0';
        
        /* Send to kernel */
        if (ioctl(fd, SHARED_MEM_SEND_MSG) < 0) {
            perror("ioctl failed");
            munmap(buffer, BUFFER_SIZE);
            close(fd);
            return 1;
        }
        
        /* Read response */
        printf("Response: %s\n", kernel_to_user);
    }
    
    /* Cleanup */
    munmap(buffer, BUFFER_SIZE);
    close(fd);
    return 0;
}