#include <unistd.h>
#include <sys/syscall.h>
#include <stdio.h>
// gcc -o test_hello test_hello.c

#define __NR_hello_user 548

int main() {
    long result = syscall(__NR_hello_user, "Alice", 25);
    printf("Syscall returned: %ld\n", result);
    return 0;
}