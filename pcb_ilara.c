#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

struct mm_struct {
    int pgb;
    int data;
    int code;
};

struct pcb_struct {
    int pid;
    int denbora;
    int lehentasuna;
    int erregistroak[16];
    int uneko_komando_helbidea;
    struct mm_struct mm;

};

struct Queue {
    int front, rear, size;
    unsigned capacity;
    struct pcb_struct* array;
    pthread_mutex_t mutex;



};

struct Queue* createQueue(unsigned capacity)
{
    struct Queue* queue = (struct Queue*)malloc(sizeof(struct Queue));
    queue->capacity = capacity;
    queue->front = queue->size = 0;


    queue->rear = capacity - 1;
    queue->array = (struct pcb_struct*) malloc(queue->capacity * sizeof(struct pcb_struct));
    return queue;
}

int isFull(struct Queue* queue)
{
    return (queue->size == queue->capacity);
}

int isEmpty(struct Queue* queue)
{
    return (queue->size == 0);
}

int kopurua(struct Queue* queue)
{
    return queue->size;
}

void enqueue(struct Queue* queue, struct pcb_struct item)
{

    if (!isFull(queue))
    {
      queue->rear = (queue->rear + 1)  % queue->capacity;
      queue->array[queue->rear] = item;
      queue->size = queue->size + 1;
    }

}


struct pcb_struct dequeue(struct Queue* queue)
{


    struct pcb_struct item = queue->array[queue->front];
    queue->front = (queue->front + 1)
                   % queue->capacity;
    queue->size = queue->size - 1;
    return item;

}

struct pcb_struct front(struct Queue* queue)
{

    return queue->array[queue->front];
}

struct pcb_struct rear(struct Queue* queue)
{

    return queue->array[queue->rear];
}
