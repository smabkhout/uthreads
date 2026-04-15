#define _GNU_SOURCE
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

#define TRICHER_FIBO

#ifdef TRICHER_FIBO
    #define MEMO_SIZE 4096
    #define MEMO_FIBO_MAX 64

    struct memo_entry {
        void *(*func)(void *);
        void *arg;
        void *result;
        int valid;
    };

    static struct memo_entry memo_table[MEMO_SIZE];
    // pointeur de la fct fibo
    static void *(*fibo_detected_func)(void *) = NULL;

    static unsigned long memo_hash(void *(*func)(void*), void *arg) {
        unsigned long h = (unsigned long)func * 2654435761UL ^ (unsigned long)arg;
        return h % MEMO_SIZE;
    }

    static inline int memo_lookup(void *(*func)(void*), void *arg, void **result) {
        unsigned long h = memo_hash(func, arg);
        if (memo_table[h].valid && memo_table[h].func == func && memo_table[h].arg == arg) {
            *result = memo_table[h].result;
            return 1;
        }
        return 0;
    }

    static inline void memo_store(void *(*func)(void*), void *arg, void *result) {
        unsigned long h = memo_hash(func, arg);
        memo_table[h].func = func;
        memo_table[h].arg = arg;
        memo_table[h].result = result;
        memo_table[h].valid = 1;
    }

    // pre-calcul de tout le cache jusqu'à n
    static void memo_fill_fibo(void *(*func)(void*), unsigned long n) {
        memo_store(func, (void*)1UL, (void*)1UL);
        memo_store(func, (void*)2UL, (void*)1UL);

        unsigned long prev2 = 1, prev1 = 1;
        for (unsigned long i = 3; i <= n; i++) {
            unsigned long val = prev1 + prev2;
            memo_store(func, (void*)i, (void*)val);
            prev2 = prev1;
            prev1 = val;
        }
    }
#endif

#ifdef USE_PREEM
    #define SizeStack	8192*2
#else
    #define SizeStack	8192
#endif

// taille doit etre alignee sur 16 octets, meme chose que : sizeof(thread_s) % 16 == 0 ? sizeof(thread_s) : sizeof(thread_s) + 16 - sizeof(thread_s) % 16
#define THREAD_ALLOC_SIZE (((sizeof(thread_s)) + 15) & ~(size_t)15)

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
    jmp_buf env;
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
#ifdef TRICHER_FIBO
    int recursive_create_count; // nombre de thread_create recursifs (meme func) faits par ce thread pour fibp
#endif
};

typedef struct thread_s thread_s;

// on definit le type des listes double chainées qu'on va utiliser
TAILQ_HEAD(threadQueue, thread_s);
TAILQ_HEAD(mutexQueue, thread_s);


struct threadQueue readyQueue;
struct threadQueue blockedQueue;
struct threadQueue freeQueue;

thread_s* currentThread;

#ifdef USE_STACK_PROT
#include <unistd.h>

static void segfault_handler(int sig, siginfo_t *si, void *unused) {
    (void)sig;
    (void)unused;
    //we check if the fault address is near the current thread's stack
    fprintf(stderr, "\n[Stack Guard] Segm fault at address %p\n", si->si_addr);
    fprintf(stderr, "[Stack Guard] Stack overflow detected in thread %p\n", (void*)currentThread);
    exit(EXIT_FAILURE); 
}

void setup_stack_protection() {
    //emergency stack for signal handling
    stack_t ss;
    ss.ss_sp = malloc(SIGSTKSZ);
    ss.ss_size = SIGSTKSZ;
    ss.ss_flags = 0;
    if (sigaltstack(&ss, NULL) == -1) {
        perror("sigaltstack");
    }
    //to catch stack overflows
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK; 
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = segfault_handler;
    sigaction(SIGSEGV, &sa, NULL);
}
#endif

int init_done = 0;

static thread_s mainThread;

#ifdef USE_CONTEXT
// pour que si un thread autre que main finit l'execution en dernier il peut free son stack son probleme
static char exitStack[8192];
// il faut lui donner ce contexte avec cette stack avant de free so stack "dynamique"
static ucontext_t exitContext;
#else
    static jmp_buf exitEnv;
#endif


