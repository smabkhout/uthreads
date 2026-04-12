\#define _GNU_SOURCE
#include "thread.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <setjmp.h>
#include <ucontext.h>
#include <sys/queue.h>
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/mman.h>

#include <valgrind/valgrind.h>

//registre pour stackpointer et program counter
#define JB_RSP 6
#define JB_PC  7


#ifdef USE_PREEM
    #define SizeStack	8192*2
#else
    #define SizeStack	8192
#endif

#define PageSize 4096


enum threadState {
    READY,
    RUNNING,
    BLOCKED,
    TERMINATED 
};

struct thread_s {
    #ifdef USE_CONTEXT
    ucontext_t context;
#else
    #ifdef USE_PREEM
        sigjmp_buf env;
    #else
        jmp_buf env;
    #endif

#endif
    void *stack;
    int valgrind_stackid; //pour valgrind
    enum threadState state;
    void *(*func)(void *);
    void *arg;
    void *retval;
    struct thread_s *joiningThread; 
    TAILQ_ENTRY(thread_s) entries; // reference au thread suivant et thread precedent
    volatile int signals_blocked;
};

typedef struct thread_s thread_s;

// on definit le type des listes double chainées qu'on va utiliser
TAILQ_HEAD(threadQueue, thread_s);
TAILQ_HEAD(mutexQueue, thread_s);

struct threadQueue readyQueue;
struct threadQueue blockedQueue;

thread_s* currentThread;

int init_done = 0;

static thread_s mainThread;

#ifdef USE_CONTEXT
// pour que si un thread autre que main finit l'execution en dernier il peut free son stack son probleme
static char exitStack[8192];
// il faut lui donner ce contexte avec cette stack avant de free so stack "dynamique"
static ucontext_t exitContext;
#else
    #ifdef USE_PREEM
        static sigjmp_buf exitEnv;
    #else
        static jmp_buf exitEnv;
    #endif
#endif


static void exitFunc() {
    if (currentThread != &mainThread) {
        free(currentThread->stack);
        free(currentThread);
    }
    exit(0);
}

#ifdef USE_PREEM
static void lock_preemption() {
    if (currentThread != NULL) {
        currentThread->signals_blocked++;
    }
}

static void unlock_preemption() {
    if (currentThread != NULL) {
        currentThread->signals_blocked--;
    }
}


static void alarm_handler(int sig) {
    (void)sig;
    if (currentThread != NULL && currentThread->signals_blocked == 0) {

        sigset_t set;
        sigemptyset(&set);
        sigaddset(&set, SIGVTALRM);
        sigprocmask(SIG_UNBLOCK, &set, NULL);
        thread_yield();
    }
}

void start_preemption() {
    struct sigaction sa;
    struct itimerval it;

    sa.sa_handler = alarm_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART ;
    sigaction(SIGVTALRM, &sa, NULL);

    if (RUNNING_ON_VALGRIND) {
        it.it_interval.tv_sec = 0;
        it.it_interval.tv_usec = 500000; 
    } else {
        it.it_interval.tv_sec = 0;
        it.it_interval.tv_usec = 10000; 
    }
    it.it_value = it.it_interval;

    setitimer(ITIMER_VIRTUAL, &it, NULL);
}
#endif


// longjmp effectue un demangle donc il faut lui doner l'addresse brouille pas en clair
static inline long int mangle(long int p) {
    long int ret;
    asm ("xor %%fs:0x30, %0\n"
         "rol $0x11, %0\n"
         : "=r" (ret)
         : "0" (p));
    return ret;
}
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

    #ifdef USE_PREEM
        currentThread->signals_blocked = 0; 
    #endif

#ifdef USE_CONTEXT
    if (getcontext(&currentThread->context) == -1) {
        perror("Erreur lors du getcontext du main");
        exit(EXIT_FAILURE);
    }

    // contexte de sortie
    getcontext(&exitContext);
    exitContext.uc_stack.ss_sp = exitStack;
    exitContext.uc_stack.ss_size = SizeStack;
    exitContext.uc_link = NULL;
    makecontext(&exitContext, exitFunc, 0);
#else
    /* enregistre le contexte actuel */
    #ifdef USE_PREEM
        if (sigsetjmp(exitEnv, 0) != 0) {
            exitFunc();
        }
    #else
        if (setjmp(exitEnv) != 0) {
            exitFunc();
        }
    #endif
