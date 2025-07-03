// ioctl_test.c - User space test program
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>

// IOCTL command definitions (must match kernel module)
#define IOCTL_DEMO_MAGIC 'D'
#define IOCTL_SET_VALUE    _IOW(IOCTL_DEMO_MAGIC, 1, int)
#define IOCTL_GET_VALUE    _IOR(IOCTL_DEMO_MAGIC, 2, int)
#define IOCTL_SET_STRING   _IOW(IOCTL_DEMO_MAGIC, 3, char*)
#define IOCTL_GET_STRING   _IOR(IOCTL_DEMO_MAGIC, 4, char*)
#define IOCTL_RESET        _IO(IOCTL_DEMO_MAGIC, 5)

#define DEVICE_PATH "/dev/ioctl_demo"

void print_usage(const char *program_name)
{
    printf("Usage: %s <command> [value]\n", program_name);
    printf("Commands:\n");
    printf("  set_value <number>   - Set integer value\n");
    printf("  get_value           - Get integer value\n");
    printf("  set_string <text>   - Set string value\n");
    printf("  get_string          - Get string value\n");
    printf("  reset               - Reset to defaults\n");
    printf("  test_all            - Run all tests\n");
}

int test_value_operations(int fd)
{
    int value, ret;
    
    printf("\n=== Testing Value Operations ===\n");
    
    // Set value
    value = 12345;
    printf("Setting value to %d...\n", value);
    ret = ioctl(fd, IOCTL_SET_VALUE, &value);
    if (ret < 0) {
        perror("Failed to set value");
        return -1;
    }
    
    // Get value
    value = 0;
    printf("Getting value...\n");
    ret = ioctl(fd, IOCTL_GET_VALUE, &value);
    if (ret < 0) {
        perror("Failed to get value");
        return -1;
    }
    printf("Retrieved value: %d\n", value);
    
    return 0;
}

int test_string_operations(int fd)
{
    char string[256];
    int ret;
    
    printf("\n=== Testing String Operations ===\n");
    
    // Set string
    strcpy(string, "Hello from user space!");
    printf("Setting string to '%s'...\n", string);
    ret = ioctl(fd, IOCTL_SET_STRING, string);
    if (ret < 0) {
        perror("Failed to set string");
        return -1;
    }
    
    // Get string
    memset(string, 0, sizeof(string));
    printf("Getting string...\n");
    ret = ioctl(fd, IOCTL_GET_STRING, string);
    if (ret < 0) {
        perror("Failed to get string");
        return -1;
    }
    printf("Retrieved string: '%s'\n", string);
    
    return 0;
}

int test_reset_operation(int fd)
{
    int ret, value;
    char string[256];
    
    printf("\n=== Testing Reset Operation ===\n");
    
    // Reset device
    printf("Resetting device...\n");
    ret = ioctl(fd, IOCTL_RESET);
    if (ret < 0) {
        perror("Failed to reset device");
        return -1;
    }
    
    // Verify reset values
    ret = ioctl(fd, IOCTL_GET_VALUE, &value);
    if (ret < 0) {
        perror("Failed to get value after reset");
        return -1;
    }
    
    ret = ioctl(fd, IOCTL_GET_STRING, string);
    if (ret < 0) {
        perror("Failed to get string after reset");
        return -1;
    }
    
    printf("After reset - Value: %d, String: '%s'\n", value, string);
    return 0;
}

int main(int argc, char *argv[])
{
    int fd, ret = 0;
    int value;
    char string[256];
    
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    // Open device
    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("Failed to open device");
        printf("Make sure the ioctl_demo module is loaded\n");
        return 1;
    }
    
    printf("Successfully opened %s\n", DEVICE_PATH);
    
    // Parse command
    if (strcmp(argv[1], "set_value") == 0) {
        if (argc != 3) {
            printf("Error: set_value requires a number\n");
            ret = 1;
            goto cleanup;
        }
        value = atoi(argv[2]);
        ret = ioctl(fd, IOCTL_SET_VALUE, &value);
        if (ret < 0) {
            perror("IOCTL_SET_VALUE failed");
            ret = 1;
        } else {
            printf("Value set to %d\n", value);
        }
    }
    else if (strcmp(argv[1], "get_value") == 0) {
        ret = ioctl(fd, IOCTL_GET_VALUE, &value);
        if (ret < 0) {
            perror("IOCTL_GET_VALUE failed");
            ret = 1;
        } else {
            printf("Current value: %d\n", value);
        }
    }
    else if (strcmp(argv[1], "set_string") == 0) {
        if (argc != 3) {
            printf("Error: set_string requires a string\n");
            ret = 1;
            goto cleanup;
        }
        strncpy(string, argv[2], sizeof(string) - 1);
        string[sizeof(string) - 1] = '\0';
        ret = ioctl(fd, IOCTL_SET_STRING, string);
        if (ret < 0) {
            perror("IOCTL_SET_STRING failed");
            ret = 1;
        } else {
            printf("String set to '%s'\n", string);
        }
    }
    else if (strcmp(argv[1], "get_string") == 0) {
        ret = ioctl(fd, IOCTL_GET_STRING, string);
        if (ret < 0) {
            perror("IOCTL_GET_STRING failed");
            ret = 1;
        } else {
            printf("Current string: '%s'\n", string);
        }
    }
    else if (strcmp(argv[1], "reset") == 0) {
        ret = ioctl(fd, IOCTL_RESET);
        if (ret < 0) {
            perror("IOCTL_RESET failed");
            ret = 1;
        } else {
            printf("Device reset successfully\n");
        }
    }
    else if (strcmp(argv[1], "test_all") == 0) {
        printf("Running comprehensive IOCTL tests...\n");
        
        if (test_value_operations(fd) < 0) {
            ret = 1;
            goto cleanup;
        }
        
        if (test_string_operations(fd) < 0) {
            ret = 1;
            goto cleanup;
        }
        
        if (test_reset_operation(fd) < 0) {
            ret = 1;
            goto cleanup;
        }
        
        printf("\n=== All tests completed successfully! ===\n");
    }
    else {
        printf("Error: Unknown command '%s'\n", argv[1]);
        print_usage(argv[0]);
        ret = 1;
    }

cleanup:
    close(fd);
    return ret;
}