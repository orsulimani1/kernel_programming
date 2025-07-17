#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <errno.h>

#define MOUNT_POINT "/mnt/example_vfs"
#define TEST_FILE "/mnt/example_vfs/testfile"

// Helper function to create directory recursively
int create_dir_recursive(const char *path, mode_t mode)
{
    char *path_copy = strdup(path);
    char *p = path_copy;
    int ret = 0;
    
    // Skip leading slash
    if (*p == '/') p++;
    
    while ((p = strchr(p, '/'))) {
        *p = '\0';
        if (mkdir(path_copy, mode) < 0 && errno != EEXIST) {
            ret = -1;
            goto out;
        }
        *p++ = '/';
    }
    
    // Create final directory
    if (mkdir(path_copy, mode) < 0 && errno != EEXIST) {
        ret = -1;
    }
    
out:
    free(path_copy);
    return ret;
}

int main()
{
    int fd, ret;
    char write_buf[] = "Hello from userspace!";
    char read_buf[100];
    struct stat st;
    
    printf("=== VFS Filesystem Test ===\n");
    
    // First create /mnt if it doesn't exist
    ret = create_dir_recursive("/mnt", 0755);
    if (ret < 0) {
        perror("Failed to create /mnt");
        return 1;
    }
    
    // Create mount point
    ret = mkdir(MOUNT_POINT, 0755);
    if (ret < 0 && errno != EEXIST) {
        perror("mkdir");
        return 1;
    }
    
    // Mount our filesystem
    printf("Mounting example_vfs at %s...\n", MOUNT_POINT);
    ret = mount("none", MOUNT_POINT, "example_vfs", 0, NULL);
    if (ret < 0) {
        perror("mount");
        printf("Make sure example_vfs module is loaded\n");
        printf("Try: lsmod | grep simple_vfs\n");
        printf("And: cat /proc/filesystems | grep example_vfs\n");
        return 1;
    }
    
    printf("Filesystem mounted successfully\n");
    
    // Test file operations
    printf("\nTesting file operations:\n");
    
    // Open file (should trigger lookup and create)
    printf("Opening %s...\n", TEST_FILE);
    fd = open(TEST_FILE, O_RDWR | O_CREAT, 0644);
    if (fd < 0) {
        perror("open");
        goto cleanup;
    }
    
    // Write to file
    printf("Writing data to file...\n");
    ret = write(fd, write_buf, strlen(write_buf));
    if (ret < 0) {
        perror("write");
        close(fd);
        goto cleanup;
    }
    printf("Wrote %d bytes: %s\n", ret, write_buf);
    
    // Reset file position
    lseek(fd, 0, SEEK_SET);
    
    // Read from file
    printf("Reading data from file...\n");
    memset(read_buf, 0, sizeof(read_buf));
    ret = read(fd, read_buf, sizeof(read_buf) - 1);
    if (ret < 0) {
        perror("read");
        close(fd);
        goto cleanup;
    }
    read_buf[ret] = '\0';
    printf("Read %d bytes: %s\n", ret, read_buf);
    
    close(fd);
    
    // Test file stat
    printf("\nTesting file stat...\n");
    ret = stat(TEST_FILE, &st);
    if (ret < 0) {
        perror("stat");
        goto cleanup;
    }
    
    printf("File size: %ld bytes\n", st.st_size);
    printf("File mode: 0%o\n", st.st_mode & 0777);
    printf("File inode: %lu\n", st.st_ino);
    
    // Test directory listing
    printf("\nTesting directory listing:\n");
    printf("Contents of %s:\n", MOUNT_POINT);
    
    // Use a simple approach since we might not have ls
    DIR *dir = opendir(MOUNT_POINT);
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            printf("  %s\n", entry->d_name);
        }
        closedir(dir);
    } else {
        // Fallback to system ls if available
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "ls -la %s", MOUNT_POINT);
        system(cmd);
    }
    
cleanup:
    // Unmount filesystem
    printf("\nUnmounting filesystem...\n");
    ret = umount(MOUNT_POINT);
    if (ret < 0) {
        perror("umount");
        printf("You may need to manually unmount: umount %s\n", MOUNT_POINT);
        return 1;
    }
    
    printf("VFS test completed successfully\n");
    return 0;
}
