/*
NAME: Chaoran Lin
EMAIL: linmc@ucla.edu
ID: 004674598
*/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <sched.h>

#define BILLION 1000000000L

int numThreads, numIterations, opt_yield;

char syncOption;

// Mutex lock
pthread_mutex_t count_mutex;

// Spin lock
int spinLock = 0;
struct thread_data {
  int thread_id;
  long long * pointer;
};

void add(long long *pointer, long long value) {
  long long sum = *pointer + value;
  if (opt_yield)
    sched_yield();
  *pointer = sum;
}

// Version of add function with mutexes
void mutexAdd(long long *pointer, long long value) {
  pthread_mutex_lock(&count_mutex);
  add(pointer, value);
  pthread_mutex_unlock(&count_mutex);
}

// Version of add function with spin locks
void spinlockAdd(long long *pointer, long long value) {
  // Keep threads spinning while lock is not yet released
  while (__sync_lock_test_and_set(&spinLock, 1));
  add(pointer, value);
  __sync_lock_release(&spinLock);
}

void compareSwapAdd(long long *pointer, long long value) {
  long long old, new;
  do {
    old = *pointer;
    new = old + value;
    if (opt_yield)
      sched_yield();
  } while (__sync_val_compare_and_swap(pointer, old, new) != old);
  // only modify pointer if old and new are the same
}

void error(char* msg) {
  fprintf(stderr, "%s\n", msg);
  exit(1);
}

void *ThreadFunc(void* threadArg) {
  struct thread_data *threadData = (struct thread_data *) threadArg; 
  //  int tid = threadData->thread_id;
  long long * pointer = threadData->pointer;

  //printf("Hello world! It's me, thread #%ld!\n", tid+1);
  
  int i;

  // Increment
  int value = 1;
  for (i = 0; i < numIterations; i++) {
    switch(syncOption) {
    case 'm':
      mutexAdd(pointer, value);
      break;
    case 's':
      spinlockAdd(pointer, value);
      break;
    case 'c':
      compareSwapAdd(pointer, value);
      break;
    default:
      add(pointer, value);
    }
  }

  // Decrement
  value = -1;
  for (i = 0; i < numIterations; i++) {
    switch(syncOption) {
    case 'm':
      mutexAdd(pointer, value);
      break;
    case 's':
      spinlockAdd(pointer, value);
      break;
    case 'c':
      compareSwapAdd(pointer, value);
      break;
    default:
      add(pointer, value);
    }
  }

  pthread_exit(NULL);
}

int main (int argc, char **argv)
{
  
  /* Read options (if any) */
  static struct option long_options[] =
    {
      {"threads", optional_argument, 0, 't'},
      {"iterations", optional_argument, 0, 'i'},
      {"yield", no_argument, 0, 'y'},
      {"sync", required_argument, 0, 's'},
      {0, 0, 0, 0}
    };

  // Initialize global values
  numThreads = 1;
  numIterations = 1;
  opt_yield = 0;
  syncOption = '\0';

  /* getopt_long stores the option index here. */
  int option_index = 0;
  char c;
  
  while ((c = getopt_long(argc, argv, "t:i:ys", long_options, &option_index)) != -1 ||
	 (c = getopt(argc, argv, "t:i:ys")) != -1) {
    switch (c)
     {
     case 't':
       numThreads = atoi(optarg);
       if (numThreads < 1)
	 error("Must specify at least one thread\n");
       break;
       
     case 'i':
       numIterations = atoi(optarg);
       break;

     case 'y':
       opt_yield = 1;
       break;

     case 's':
       syncOption = optarg[0];
       if (syncOption != 'm' && syncOption != 's' && syncOption != 'c')
	 error("Unrecognized option for sync");
       break;

     case '?':
       /* getopt_long already printed an error message. */
       fprintf(stderr, "Usage: ./lab2_add [OPTIONS]\n");
       exit(1);
       
     default:
       abort ();
     }
  }

  // set testName
  char testName[100] = "add";
  if (opt_yield)
    strcat(testName, "-yield");
  if (syncOption == '\0')
    strcat(testName, "-none");
  else {
    strcat(testName, "-");
    strncat(testName, &syncOption, 1);
  }

  // note the (high resolution) starting time for the run (using clock_gettime(3))
  long long counter = 0;
  struct timespec start, end;
  
  if (clock_gettime( CLOCK_REALTIME, &start) == -1)
    error("Error with clock gettime");
  
  // create threads
  pthread_t threads[numThreads];
  struct thread_data threadDataArray[numThreads];
  pthread_attr_t attr;

  /* Initialize and set thread attribute to joinable*/
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

  int i;
  for (i = 0; i < numThreads; i++) {
    threadDataArray[i].thread_id = i;
    threadDataArray[i].pointer = &counter;
    if (pthread_create(&threads[i], NULL, ThreadFunc, (void*) &threadDataArray[i]) != 0)
      error("Error with pthread_create");
  }

  /* Free attribute and wait for the other threads */
  pthread_attr_destroy(&attr);
  void * status;
  for (i = 0; i < numThreads; i++) {
    if (pthread_join(threads[i], &status) != 0)
      error("Error with pthread_join");
  }

  // Stop clock
  if (clock_gettime( CLOCK_REALTIME, &end) == -1)
    error("Error with clock gettime");

  // Format csv 
  uint64_t diff = BILLION * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
  long long unsigned int runtime = (long long unsigned int) diff;
  int numOperations = numThreads * numIterations * 2;

  fprintf(stdout, "%s,%d,%d,%d,%llu,%llu,%llu\n", testName, numThreads, numIterations, numOperations, runtime, runtime / numOperations, counter);

  // Exit main thread
  pthread_exit(NULL);
}
