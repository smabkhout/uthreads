#include <stdio.h>
#include <stdlib.h>
#include "thread.h"

extern void thread_init(void);


void *infinite_recursion(void *arg) {
    volatile char buffer[1024]; //volatile to prevent the compiler from optimizing, it needs to be allocated on the stack
    buffer[0] = 1;
    (void)buffer;
    
    //to suppress the infinite recursion warning
    void *(*func)(void *) = infinite_recursion;
    return func(arg); 
}

int main() {
    thread_init(); //initialize the signal handler
    thread_t th;
    printf("Starting stack overflow test...\n");
    thread_create(&th, infinite_recursion, NULL);
    thread_join(th, NULL);
    return 0;
}