# Lab 2: Adding Custom System Calls

## Objective
Learn how to add a custom system call to the Linux kernel and test it from user space.

## Prerequisites
- Completed Lab 1
- Kernel source built and working
- Basic understanding of system calls

## Step 1: Implement the Function

Create the system call implementation:

```c
// kernel/my_syscall.c
#include <linux/syscalls.h>
#include <linux/kernel.h>

SYSCALL_DEFINE2(hello_user, const char __user *, name, int, age)
{
    char kname[256];
    
    // Copy string from user space
    if (strncpy_from_user(kname, name, sizeof(kname)) < 0)
        return -EFAULT;
    
    pr_info("Hello %s, age %d!\n", kname, age);
    return 0;
}
```
## Step 1.5: Add to Build System
Add your syscall file to the kernel build:
makefile# Edit kernel/Makefile
Find the line with other kernel objects and add:
```makefile
obj-y += my_syscall.o
```
Alternative: Add directly to existing file:

## Step 2: Add to System Call Table

Edit the system call table:

```c
// arch/x86/entry/syscalls/syscall_64.tbl
548    common  hello_user    sys_hello_user
```

## Step 3: Add Header Declaration

Add function declaration to syscalls header:

```c
// include/linux/syscalls.h
asmlinkage long sys_hello_user(const char __user *name, int age);
```

## Step 4: Updated NR_syscalls

see if the maximum syscall number is updated:

```c
// arch/x86/include/asm/unistd.h
#define __NR_syscalls 549  // Increment by 1
```

## Step 5: User Space Test Program

Create test program:

```c
// test_hello.c
#include <unistd.h>
#include <sys/syscall.h>
#include <stdio.h>

#define __NR_hello_user 548

int main() {
    long result = syscall(__NR_hello_user, "Alice", 25);
    printf("Syscall returned: %ld\n", result);
    return 0;
}
```

## Step 6: Build and Test

Build kernel and test:

```bash
# Rebuild kernel with new syscall
cd ${KERNEL_DIR}
make -j$(nproc)

# Build test program
gcc -o test_hello test_hello.c

# Test in QEMU
./test_hello
dmesg | tail  # Check kernel output
```

## Expected Output

**User space:**
```
Syscall returned: 0
```

**Kernel logs (dmesg):**
```
Hello Alice, age 25!
```

## Lab Exercises

### Exercise 1: Basic System Call
Follow the steps above to implement and test the `hello_user` system call.

### Exercise 2: Calculator System Call
Implement a system call that performs arithmetic operations:

```c
SYSCALL_DEFINE3(calc, int, a, int, b, int, op)
{
    switch (op) {
    case 0: return a + b;  // ADD
    case 1: return a - b;  // SUB
    case 2: return a * b;  // MUL
    case 3: return (b != 0) ? a / b : -EINVAL;  // DIV
    default: return -EINVAL;
    }
}
```

### Exercise 3: String Manipulation
Create a system call that reverses a string:

```c
SYSCALL_DEFINE2(reverse_string, char __user *, str, size_t, len)
{
    char *kstr;
    int i;
    
    kstr = kmalloc(len + 1, GFP_KERNEL);
    if (!kstr)
        return -ENOMEM;
    
    if (copy_from_user(kstr, str, len)) {
        kfree(kstr);
        return -EFAULT;
    }
    
    // Reverse the string
    for (i = 0; i < len / 2; i++) {
        char temp = kstr[i];
        kstr[i] = kstr[len - 1 - i];
        kstr[len - 1 - i] = temp;
    }
    
    if (copy_to_user(str, kstr, len)) {
        kfree(kstr);
        return -EFAULT;
    }
    
    kfree(kstr);
    return len;
}
```

## Common Issues

### Build Errors
- Check syscall number conflicts
- Verify all files are modified correctly
- Ensure proper header includes

### Runtime Errors
- `ENOSYS`: Syscall number not found in table
- `EFAULT`: Invalid user space pointer
- `EINVAL`: Invalid parameters

### Debugging Tips
```bash
# Check syscall table
grep hello_user arch/x86/entry/syscalls/syscall_64.tbl

# Verify syscall number
grep __NR_hello_user arch/x86/include/generated/asm/unistd_64.h

# Monitor kernel logs
dmesg -w
```

## Deliverables

1. Working `hello_user` system call
2. Test program demonstrating functionality
3. At least one additional custom system call
4. Documentation of any issues encountered

## Bonus Challenges not learned yet

1. Implement system call with complex data structures
2. Add parameter validation and error handling
3. Create system call that interacts with kernel data structures 
4. Implement system call for process information retrieval

## Resources

- Linux System Call Reference
- Kernel source: `kernel/` directory
- System call table: `arch/x86/entry/syscalls/`
- Headers: `include/linux/syscalls.h`