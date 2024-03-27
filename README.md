# Thread Pool
Simple thread pool implementation in C. 

## Run an example
Compile the example using GCC:
```
gcc example.c thread_pool.c -o example
```
Then run the executable:
```
./example
```

## Usage

| Function | Description |
| --- | --- |
| `int TH_PoolInit(TH_Pool* pool, const uint32_t n_threads)` | Initialize the thread pool structure with the specified number of threads. |
| `int TH_PoolAddJob(TH_Pool* pool, const TH_Job job)` | Add a new job to the pool. |
| `int TH_PoolWait(TH_Pool* pool)` | Wait for all jobs to complete, including those currently in the pool. |
| `void TH_PoolDtor(TH_Pool* pool)` | Wait for all jobs to finish and destroy the thread pool. |

## System
This library has been tested on a Linux system. It may work on other platforms, but it is not guaranteed.
