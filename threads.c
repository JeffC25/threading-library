#include "asm_wrappers.h"
#include <stdio.h>
#include <pthread.h>
#include <setjmp.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <semaphore.h>
#include "queue.h"

#define MAXTHREADS  128                                                                      // maximum threads supported
#define STACKSIZE   32767                                                                    // stack byte size to allocate
#define QUANTUM     50 * 1000                                                                // 50,000 Us = 50 ms interval

// Registers
#define JB_RBX  0
#define JB_RBP  1
#define JB_R12  2
#define JB_R13  3
#define JB_R14  4
#define JB_R15  5
#define JB_RSP  6                                                                           // stack pointer
#define JB_PC   7                                                                           // program counter

enum threadStatus {RUNNING, READY, EXITED, AVAILABLE, BLOCKED};                             // thread states

struct threadControlBlock {                                                                 // thread control block
    pthread_t id;                                                                           // thread ID
    jmp_buf registers;                                                                      // from setjmp.h: "__extension__ typedef long long int __jmp_buf[8];"
    void* stack;                                                                            // stack pointer
    enum threadStatus status;                                                               // thread state

    int woken;                                                                              // flag for if thread just woken
    int initialized;                                                                        // flag for if thread just initialized
    void* exitStatus;                                                                       // pointer to exit status
};

struct semaphoreControlBlock {
    int counter;                                                                            // semaphore counter
    struct Queue* roundRobinQueue;                                                          // queue to store threads
    int initialized;                 
};

void pthread_exit_wrapper() {
    unsigned long int res;
    asm("movq %%rax, %0\n":"=r"(res));
    pthread_exit((void *) res);
}

struct threadControlBlock TCBTable[MAXTHREADS];                                             // table/array to store TCBs 
pthread_t gCurrent = 0;                                                                     // global ID of current running thread
struct sigaction signalHandler;                                                             // signal handler for Round Robin                      

void initializeTCB() {
    int i;              
    for (i = 0; i < MAXTHREADS; i++) {                                                  // initialize values in TCB table
        TCBTable[i].id = i;             
        TCBTable[i].status = AVAILABLE;             
        TCBTable[i].initialized = 0;
        TCBTable[i].woken = 0;
    }               

    ualarm(QUANTUM, QUANTUM);                                                           // SIGALARM every 50 ms

    sigemptyset(&signalHandler.sa_mask);                                                // initialize signal set
    signalHandler.sa_handler = &schedule;                                               // pointer to signal handling function (schedule)
    signalHandler.sa_flags = SA_NODEFER;                                                // do not prevent signal from being received within own signal handler                
    sigaction(SIGALRM, &signalHandler, NULL);                                           // handle alarm signal and schedule
}

extern int pthread_create(
    pthread_t *thread, 
    const pthread_attr_t *attr, 
    void *(*start_routine) (void *), 
    void *arg) {

    static int firstThread = 1;                                                             // flag for initializing scheduler
    attr = NULL;                                                                            // "In our implementation, the second argument (attr) shall always be NULL"
    int mainThread = 0;                                                                     // flag for main program

    if (firstThread) {
        initializeTCB();

        firstThread = 0;                                                                    // unset flag 
        TCBTable[0].status = READY;                                                         // set status of first thread as ready
        TCBTable[0].initialized = 1;                                                        // set initialized flag
        mainThread = setjmp(TCBTable[0].registers);                                         // save context
    }

    if (!mainThread) {
        pthread_t tidCurrent = 1;
        while(TCBTable[tidCurrent].status != AVAILABLE && tidCurrent < MAXTHREADS) {
            tidCurrent++;
        }
        if (tidCurrent == MAXTHREADS) {
            fprintf(stderr, "ERROR: Reached maximum threads supported\n");
            exit(EXIT_FAILURE);
        }
        
        *thread = tidCurrent;                                                                   

        setjmp(TCBTable[tidCurrent].registers);                                                 // save current thread context

        TCBTable[tidCurrent].stack = malloc(STACKSIZE);                                         // allocate 32,767 bytes for new thread stack
        void* stackBottom = STACKSIZE + TCBTable[tidCurrent].stack;                             // set pointer to top of stack

        void* stackPointer = stackBottom - 8;                                                   // update stack pointer
        void (*newStack)(void*) = &pthread_exit_wrapper;                                              
        stackPointer = memcpy(stackPointer, &newStack, sizeof(newStack)); 

        TCBTable[tidCurrent].registers[0].__jmpbuf[JB_PC] = ptr_mangle((unsigned long int)start_thunk);      // decrypt 
        TCBTable[tidCurrent].registers[0].__jmpbuf[JB_R13] = (unsigned long int)arg;                         // store arg in register R13
        TCBTable[tidCurrent].registers[0].__jmpbuf[JB_R12] = (unsigned long int)start_routine;               // store address of start_routine function in register R12
        TCBTable[tidCurrent].registers[0].__jmpbuf[JB_RSP] = ptr_mangle((unsigned long int)stackPointer);    

        TCBTable[tidCurrent].initialized = 1;
        TCBTable[tidCurrent].id = tidCurrent;                                                   // set ID of new thread
        TCBTable[tidCurrent].status = READY;                                                    // set status to ready

        schedule();                                                                             // call scheduler
    }
    else {   
        mainThread = 0;
    }
    
    return 0;
}

