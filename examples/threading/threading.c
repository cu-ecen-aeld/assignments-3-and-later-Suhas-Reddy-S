#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    
    struct thread_data *th_data = (struct thread_data *)thread_param;

    // Sleep for wait_to_obtain_ms milliseconds
    usleep(th_data->wait_to_obtain_ms * 1000);

    // Obtain the mutex
    pthread_mutex_lock(th_data->mutex);

    // Sleep for wait_to_release_ms milliseconds
    usleep(th_data->wait_to_release_ms * 1000);

    // Release the mutex
    pthread_mutex_unlock(th_data->mutex);

    // Set thread complete success flag
    th_data->thread_complete_success = true;
    
    free(th_data);
    
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */
     
     struct thread_data *th_data = (struct thread_data *)malloc(sizeof(th_data));
     if (th_data == NULL) {
         return false;
     }
     
     // Initialize thread data
    th_data->mutex = mutex;
    th_data->wait_to_obtain_ms = wait_to_obtain_ms;
    th_data->wait_to_release_ms = wait_to_release_ms;
    th_data->thread_complete_success = false;

    // Create the thread
    if (pthread_create(thread, NULL, threadfunc, (void *)th_data) != 0) {
        free(th_data);  // Free allocated memory on thread creation failure
        return false;
    }

    if (th_data->thread_complete_success == true) {
        free(th_data);
    }

    return true;  // Thread started successfully
     
}

