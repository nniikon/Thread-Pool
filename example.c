#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "thread_pool.h"

void long_calculations(void* arg) {
    fprintf(stderr, "My favorite number is #%d\n", rand() % 100);
    sleep(5);
}

int main() {
    int error = 0;
    TH_Pool pool = {};
    const uint32_t n_threads = 4;

    error = TH_PoolInit(&pool, n_threads);
    if (error)
    {
        fprintf(stderr, "Error initializing thread pool\n");
        return -1;
    }

    TH_Job job = 
    {
        .func = long_calculations,
        .args = NULL,
    };

    for (int i = 0; i < 12; i++)
    {   
        error = TH_PoolAddJob(&pool, job);
        if (error)
        {
            fprintf(stderr, "Error adding a new job\n");
            TH_PoolDtor(&pool);
            return -1;
        }
    }

    TH_PoolWait(&pool);

    TH_PoolDtor(&pool);

    return 0;
}