void schedule() {        
    if(TCBTable[gCurrent].status == RUNNING) {
        TCBTable[gCurrent].status = READY;
    }

    pthread_t tidCurrent = gCurrent + 1;
    while(TCBTable[tidCurrent].status != READY) {                                           // cycle through TCB table until finding ready thread
        tidCurrent = (tidCurrent + 1) % MAXTHREADS;
    }

    int save = 0;

    if (TCBTable[gCurrent].initialized == 0  && TCBTable[gCurrent].status != EXITED) {
        save = setjmp(TCBTable[gCurrent].registers);
    }
    
    if (TCBTable[tidCurrent].initialized) {
        TCBTable[tidCurrent].initialized = 0;
    }

    if (!save) {
        gCurrent = tidCurrent;
        TCBTable[gCurrent].status = RUNNING;
        longjmp(TCBTable[gCurrent].registers, 1);
    }
}

extern void exitCleanup() {                                                                        // clean up remaining threads
    int i;
    for (i = 0; i < MAXTHREADS; i++) {                                                      // iterate through TCB table
        if (TCBTable[i].status == EXITED) {                                                 // check if thread has exited
            free(TCBTable[i].stack);                                                        // free memory from thread
            TCBTable[i].status = AVAILABLE;                                                 // set TCB table entry to available
        }
    }
}

extern void pthread_exit(void *value_ptr) {
    TCBTable[gCurrent].status = EXITED;
    TCBTable[gCurrent].exitStatus = value_ptr;

    pthread_t tidWaiting = TCBTable[gCurrent].id;
    if (tidWaiting != gCurrent) {
        TCBTable[tidWaiting].status = READY;
    }

    int threadsRemaining = 0;
    int i;
    for (i = 0; i < MAXTHREADS; i++) {
        switch(TCBTable[i].status) {
            case READY   :
            case RUNNING :
            case BLOCKED :
                threadsRemaining = 1;
                break;
            case EXITED:
            case AVAILABLE:
                break;
        }
    }

    if (threadsRemaining) {                                                                 // call schedule if threads still in queue
        schedule();
    }

    exitCleanup();
    exit(0);
}

extern pthread_t pthread_self() {
    return gCurrent;
}

extern void lock() {
    sigset_t set;
    sigemptyset (&set);
    sigaddset(&set, SIGALRM);
    sigprocmask(SIG_BLOCK, &set, NULL);
}

extern void unlock() {
    sigset_t set;
    sigemptyset (&set);
    sigaddset(&set, SIGALRM);
    sigprocmask(SIG_UNBLOCK, &set, NULL);
}

extern int pthread_join(pthread_t thread, void** value_ptr) {
    switch(TCBTable[thread].status)
    {
        case READY   :
        case RUNNING :
        case BLOCKED :        
            TCBTable[gCurrent].status = BLOCKED;
            TCBTable[thread].id = gCurrent;
	        schedule();
        case EXITED:
            if (value_ptr != NULL) {
                *value_ptr = TCBTable[thread].exitStatus;
            }
            free(TCBTable[thread].stack);
            TCBTable[thread].status = AVAILABLE;
            break;
        case AVAILABLE:
            return 1; 
            break;
    }
    return 0;
}

extern int sem_init(sem_t *sem, int pshared, unsigned value) {                                // initialize semaphore
    struct semaphoreControlBlock* SCB = (struct semaphoreControlBlock*) malloc(sizeof(struct semaphoreControlBlock));

    SCB->counter = value;                                                                         // initial value of semaphore
    SCB->roundRobinQueue = createQueue();                                                        // create new queue
    SCB->initialized = 1;

    sem->__align = (unsigned long int) SCB;

    return 0;
}

extern int sem_wait(sem_t *sem) {                                                             // locks semaphore
    struct semaphoreControlBlock* SCB = (struct semaphoreControlBlock*) (sem->__align);

    if (SCB->counter <= 0) {
        TCBTable[gCurrent].status = BLOCKED;
        enQueue(SCB->roundRobinQueue, gCurrent);
        schedule();
    }
    else {
        (SCB->counter)--;
        return 0;
    }
    if (TCBTable[gCurrent].woken) {
        TCBTable[gCurrent].woken = 0;

        return 0;
    }

    return -1;                                                                                  // error
}

extern int sem_post(sem_t *sem) {                                                             // unlocks/increments semaphore
    struct semaphoreControlBlock* SCB = (struct semaphoreControlBlock*)(sem->__align);

    if ((SCB->counter) >= 0) {
        pthread_t nextThread = deQueue(SCB->roundRobinQueue);                                   // pop next thread from queue

        if (nextThread != -1) {                                                                 // queue not empty
            TCBTable[nextThread].woken = 1;                                                     // wake up thread
            TCBTable[nextThread].status = READY;                                                // thread ready
        }
        else {
            (SCB->counter)++;                                                                   // queue empty
        }
        unlock();
        return 0;
    }
    return -1;                                                                                  // error
}   

extern int sem_destroy(sem_t *sem) {                                                            // destroy semaphore
     struct semaphoreControlBlock* SCB = (struct semaphoreControlBlock*) (sem->__align);

     if (SCB->initialized) {
        free((void*)(sem->__align));
     }
     else {
        return -1;                                                                              // error
     }
     return 0;
}

