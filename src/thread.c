#include "thread.h"
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <sys/queue.h>

#define SizeStack	8192

enum threadState {
    READY,
    RUNNING,
    BLOCKED,
    TERMINATED 
};

struct thread_s {
    ucontext_t context;
    void *stack;
    enum threadState state;
    void *retval;
    struct thread_s *joiningThread; 
    TAILQ_ENTRY(thread_s) entries; // reference au thread suivant et thread precedent
};

typedef struct thread_s thread_s;

// on definit le type des listes double chainées qu'on va utiliser
TAILQ_HEAD(threadQueue, thread_s);

struct threadQueue readyQueue;
struct threadQueue blockedQueue;

thread_s* currentThread;

//the main function should be the first thread created
void thread_init() {
    // on initialise la readyqueue à NULL en debut
    (&readyQueue)->tqh_first = NULL; 
    (&readyQueue)->tqh_last = &(&readyQueue)->tqh_first;

    currentThread = (thread_s*) malloc(sizeof(thread_s));
    if (currentThread == NULL) {
        perror("Erreur d'allocation pour le thread principal");
        exit(EXIT_FAILURE);
    }

    currentThread->state = RUNNING;
    currentThread->joiningThread = NULL;
    currentThread->stack = NULL; // pile principale

    // sauvegarder le contexte actuel d'exécution dans ce thread
    if (getcontext(&currentThread->context) == -1) {
        perror("Erreur lors du getcontext du main");
        exit(EXIT_FAILURE);
    }
}

int thread_create(thread_t *newthread, void *(*start_routine)(void *), void *arg) {
    // 1. Allouer la structure du nouveau thread
    thread_s *newThread = (thread_s*) malloc(sizeof(thread_s));
    if (newThread == NULL) return -1;

    // 2. Allouer la pile (stack) pour ce thread
    newThread->stack = malloc(SizeStack); 
    if (newThread->stack == NULL) {
        free(newThread);
        return -1;
    }

    // 3. Initialiser le contexte
    if (getcontext(&newThread->context) == -1) {
        free(newThread->stack);
        free(newThread);
        return -1;
    }

    // Configuration de la pile dans le contexte
    newThread->context.uc_stack.ss_sp = newThread->stack;
    newThread->context.uc_stack.ss_size = SizeStack;
    newThread->context.uc_stack.ss_flags = 0;
    
    // uc_link pointe vers le contexte à exécuter quand start_routine se termine.
    // Pour l'instant on met NULL, on gérera la fin proprement avec thread_exit plus tard.
    newThread->context.uc_link = NULL; 

    // 4. Configurer les métadonnées de ta structure
    newThread->state = READY;
    newThread->joiningThread = NULL;

    // 5. Lier le contexte à la fonction (Attention : makecontext attend formellement des int)
    // Sur certaines architectures 64 bits, passer un pointeur (arg) directement peut demander un cast spécial,
    // mais on reste simple ici.
    makecontext(&newThread->context, (void (*)(void))start_routine, 1, arg);

    *newthread = (thread_t)newThread;

    // 6. Ajouter le thread à la file des threads prêts
    TAILQ_INSERT_TAIL(&readyQueue, newThread, entries);

    return 0;
}

int thread_yield(){
    thread_s *oldThread = currentThread;
    //cas si il y a personne d'autre
    if (TAILQ_EMPTY(&readyQueue)){
        return;
    }
    thread_s *nextThread = TAILQ_FIRST(&readyQueue);
    TAILQ_REMOVE(&readyQueue, nextThread, entries);

    oldThread->state = READY;
    nextThread->state = RUNNING;

    TAILQ_INSERT_TAIL(&readyQueue, oldThread, entries);

    currentThread = nextThread;
    swapcontext(&oldThread->context, &nextThread->context);

    return 0;

}

int thread_join(thread_t thread, void **retval){

    thread_s *targetThread = (thread_s *)thread;

    if (targetThread->state != TERMINATED){
        targetThread->joiningThread = currentThread;
        currentThread->state = BLOCKED;
        thread_schedule_next();
    }
    if (retval != NULL) {
        *retval = targetThread->retval; 
    }

    free(targetThread->stack);
    free(targetThread);

    return 0;
}