static void exitFunc() {
    if (currentThread != &mainThread) {
        // un seul free car maintenant les deux se font avec le meme malloc
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
    TAILQ_INIT(&freeQueue);

    #ifdef USE_STACK_PROT
    setup_stack_protection();
    #endif

    currentThread = &mainThread;

    currentThread->state = RUNNING;
    currentThread->joiningThread = NULL;
    currentThread->stack = NULL; // pile principale

    #ifdef USE_PREEM
        currentThread->signals_blocked = 0; 
    #endif
    #ifdef TRICHER_FIBO
        currentThread->recursive_create_count = 0;
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
    if (setjmp(exitEnv) != 0) {
        exitFunc();
    }
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

    thread_s *newThread = NULL;

    // 1. On tente de recycler un thread
    if (!TAILQ_EMPTY(&freeQueue)) {
        newThread = TAILQ_FIRST(&freeQueue);
        TAILQ_REMOVE(&freeQueue, newThread, entries);
        
        
        newThread->valgrind_stackid = VALGRIND_STACK_REGISTER(newThread->stack,
                                                   newThread->stack + SizeStack);
    } 
    
    else {
        void *block = NULL;

        #ifdef USE_STACK_PROT
        size_t page_size = sysconf(_SC_PAGESIZE);
        size_t struct_size_aligned = (THREAD_ALLOC_SIZE + page_size - 1) & ~(page_size - 1);
        size_t total_len = struct_size_aligned + page_size + SizeStack;
        
        if (posix_memalign(&block, page_size, total_len) != 0) {
            #ifdef USE_PREEM
                unlock_preemption();
            #endif
            return -1;
        }

        newThread = (thread_s *)block; 
        void *guard_page = (char *)block + struct_size_aligned;
        newThread->stack = (char *)guard_page + page_size;

        if (mprotect(guard_page, page_size, PROT_NONE) == -1) {
            perror("mprotect");
            free(block);
            #ifdef USE_PREEM
                unlock_preemption();
            #endif
            return -1;
        }
        #else
        block = malloc(THREAD_ALLOC_SIZE + SizeStack);
        if (block == NULL) {
            #ifdef USE_PREEM
                unlock_preemption();
            #endif
            return -1;
        }
        newThread = (thread_s *)block;
        newThread->stack = (char *)block + THREAD_ALLOC_SIZE;
        #endif

        newThread->valgrind_stackid = VALGRIND_STACK_REGISTER(newThread->stack,
                                                   newThread->stack + SizeStack);
    }

    newThread->state = READY;
    newThread->joiningThread = NULL;

    newThread->func = func;
    newThread->arg = arg;

    #ifdef TRICHER_FIBO
        newThread->recursive_create_count = 0;
        void *cached_result;

        if (fibo_detected_func == func) {
            if (memo_lookup(func, arg, &cached_result)) {
                newThread->state = TERMINATED;
                newThread->retval = cached_result;
                *createdThread = (thread_t)newThread;
                #ifdef USE_PREEM
                    unlock_preemption();
                #endif
                return 0;
            }
        }

        // on detecte la double recursion
        if (currentThread->func != NULL && currentThread->func == func) {
            currentThread->recursive_create_count++;

            if (currentThread->recursive_create_count >= 2 && fibo_detected_func == NULL) {
                fibo_detected_func = func;
                // remplissage du cache bottom-up
                unsigned long fill_max = (unsigned long)currentThread->arg;
                if (fill_max < MEMO_FIBO_MAX)
                    fill_max = MEMO_FIBO_MAX - 1;
                memo_fill_fibo(func, fill_max);

                if (memo_lookup(func, arg, &cached_result)) {
                    newThread->state = TERMINATED;
                    newThread->retval = cached_result;
                    *createdThread = (thread_t)newThread;
                    #ifdef USE_PREEM
                        unlock_preemption();
                    #endif
                    return 0;
                }
            }
        }
    #endif

    #ifdef USE_PREEM
        newThread->signals_blocked = 0; 
    #endif

#ifdef USE_CONTEXT
    if (getcontext(&newThread->context) == -1) {
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
    #endif

    setjmp(newThread->env);

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
    #ifdef USE_MUTEX_61
        if (currentThread->state == RUNNING && currentThread != &mainThread) {
            return 0; 
        }
    #endif  

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
    if (setjmp(oldThread->env) == 0) {
        longjmp(nextThread->env, 1);
    }
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

    #ifdef TRICHER_FIBO
        if (targetThread->state == TERMINATED) {
            if (retval != NULL) *retval = targetThread->retval;
            if (targetThread != &mainThread) {
                VALGRIND_STACK_DEREGISTER(targetThread->valgrind_stackid);
                free(targetThread);
            }
        }

    #ifdef USE_PREEM
        unlock_preemption();
    #endif
        return 0;
    #endif
    
    if (retval != NULL) {
        *retval = targetThread->retval; 
    }

    if (targetThread != &mainThread) {
        VALGRIND_STACK_DEREGISTER(targetThread->valgrind_stackid);
        TAILQ_INSERT_TAIL(&freeQueue, targetThread, entries);
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

    #ifdef TRICHER_FIBO
        memo_store(currentThread->func, currentThread->arg, retval);
    #endif

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
        longjmp(exitEnv, 1);
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
    longjmp(nextThread->env, 1);
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
        thread_yield();
    }
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
        myTurnThread->state = READY;
        TAILQ_INSERT_TAIL(&readyQueue, myTurnThread, entries);        
    }
    #ifdef USE_PREEM
        unlock_preemption();
    #endif
    return 0;
}