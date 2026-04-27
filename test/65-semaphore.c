#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include "thread.h"
thread_sem_t sem;
int counter = 0;
void * worker(void *arg) {
    thread_sem_wait(&sem);
    //section critique
    counter++;
    printf("Thread %ld est passé\n", (long)arg);
    thread_sem_post(&sem);
    return NULL;
}
int main(int argc, char *argv[]) {
    struct timeval tv1, tv2;
    if (argc < 2) {
        printf("argument manquant: nombre de threads\n");
        return EXIT_FAILURE;
    }
    int nb = atoi(argv[1]);
    thread_t ths[nb];
    thread_sem_init(&sem, nb);
    gettimeofday(&tv1, NULL);
    for(long i=0; i<nb; i++)
        thread_create(&ths[i], worker, (void*)i);
    for(int i=0; i<nb; i++)
        thread_join(ths[i], NULL);
    assert(counter == nb);
    gettimeofday(&tv2, NULL);
    long us = (tv2.tv_sec - tv1.tv_sec) * 1000000 + (tv2.tv_usec - tv1.tv_usec);
    thread_sem_destroy(&sem);
    printf("Test Sémaphore réussi !\n");
    printf("Temps d'exécution: %ld us\n", us);
    return 0;
}