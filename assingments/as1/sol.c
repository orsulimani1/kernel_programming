static void swap_asm(unsigned long *a, unsigned long *b)
{
    asm volatile ("movq %0, %%rax\n\t"      // Load address of a
                 "movq %1, %%rbx\n\t"      // Load address of b
                 "movq (%%rax), %%rcx\n\t" // Load value of *a
                 "movq (%%rbx), %%rdx\n\t" // Load value of *b
                 "movq %%rdx, (%%rax)\n\t" // Store *b into *a
                 "movq %%rcx, (%%rbx)"     // Store *a into *b
                 :
                 : "m"(a), "m"(b)
                 : "rax", "rbx", "rcx", "rdx", "memory");
}