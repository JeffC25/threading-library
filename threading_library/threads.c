#include "ec440threads.h"
#include <stdio.h>
#include <pthread.h>
#include <setjmp.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

// implement in order:
// 1. pthread_create()
// 2. schedule()
// 3. pthread_exit()
// 4. pthread_self()

// Registers
#define JB_RBX 0
#define JB_RBP 1
#define JB_R12 2
#define JB_R13 3
#define JB_R14 4
#define JB_R15 5
#define JB_RSP 6                                                                            // stack pointer
#define JB_PC 7                                                                             // program counter

#define MAXTHREADS 128                                                                      // maximum threads supported
#define STACKSIZE 32767                                                                     // stack byte size to allocate
#define QUANTUM 50 * 1000                                                                   // 50,000 Us = 50 ms interval

pthread_t gCurrent = 0;                                                                     // ID of current running thread
int firstThread = 1;                                                                        // flag for initializing scheduler

struct sigaction signalHandler;                                                             // signal handler for Round Robin

enum threadStatus {running, ready, exited, available};                                      // thread states

struct threadControlBlock {                                                                 // thread control block
    int id;                                                                                 // thread ID
    jmp_buf registers;                                                                      // from setjmp.h: "__extension__ typedef long long int __jmp_buf[8];"
    void* stack;                                                                            // stack pointer
    enum threadStatus status;                                                               // thread state
};

struct threadControlBlock TCBTable[MAXTHREADS];                                             // table/array to store TCBs 

int pthread_create(
    pthread_t *thread, 
    const pthread_attr_t *attr, 
    void *(*start_routine)(void *), 
    void *arg) {

    attr = NULL;                                                                            // "In our implementation, the second argument (attr) shall always be NULL"

    if (firstThread) {                                                                      // set up data sctructures and scheduler             
        int i;              
        for (i = 0; i < MAXTHREADS; i++) {                                                  // initialize values in TCB table
            TCBTable[i].id = i;             
            TCBTable[i].status = available;             
        }               

        ualarm(QUANTUM, QUANTUM);                                                           // SIGALARM every 50 ms

        sigemptyset(&signalHandler.sa_mask);                                                // initialize signal set
        signalHandler.sa_handler = &schedule;                                               // pointer to signal handling function (schedule)
        signalHandler.sa_flags = SA_NODEFER;                                                // do not prevent signal from being received within own signal handler                
        sigaction(SIGALRM, &signalHandler, NULL);                                           // handle alarm signal and schedule
             
        firstThread = 0;                                                                    // unset flag 
        TCBTable[0].status = ready;                                                         // set status of first thread as ready

    }                           

    pthread_t tidCurrent = 1;
    while (TCBTable[tidCurrent].status != available) {                                      // iterate through TCB table to find next available thread
        if (tidCurrent > MAXTHREADS) {
            perror("ERROR: Reached maximum threads supported\n");                           // error if maximum number of threads exceeded
        }
        else {
            tidCurrent++;
        }
    }
    *thread = tidCurrent;                                                                   

    setjmp(TCBTable[tidCurrent].registers);                                                 // save current thread context

    TCBTable[tidCurrent].registers[0].__jmpbuf[JB_PC] = ptr_mangle((long)start_thunk);      // decrypt 
    TCBTable[tidCurrent].registers[0].__jmpbuf[JB_R13] = (long)arg;                         // store arg in register R13
    TCBTable[tidCurrent].registers[0].__jmpbuf[JB_R12] = (long)start_routine;               // store address of start_routine function in register R12

    TCBTable[tidCurrent].stack = malloc(STACKSIZE);                                         // allocate 32,767 bytes for new thread stack
    void* stackBottom = STACKSIZE + TCBTable[tidCurrent].stack;                             // set pointer to top of stack

    void* stackPointer = stackBottom - sizeof(&pthread_exit);                               // update stack pointer
    void (*temp)(void*) = (void*)&pthread_exit;                                              
    stackPointer = memcpy(stackPointer, &temp, sizeof(temp));                               
    TCBTable[tidCurrent].registers[0].__jmpbuf[JB_RSP] = ptr_mangle((long)stackPointer);    

    TCBTable[tidCurrent].id = tidCurrent;                                                   // set ID of new thread
    TCBTable[tidCurrent].status = ready;                                                    // set status to ready

    schedule();                                                                             // call scheduler

    return 0;
}

void schedule() {
    if (TCBTable[gCurrent].status == running) {                                             // suspend current thread
        TCBTable[gCurrent].status = ready;                      
    }

    pthread_t tidCurrent = gCurrent + 1;
    while(TCBTable[tidCurrent].status != ready) {                                           // cycle through TCB table until finding ready thread
        tidCurrent = (tidCurrent + 1) % MAXTHREADS;
    }

    int save = 0;
    if(TCBTable[gCurrent].status != exited) {
        save = setjmp(TCBTable[gCurrent].registers);                                        // 
    }

    if (!save) {
        gCurrent = tidCurrent;
        TCBTable[gCurrent].status = running;
        longjmp(TCBTable[gCurrent].registers, 1);                                           
    }

}

void exitCleanup() {                                                                        // clean up remaining threads
    int i;
    for (i = 0; i < MAXTHREADS; i++) {                                                      // iterate through TCB table
        if (TCBTable[i].status == exited) {                                                 // check if thread has exited
            free(TCBTable[i].stack);                                                        // free memory from thread
            TCBTable[i].status = available;                                                 // set TCB table entry to available
        }
    }
}

void pthread_exit(void *retval) {                                                           // 
    TCBTable[gCurrent].status = exited; 

    exitCleanup();                                                                          

    int i;  
    for (i = 0; i < MAXTHREADS; i++) {                                                      // call scheduler if any threads are waiting
        if (TCBTable[i].status == ready) {  
            schedule(); 
            break;  
        }   
    }   

    exit(0);                                                                                // exit if no threads left
}   

pthread_t pthread_self() {                                                                  // return ID of current running thread
    return gCurrent;
}