#endif
#ifdef USE_PREEM
    start_preemption();
#endif
}

thread_t thread_self() {
    if (!init_done) thread_init();
    return (thread_t) currentThread;
}

static void thread_wrapper(void) {
    thread_s *thread = currentThread; 
    void *retval = thread->func(thread->arg);
    thread_exit(retval);
}

int thread_create(thread_t *createdThread, void *(*func)(void *), void *arg) {
    if (currentThread == NULL) {
        thread_init();
    }
    #ifdef USE_PREEM
        lock_preemption();
    #endif
    thread_s *newThread = (thread_s*) calloc(1, sizeof(thread_s));
    if (newThread == NULL) {
        #ifdef USE_PREEM
            unlock_preemption();
        #endif
        return -1;
    }


    newThread->stack = calloc(1, SizeStack); 
    if (newThread->stack == NULL) {
        free(newThread);
        #ifdef USE_PREEM
            unlock_preemption();
        #endif
        return -1;
    }
    //detection debordement de pile avec mprotect
    // if (mprotect(newThread->stack, PageSize, PROT_NONE)!=0){
    //     perror("mprotect");
    // }



    newThread->valgrind_stackid = VALGRIND_STACK_REGISTER(newThread->stack,
                                               newThread->stack + SizeStack);

    newThread->state = READY;
    newThread->joiningThread = NULL;

    newThread->func = func;
    newThread->arg = arg;
    #ifdef USE_PREEM
        newThread->signals_blocked = 0; 
    #endif

#ifdef USE_CONTEXT
    if (getcontext(&newThread->context) == -1) {
        free(newThread->stack);
        free(newThread);
        return -1;
    }

    newThread->context.uc_stack.ss_sp = newThread->stack;
    newThread->context.uc_stack.ss_size = SizeStack;
    newThread->context.uc_stack.ss_flags = 0;

    newThread->context.uc_link = NULL;

    makecontext(&newThread->context, (void (*)(void))thread_wrapper, 1, newThread);
#else
    #ifdef USE_PREEM
        unlock_preemption();
        sigsetjmp(newThread->env, 0);
    #else
        setjmp(newThread->env);
    #endif

    #ifdef USE_PREEM
        lock_preemption();
    #endif

    unsigned long sp = (unsigned long)newThread->stack + SizeStack - 16;
    unsigned long pc = (unsigned long)thread_wrapper;

    newThread->env[0].__jmpbuf[JB_RSP] = mangle((long int)sp);
    newThread->env[0].__jmpbuf[JB_PC]  = mangle((long int)pc);
#endif

    *createdThread = (thread_t)newThread;

    TAILQ_INSERT_TAIL(&readyQueue, newThread, entries);
    #ifdef USE_PREEM
        unlock_preemption();
    #endif
    return 0;
}

int thread_yield(){
    #ifdef USE_PREEM
        lock_preemption();
    #endif
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
#ifdef USE_CONTEXT
    swapcontext(&oldThread->context, &nextThread->context);
#else
    #ifdef USE_PREEM
        if (sigsetjmp(oldThread->env, 0) == 0) {
            siglongjmp(nextThread->env, 1);
        }
    #else
        if (setjmp(oldThread->env) == 0) {
            longjmp(nextThread->env, 1);
        }
    #endif
#endif
    #ifdef USE_PREEM
        unlock_preemption();
    #endif
    return 0;
}

int thread_join(thread_t thread, void **retval){
    #ifdef USE_PREEM
        lock_preemption();
    #endif
    thread_s* oldCurrentThread = currentThread;
    thread_s* targetThread = (thread_s *)thread;

    if (targetThread == NULL) return -1;

    if (targetThread->state == READY){
        targetThread->joiningThread = oldCurrentThread;
        oldCurrentThread->state = BLOCKED;
        TAILQ_INSERT_TAIL(&blockedQueue, oldCurrentThread, entries);
        thread_yield(); // L'appelant bloque et passe la main
    } else if (targetThread->state == BLOCKED) {
        // il y aura surement un deadlock à un certain moment
        if (TAILQ_EMPTY(&readyQueue)){
            TAILQ_REMOVE(&blockedQueue, oldCurrentThread, entries);
            oldCurrentThread->state = RUNNING;
            targetThread->joiningThread = NULL;
            return EDEADLK; 
        }
        else {
            targetThread->joiningThread = oldCurrentThread;
            oldCurrentThread->state = BLOCKED;
            TAILQ_INSERT_TAIL(&blockedQueue, oldCurrentThread, entries);
            thread_yield();
        }
    }
    
    if (retval != NULL) {
        *retval = targetThread->retval; 
    }

    
    
    if (targetThread != &mainThread) {
        if (targetThread->stack) free(targetThread->stack);
        free(targetThread);
    }
    #ifdef USE_PREEM
        unlock_preemption();
    #endif

    return 0;
}

