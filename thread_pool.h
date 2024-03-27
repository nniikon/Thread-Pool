#ifndef THREAD_POOL_H_
#define THREAD_POOL_H_

#include <inttypes.h>
#include <pthread.h>
#include <stdbool.h>


typedef struct {
    void (*func)(void* args);
    void* args;
} TH_Job;

typedef struct {
    uint32_t head;
    uint32_t tail;
    uint32_t size;
    uint32_t capacity;
    TH_Job*  jobs;
} TH_Queue;

typedef struct {
    pthread_t* threads;
    uint32_t n_threads;
    uint32_t n_threads_alive;

    pthread_mutex_t mutex_queue;
    pthread_cond_t   cond_queue;
    pthread_cond_t   cond_finished;
    pthread_mutex_t mutex_finished;
    TH_Queue              queue;

    bool is_running;
    uint32_t num_threads_running; 
} TH_Pool;

int  TH_PoolInit  (TH_Pool* pool, uint32_t n_treads);
int  TH_PoolAddJob(TH_Pool* pool, TH_Job job);
void TH_PoolDtor  (TH_Pool* pool);
int  TH_PoolWait  (TH_Pool* pool);

#endif
