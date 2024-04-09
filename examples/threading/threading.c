#include "threading.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Optional: use these functions to add debug or error prints to your
// application
#define DEBUG_LOG(msg, ...)
// #define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg, ...) printf("threading ERROR: " msg "\n", ##__VA_ARGS__)

void *threadfunc(void *thread_param) {
  struct thread_data *data = (struct thread_data *)thread_param;

  // waiting to obtain
  DEBUG_LOG("Waiting to obtain lock")
  usleep(data->wait_to_obtain_ms);

  DEBUG_LOG("Locking")
  int rc = pthread_mutex_lock(data->mutex);
  if (rc != 0) {
    ERROR_LOG("failed to obtain lock");
  } else {
    data->thread_complete_success = true;

    // waiting to release
    DEBUG_LOG("Waiting to release lock")
    usleep(data->wait_to_release_ms);
    rc = pthread_mutex_unlock(data->mutex);
    if (rc != 0) {
      ERROR_LOG("failed to release lock");
    }
  }

  return thread_param;
}

bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,
                                  int wait_to_obtain_ms,
                                  int wait_to_release_ms) {
  /* Initiliaze thread data */
  struct thread_data* data =
      (struct thread_data*)malloc(sizeof(struct thread_data));
  data->mutex = mutex;
  data->wait_to_obtain_ms = wait_to_obtain_ms;
  data->wait_to_release_ms = wait_to_release_ms;
  data->thread_complete_success = false;

  /*lauch thread*/
  return pthread_create(thread, NULL, threadfunc, (void*)data) == 0;
}