// on specifie à gcc que la fct ne retourne pas pour eviter les warnings
__attribute__((noreturn)) void thread_exit(void *retval) {
    #ifdef USE_PREEM
        lock_preemption();
    #endif
    currentThread->retval = retval;
    currentThread->state = TERMINATED;

    if (currentThread->joiningThread != NULL) {
        currentThread->joiningThread->state = READY;
        TAILQ_REMOVE(&blockedQueue, currentThread->joiningThread, entries);
        TAILQ_INSERT_TAIL(&readyQueue, currentThread->joiningThread, entries);
        currentThread->joiningThread = NULL;
    }

    if (TAILQ_EMPTY(&readyQueue)) {
        VALGRIND_STACK_DEREGISTER(currentThread->valgrind_stackid);
#ifdef USE_CONTEXT
        setcontext(&exitContext);
#else
    #ifdef USE_PREEM
        siglongjmp(exitEnv, 1);
    #else
        longjmp(exitEnv, 1);
    #endif
#endif
        assert(0);
    }

    thread_s *nextThread = TAILQ_FIRST(&readyQueue);
    TAILQ_REMOVE(&readyQueue, nextThread, entries);
    
    nextThread->state = RUNNING;
    currentThread = nextThread;

#ifdef USE_CONTEXT
    setcontext(&nextThread->context);
#else
    #ifdef USE_PREEM
        siglongjmp(nextThread->env, 1);
    #else
        longjmp(nextThread->env, 1);
    #endif
#endif
    assert(0);
    #ifdef USE_PREEM
        unlock_preemption();
    #endif
}


// 0 est unlocked
int thread_mutex_init(thread_mutex_t *m)
{
    if (!m) return -1;
    m->dummy = 0;
    m->waitingQueue = (struct mutexQueue*) malloc(sizeof(struct mutexQueue));
    TAILQ_INIT((struct mutexQueue*) m->waitingQueue);
    return 0;
}
int thread_mutex_destroy(thread_mutex_t *m)
{
    if (!m) return -1;
    m->dummy = 0;
    free(m->waitingQueue);
    return 0;
}

int thread_mutex_lock(thread_mutex_t *m)
{
    if (!m) return -1;
    #ifdef USE_PREEM
        lock_preemption();
    #endif
    while (m->dummy) {
        if (currentThread->state != BLOCKED) {
            currentThread->state = BLOCKED;
            TAILQ_INSERT_TAIL((struct mutexQueue*) m->waitingQueue, currentThread, entries);
        }
        //TAILQ_INSERT_TAIL(&blockedQueue, currentThread, entries);
        thread_yield();
    }
    // while (m->dummy) {
    //     thread_yield();
    // }
    m->dummy = 1;
    #ifdef USE_PREEM
        unlock_preemption();
    #endif
    return 0;
}

int thread_mutex_unlock(thread_mutex_t *m)
{
    if (!m) return -1;
    #ifdef USE_PREEM
        lock_preemption();
    #endif
    m->dummy = 0;
    if (!TAILQ_EMPTY((struct mutexQueue*) m->waitingQueue)) {
        thread_s* myTurnThread = TAILQ_FIRST((struct mutexQueue*) m->waitingQueue);
        TAILQ_REMOVE((struct mutexQueue*) m->waitingQueue, myTurnThread, entries);
        //TAILQ_REMOVE(&blockedQueue, myTurnThread, entries);
        myTurnThread->state = READY;
        TAILQ_INSERT_TAIL(&readyQueue, myTurnThread, entries);        
    }
    #ifdef USE_PREEM
        unlock_preemption();
    #endif
    return 0;
}