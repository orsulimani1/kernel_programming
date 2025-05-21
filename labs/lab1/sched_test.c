/**
 * sched_test.c - A program to set process scheduling policy
 * 
 * Usage: ./sched_test [SCHED_OTHER|SCHED_FIFO|SCHED_RR] [priority]
 * 
 * Examples:
 *   ./sched_test SCHED_OTHER 0
 *   ./sched_test SCHED_FIFO 99
 *   ./sched_test SCHED_RR 50
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <errno.h>
#include <time.h>

int main(int argc, char *argv[]) {
    int policy = -1;
    int priority = 0;
    struct sched_param param;
    
    if (argc != 3) {
        fprintf(stderr, "Usage: %s [SCHED_OTHER|SCHED_FIFO|SCHED_RR] [priority]\n", argv[0]);
        return 1;
    }
    
    /* Parse scheduler type */
    if (strcmp(argv[1], "SCHED_OTHER") == 0) {
        policy = SCHED_OTHER;
    } else if (strcmp(argv[1], "SCHED_FIFO") == 0) {
        policy = SCHED_FIFO;
    } else if (strcmp(argv[1], "SCHED_RR") == 0) {
        policy = SCHED_RR;
    } else {
        fprintf(stderr, "Invalid scheduler type. Use SCHED_OTHER, SCHED_FIFO, or SCHED_RR\n");
        return 1;
    }
    
    /* Parse priority */
    priority = atoi(argv[2]);
    
    /* Check priority limits */
    if ((policy == SCHED_FIFO || policy == SCHED_RR) && 
        (priority < sched_get_priority_min(policy) || priority > sched_get_priority_max(policy))) {
        fprintf(stderr, "Priority out of range for %s (%d-%d)\n", 
                argv[1], sched_get_priority_min(policy), sched_get_priority_max(policy));
        return 1;
    }
    
    /* Set scheduling parameters */
    param.sched_priority = priority;
    
    /* Set scheduler policy for the current process */
    if (sched_setscheduler(0, policy, &param) == -1) {
        perror("sched_setscheduler failed");
        if (errno == EPERM) {
            fprintf(stderr, "Permission denied. Try running with sudo for real-time policies.\n");
        }
        return 1;
    }
    
    /* Get current scheduling policy to verify it was set */
    int current_policy = sched_getscheduler(0);
    struct sched_param current_param;
    sched_getparam(0, &current_param);
    
    printf("Process %d scheduler changed to: ", getpid());
    switch (current_policy) {
        case SCHED_OTHER:
            printf("SCHED_OTHER");
            break;
        case SCHED_FIFO:
            printf("SCHED_FIFO");
            break;
        case SCHED_RR:
            printf("SCHED_RR");
            break;
        default:
            printf("Unknown (%d)", current_policy);
    }
    printf(", priority: %d\n", current_param.sched_priority);
    
    /* Run a simple CPU-bound loop to demonstrate scheduling */
    printf("Running CPU-bound loop for 5 seconds...\n");
    unsigned long long count = 0;
    time_t start_time = time(NULL);
    while (time(NULL) < start_time + 5) {
        count++;
    }
    printf("Loop finished, iterations: %llu\n", count);
    
    return 0;
}