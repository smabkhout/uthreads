#include "thread.h"
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <sys/queue.h>
#include <assert.h>

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
    void *(*func)(void *);
    void *arg;
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

int init_done = 0;

static thread_s mainThread;

//the main function should be the first thread created
void thread_init() {
    if (init_done) return;
    init_done = 1;
    TAILQ_INIT(&readyQueue);
    TAILQ_INIT(&blockedQueue);

    currentThread = &mainThread;

    currentThread->state = RUNNING;
    currentThread->joiningThread = NULL;
    currentThread->stack = NULL; // pile principale

    // sauvegarder le contexte actuel d'exécution dans ce thread
    if (getcontext(&currentThread->context) == -1) {
        perror("Erreur lors du getcontext du main");
        exit(EXIT_FAILURE);
    }
}

thread_t thread_self() {
    if (!init_done) thread_init();
    return (thread_t) currentThread;
}

static void thread_wrapper(thread_s *thread) {
    void *retval = thread->func(thread->arg);
    thread_exit(retval);
}

int thread_create(thread_t *createdThread, void *(*func)(void *), void *arg) {
    if (currentThread == NULL) {
        thread_init();
    }
    thread_s *newThread = (thread_s*) malloc(sizeof(thread_s));
    if (newThread == NULL) return -1;

    newThread->stack = malloc(SizeStack); 
    if (newThread->stack == NULL) {
        free(newThread);
        return -1;
    }

    if (getcontext(&newThread->context) == -1) {
        free(newThread->stack);
        free(newThread);
        return -1;
    }

    newThread->context.uc_stack.ss_sp = newThread->stack;
    newThread->context.uc_stack.ss_size = SizeStack;
    newThread->context.uc_stack.ss_flags = 0;
    
    // on le laisse a null mais ca doit aller a joining thread, on pourra le changer apres
    newThread->context.uc_link = NULL; 

    newThread->state = READY;
    newThread->joiningThread = NULL;

    newThread->func = func;
    newThread->arg = arg;

    // We MUST use the wrapper to intercept the return value!
    makecontext(&newThread->context, (void (*)(void))thread_wrapper, 1, newThread);
    *createdThread = (thread_t)newThread;

    TAILQ_INSERT_TAIL(&readyQueue, newThread, entries);
    return 0;
}

int thread_yield(){
    thread_s *oldThread = currentThread;
    //cas si il y a personne d'autre
    if (init_done == 0 || TAILQ_EMPTY(&readyQueue)) {
        return 0;
    }
    thread_s *nextThread = TAILQ_FIRST(&readyQueue);
    TAILQ_REMOVE(&readyQueue, nextThread, entries);

    if (oldThread->state == RUNNING) {
        oldThread->state = READY;
        TAILQ_INSERT_TAIL(&readyQueue, oldThread, entries);
    }
    nextThread->state = RUNNING;

    currentThread = nextThread;
    swapcontext(&oldThread->context, &nextThread->context);
    return 0;
}

int thread_join(thread_t thread, void **retval){
    thread_s* oldCurrentThread = currentThread;
    thread_s* targetThread = (thread_s *)thread;

    if (targetThread == NULL) return -1;
    
    if (targetThread->state != TERMINATED){
        targetThread->joiningThread = oldCurrentThread;
        oldCurrentThread->state = BLOCKED;
        TAILQ_INSERT_TAIL(&blockedQueue, oldCurrentThread, entries);
        
        thread_yield(); // L'appelant bloque et passe la main
    }
    if (retval != NULL) {
        *retval = targetThread->retval; 
    }

    if (targetThread->stack) free(targetThread->stack);
    
    if (targetThread != &mainThread) free(targetThread);

    return 0;
}

// on specifie à gcc que la fct ne retourne pas pour eviter les warnings
__attribute__((noreturn)) void thread_exit(void *retval) {
    currentThread->retval = retval;
    currentThread->state = TERMINATED;

    if (currentThread->joiningThread != NULL) {
        currentThread->joiningThread->state = READY;
        TAILQ_REMOVE(&blockedQueue, currentThread->joiningThread, entries);
        TAILQ_INSERT_TAIL(&readyQueue, currentThread->joiningThread, entries);
        currentThread->joiningThread = NULL;
    }

    if (currentThread == &mainThread) {
        // on free tous les thred de la ready queue
        thread_s *tmp;
        while ((tmp = TAILQ_FIRST(&readyQueue)) != NULL) {
            TAILQ_REMOVE(&readyQueue, tmp, entries);
            if (tmp->stack) free(tmp->stack);
            free(tmp);
        }
        // on free tous les thred de la blocked queue
        while ((tmp = TAILQ_FIRST(&blockedQueue)) != NULL) {
            TAILQ_REMOVE(&blockedQueue, tmp, entries);
            if (tmp->stack) free(tmp->stack);
            free(tmp);
        }
    }

    if (TAILQ_EMPTY(&readyQueue)) exit(0); 

    thread_s *nextThread = TAILQ_FIRST(&readyQueue);
    TAILQ_REMOVE(&readyQueue, nextThread, entries);
    
    nextThread->state = RUNNING;
    currentThread = nextThread;

    setcontext(&nextThread->context);
    assert(0);
}



int thread_mutex_init(thread_mutex_t *m)
{
    if (!m) return -1;
    m->dummy = 0;
    return 0;
}
int thread_mutex_destroy(thread_mutex_t *m)
{
    if (!m) return -1;
    m->dummy = 0;
    return 0;
}

int thread_mutex_lock(thread_mutex_t *m)
{
    if (!m) return -1;
    while (m->dummy) {
        thread_yield();
    }
    m->dummy = 1;
    return 0;
}

int thread_mutex_unlock(thread_mutex_t *m)
{
    if (!m) return -1;
    m->dummy = 0;
    return 0;
}