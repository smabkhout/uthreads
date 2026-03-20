#include "thread.h"
#include <ucontext.h>
#include <queue.h> //ana maendish f pc donc maert

//we will need to download and include queue.h from the github they gave us maybe ? idk?

enum threadState {
    READY,
    RUNNING,
    BLOCKED,
    TERMINATED 
};

typedef struct thread_s {
    ucontext_t context;
    void *stack;
    enum threadState state;
    void *retval;
    struct thread_s *joiningThread; //thread_join
    TAILQ_ENTRY(thread_s) entries; //for the bsd queue linking
}thread_s;

TAILQ_HEAD(threadQueue, thread_s) readyQueue; //this our queue for ready threads (should we do just one for all threads or one per state dber rask a adam nta o prof)
//matalan hna threadQueue hya smya d struct o readyQueue hya smya d variable li hiya queue dyalna

thread_s* currentThread;

//the main function should be the first thread created and added to the queue + init queue + implemement threads.h
