#include "thread_pool.h"

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

const float kGrowthFactor = 2.0f;
const uint32_t kQueueStandartSize = 256;

static int TH_QueuePop(TH_Queue* queue, TH_Job* job) {
    if (queue == NULL || job == NULL || queue->size == 0) {
        return -1;
    }

    *job = queue->jobs[queue->head];

    queue->head = (queue->head + 1) % queue->capacity;

    queue->size--;

    return 0;
}

static int TH_QueuePush(TH_Queue* queue, TH_Job job) {
    if (queue == NULL) {
        return -1;
    }

    if (queue->size == queue->capacity) {
        uint32_t newCapacity = queue->capacity * kGrowthFactor;
        TH_Job* newJobs = (TH_Job*)realloc(queue->jobs, newCapacity * sizeof(TH_Job));
        if (newJobs == NULL) {
            return -1;
        }
        queue->jobs = newJobs;

        if (queue->head > queue->tail) {
            memmove(&queue->jobs[queue->capacity], &queue->jobs[0], queue->tail * sizeof(TH_Job));
            queue->tail += queue->capacity;
        }

        queue->capacity = newCapacity;
    }

    queue->jobs[queue->tail] = job;

    queue->tail = (queue->tail + 1) % queue->capacity;

    queue->size++;

    return 0;
}

static void TH_ExecJob(TH_Job job) {
    job.func(job.args);
}

void* TH_ThreadInit(TH_Pool* pool) {
    int error = 0;

    error = pthread_mutex_lock(&pool->mutex_queue);
    pool->n_threads_alive++;
    error = pthread_mutex_unlock(&pool->mutex_queue);

    while (pool->is_running) {

        error = pthread_mutex_lock(&pool->mutex_queue);

        while (pool->queue.size == 0 && pool->is_running) {
            pthread_cond_wait(&pool->cond_queue, &pool->mutex_queue);
        }

        if (!pool->is_running)
        {
            error = pthread_mutex_unlock(&pool->mutex_queue);
            break;
        }

        pool->num_threads_running++;

        // Task available
        TH_Job job = {};
        error = TH_QueuePop(&pool->queue, &job);
        if (error)
            return NULL;

        error = pthread_mutex_unlock(&pool->mutex_queue);

        TH_ExecJob(job);

        error = pthread_mutex_lock  (&pool->mutex_queue);

        pool->num_threads_running--;

        if (pool->num_threads_running == 0)
            pthread_cond_broadcast(&pool->cond_finished);

        error = pthread_mutex_unlock(&pool->mutex_queue);
    }

    error = pthread_mutex_lock(&pool->mutex_queue);
    pool->n_threads_alive--;
    error = pthread_mutex_unlock(&pool->mutex_queue);

    return NULL;
}

int TH_PoolAddJob(TH_Pool* pool, TH_Job job) {
    assert(pool);

    int error = pthread_mutex_lock(&pool->mutex_queue);

    error = TH_QueuePush(&pool->queue, job);
    if (error)
        return error;

    pthread_mutex_unlock(&pool->mutex_queue);
    pthread_cond_signal (&pool-> cond_queue);

    return 0;
}

int TH_PoolWait(TH_Pool* pool) {
    int error = pthread_mutex_lock(&pool->mutex_queue);

	while (pool->queue.size != 0 || pool->num_threads_running != 0) {
		pthread_cond_wait(&pool->cond_finished, &pool->mutex_queue);
	}

    pthread_mutex_unlock(&pool->mutex_queue);
    return 0;
}

int TH_PoolInit(TH_Pool* pool, uint32_t n_treads) {
    assert(pool);
    if (pool == NULL)
        return -1;

    if (pool->is_running)
        return -1;

    pool->is_running = false;

    pool->queue.head = 0;
    pool->queue.tail = 0;
    pool->queue.size = 0;
    pool->queue.capacity = kQueueStandartSize;
    pool->n_threads = n_treads;
    pool->num_threads_running = 0;
    pool->n_threads_alive = 0;

    int error = 0;
    error = pthread_mutex_init(&pool->mutex_queue, NULL);
    if (error) {
        return error;
    }

    error = pthread_cond_init(&pool->cond_queue, NULL);
    if (error) {
        pthread_mutex_destroy(&pool->mutex_queue);
        return error;
    }

    pool->threads = (pthread_t*) calloc(n_treads, sizeof(pthread_t));
    if (pool->threads == NULL)
    {
        pthread_mutex_destroy(&pool->mutex_queue);
        pthread_cond_destroy(&pool->cond_queue);
        return error;
    }

    pool->queue.jobs = (TH_Job*) calloc(kQueueStandartSize, sizeof(TH_Job));
    if (pool->queue.jobs == NULL) {
        free(pool->threads);
        pthread_mutex_destroy(&pool->mutex_queue);
        pthread_cond_destroy(&pool->cond_queue);
        return -1;
    }

    error = pthread_mutex_init(&pool->mutex_finished, NULL);
    if (error)
    {
        free(pool->queue.jobs);
        free(pool->threads);
        pthread_mutex_destroy(&pool->mutex_queue);
        pthread_cond_destroy(&pool->cond_queue);
        return error;
    }

    error = pthread_cond_init(&pool->cond_finished, NULL);
    if (error) {
        free(pool->queue.jobs);
        free(pool->threads);
        pthread_mutex_destroy(&pool->mutex_queue);
        pthread_mutex_destroy(&pool->mutex_finished);
        pthread_cond_destroy (&pool->cond_queue);
        return error;
    }

    pool->is_running = true;
    for (uint32_t i = 0; i < pool->n_threads; i++) {
        error = pthread_create(&pool->threads[i], NULL,
                                         (void* (*)(void*))TH_ThreadInit, pool);
        if (error) {
            for (int j = 0; j < i; j++) {
                pthread_join(pool->threads[j], NULL);
            }
            free(pool->threads);
            free(pool->queue.jobs);
            pthread_mutex_destroy(&pool->mutex_queue);
            pthread_mutex_destroy(&pool->mutex_finished);
            pthread_cond_destroy(&pool->cond_queue);
            pthread_cond_destroy(&pool->cond_finished);
            pool->is_running = false;
            return -1;
        }
    }

    return 0;
}

static void TH_StopThreads(TH_Pool* pool) {
    assert(pool);

    pthread_mutex_lock  (&pool->mutex_queue);

    pool->is_running = false;

    pthread_mutex_unlock(&pool->mutex_queue);

    volatile uint32_t n_threads_alive = 0;

    do {
        pthread_cond_broadcast(&pool->cond_queue);
        pthread_cond_broadcast(&pool->cond_finished);
        sleep(1);

        pthread_mutex_lock(&pool->mutex_queue);
        n_threads_alive = pool->n_threads_alive;
        pthread_mutex_unlock(&pool->mutex_queue);

    } while (n_threads_alive != 0);
}

void TH_PoolDtor(TH_Pool* pool) {
    assert(pool);

    TH_StopThreads(pool);

    free(pool->threads);
    free(pool->queue.jobs);
    pthread_mutex_destroy(&pool->mutex_queue);
    pthread_cond_destroy (&pool->cond_queue);
    pthread_cond_destroy (&pool->cond_finished);
    pthread_mutex_destroy(&pool->mutex_finished);
}
