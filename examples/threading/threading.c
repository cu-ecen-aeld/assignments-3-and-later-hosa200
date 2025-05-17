#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
//#define DEBUG_LOG(msg, ...)
#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg, ...) printf("threading ERROR: " msg "\n", ##__VA_ARGS__)

void *threadfunc(void *thread_param)
{
    struct thread_data *thread_func_args = (struct thread_data *)thread_param;

    DEBUG_LOG("Task started %d",thread_func_args->tsk_id);
    usleep(thread_func_args->wait_to_obtain_ms);

    pthread_mutex_lock(thread_func_args->mutex);
    DEBUG_LOG("Task %d took mutex",thread_func_args->tsk_id);
    usleep(thread_func_args->wait_to_release_ms);
    pthread_mutex_unlock(thread_func_args->mutex);
    DEBUG_LOG("Task %d released mutex",thread_func_args->tsk_id);
    thread_func_args->thread_complete_success = true;
    DEBUG_LOG("Task %d will end now",thread_func_args->tsk_id);

    return (void *)thread_func_args;
}

bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex, int wait_to_obtain_ms, int wait_to_release_ms)
{
    struct thread_data *thrd_data_ptr = (struct thread_data *)malloc(sizeof(struct thread_data));

    // Checking if failed or pass
    if (thrd_data_ptr != NULL)
    {

        DEBUG_LOG("Allocation of memory success");

        thrd_data_ptr->tsk_id = *thread;
        thrd_data_ptr->wait_to_obtain_ms = wait_to_obtain_ms;
        thrd_data_ptr->wait_to_release_ms = wait_to_release_ms;
        thrd_data_ptr->thread_complete_success = false;
        thrd_data_ptr->mutex = mutex;

        // pthread_mutex_init(thrd_data_ptr->mutex, NULL);

        pthread_create(thread,
                       NULL,
                       threadfunc,
                       (void *)thrd_data_ptr);
    }
    else
    {
        ERROR_LOG("Allocation of memory failed");
        return false;
    }

    if (((struct thread_data *)thrd_data_ptr)->thread_complete_success)
    {
        free(thrd_data_ptr);
    }
  
        return true;
}
