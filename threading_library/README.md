## EC440 Project 3: User Mode Threading Library

# Thread Control Block
The struct threadControlBlock stores the ID, register information, stack pointer, and current status of a thread. The array TCBTable 
of stores 128 (maxiumum number of threads supported) instances of threadControlBlock.

# pthread_create()
The pthread_create() function checks whether or not it is being called for the first time, in which case it initializes TCBTable and 
sets up the 50 ms SIGALRM and a corresponding signal handler as per Round Robin scheduling. It then iterates through the TCBTable
to find the first available entry to populate. Next, the function saves the current thread context in jmp_buf. A new stack of 32,767
bytes is then allocated and the RSP stack pointer is updated to point to the top of the stack. The status of the new thread is set to 
ready and the scheduler is called.

# schedule()
The scheduler is called by pthread_create(), by pthread_exit(), or when the SIGALRM is sent (every 50 ms). It first sets the status of 
the current thread from running to ready. It then iterates through the TCBTable repeatedly, starting at the thread ID after that of 
he current thread until a ready thread is found. The state of the previous thread is saved via setjmp. Next, the scheduler resumes the 
thread found in the TCBTable via longjmp.

# pthread_exit()
The pthread_exit() function sets the status of the current thread to exited, and then calls helper function exitCleanup(), which
iterates through the TCBTable and frees any threads with the exited status. Next, pthread_exit() iterates through TCBTable until it 
reaches a ready thread, in which case it will call the scheduler.

# pthread_self()
This function simply returns the thread ID of the global current running thread.