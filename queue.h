#ifndef __QUEUE__
#define __QUEUE__

struct Node { 
    pthread_t key;                                                                      // ID of thread in queue
    struct Node* next;                                                                  // next thread in queue
}; 
  
struct Queue { 
    struct Node *start;                                                                 // first entry in queue
    struct Node *end;                                                                   // last entry in queue
}; 
  
struct Node* newNode(pthread_t value) {                                                     // function to create new queue entry 
    struct Node* newStack = (struct Node*)malloc(sizeof(struct Node));                      
    newStack->key = value; 
    newStack->next = NULL; 
    return newStack; 
} 
  
struct Queue* createQueue() {                                                           // function to create an empty queue 
    struct Queue* q = (struct Queue*)malloc(sizeof(struct Queue)); 
    q->start = q->end = NULL; 
    return q; 
} 
  
void enQueue(struct Queue* q, pthread_t value) {                                        // function to push key k to queue
    
    struct Node* newStack = newNode(value);                                                 // create a new entry 

    if (q->end == NULL) {                                                               // if queue is empty, set start and end to new node 
        q->start = q->end = newStack; 
        return; 
    } 
   
    q->end->next = newStack;                                                                // update end pointer
    q->end = newStack; 
} 
   
pthread_t deQueue(struct Queue* q) {                                                      // function to pop key from queue
    if (q->start == NULL)                                                               // if queue is empty, return NULL
        return (pthread_t) - 1; 
   
    struct Node* newStack = q->start;                                                       // update start pointer
  
    q->start = q->start->next; 

    if (q->start == NULL)                                                               // if start becomes NULL, also set end to NULL 
        q->end = NULL; 
  
    return newStack->key;
} 

#endif