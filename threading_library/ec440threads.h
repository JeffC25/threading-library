#ifndef __EC440THREADS__
#define __EC440THREADS__

unsigned long int ptr_demangle(unsigned long int p)
{
    unsigned long int ret;

    asm("movq %1, %%rax;\n"
        "rorq $0x11, %%rax;"
        "xorq %%fs:0x30, %%rax;"
        "movq %%rax, %0;"
    : "=r"(ret)
    : "r"(p)
    : "%rax"
    );
    return ret;
}

unsigned long int ptr_mangle(unsigned long int p)
{
    unsigned long int ret;

    asm("movq %1, %%rax;\n"
        "xorq %%fs:0x30, %%rax;"
        "rolq $0x11, %%rax;"
        "movq %%rax, %0;"
    : "=r"(ret)
    : "r"(p)
    : "%rax"
    );
    return ret;
}

void *start_thunk() {
  asm("popq %%rbp;\n"                                                                   // clean up the function prolog
      "movq %%r13, %%rdi;\n"                                                            // put arg in $rdi
      "pushq %%r12;\n"                                                                  // push &start_routine
      "retq;\n"                                                                         // return to &start_routine
      :
      :
      : "%rdi"
  );
  __builtin_unreachable();
}

static void schedule();          

void pthread_exit_wrapper() {
    unsigned long int res;
    asm("movq %%rax, %0\n":"=r"(res));
    pthread_exit((void *) res);
}

////////////////////////////////////////////////////////// Queue implementation //////////////////////////////////////////////////////////

struct Node { 
    pthread_t key;                                                                      // ID of thread in queue
    struct Node* next;                                                                  // next thread in queue
}; 
  
struct Queue { 
    struct Node *start;                                                                 // first entry in queue
    struct Node *end;                                                                   // last entry in queue
}; 
  
struct Node* newNode(pthread_t value) {                                                     // function to create new queue entry 
    struct Node* temp = (struct Node*)malloc(sizeof(struct Node));                      
    temp->key = value; 
    temp->next = NULL; 
    return temp; 
} 
  
struct Queue* createQueue() {                                                           // function to create an empty queue 
    struct Queue* q = (struct Queue*)malloc(sizeof(struct Queue)); 
    q->start = q->end = NULL; 
    return q; 
} 
  
void enQueue(struct Queue* q, pthread_t value) {                                        // function to add a key k to queue
    
    struct Node* temp = newNode(value);                                                 // create a new entry 

    if (q->end == NULL) {                                                               // if queue is empty, set start and end to new node 
        q->start = q->end = temp; 
        return; 
    } 
   
    q->end->next = temp;                                                                // update end pointer
    q->end = temp; 
} 
   
pthread_t deQueue(struct Queue* q)                                                      // function to remove key from queue
{ 
    if (q->start == NULL)                                                               // if queue is empty, return NULL
        return (pthread_t) - 1; 
   
    struct Node* temp = q->start;                                                       // update start pointer
  
    q->start = q->start->next; 

    if (q->start == NULL)                                                               // if start becomes NULL, also set end to NULL 
        q->end = NULL; 
  
    return temp->key;
} 

////////////////////////////////////////////////////////// Queue implementation //////////////////////////////////////////////////////////

// void pthread_create(            //
//     pthread_t *thread,
//     const pthread_attr_t *attr,
//     void *(*start_routine)(void *),
//     void *arg
// );

// void pthread_exit(void *value_ptr);

//pthread_t pthread_self(void);


#endif