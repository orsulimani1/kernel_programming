#include <stdio.h>

/* void write_using_syscall(const char* msg, int len) {
    long result;
    
    asm volatile (
        "movq $1, %%rax\n\t"      // __NR_write = 1 (64-bit)
        "movq $1, %%rdi\n\t"      // stdout fd = 1
        "movq %1, %%rsi\n\t"      // buffer pointer
        "movq %2, %%rdx\n\t"      // count
        "syscall\n\t"             // modern syscall
        "movq %%rax, %0"          // store result
        : "=m"(result)
        : "m"(msg), "r"((long)len)
        : "rax", "rdi", "rsi", "rdx", "rcx", "r11"
    );
    
    printf("Write returned: %ld bytes\n", result);
} */

void write_using_int80_direct(const char* msg, int len) {
    int result;
    
    asm volatile (
        "movl $4, %%eax\n\t"      // __NR_write = 4 (32-bit)
        "movl $1, %%ebx\n\t"      // stdout fd = 1
        "movl %1, %%ecx\n\t"      // buffer pointer (truncated to 32-bit)
        "movl %2, %%edx\n\t"      // count
        "int $0x80\n\t"           // 32-bit syscall interface
        "movl %%eax, %0"          // store result
        : "=m"(result)
        : "m"(msg), "m"(len)
        : "eax", "ebx", "ecx", "edx"
    );
    
    printf("INT 0x80 returned: %d bytes\n", result);
}

int main() {
    char message[] = "Direct INT 0x80 write call\n";
    write_using_int80_direct(message, sizeof(message)-1);
    return 0;
}