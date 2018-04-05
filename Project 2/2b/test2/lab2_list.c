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
#include <signal.h>

#include "SortedList.h"

#define BILLION 1000000000L

int numThreads, numIterations, opt_yield;

char* yieldOptions;
char syncOption;

// Head of linked lists
SortedList_t * lists;

// Array of elements to insert
SortedListElement_t * elements;
int numElements;

// Mutex lock
pthread_mutex_t * mutexList;

// Spin lock
int * spinLockList;

// Number of lock operations
int lockOperations;

// Number of sublists
int numLists;

struct thread_data {
  int thread_id;
  int startIndex;
  int endIndex;
  long long unsigned int lockTime;
};

void freeMemory() {
  int i;
  for (i = 0; i < numElements; i++) {
    free((char*) elements[i].key);
  }
  free(elements);
  free(lists);
  if (syncOption == 'm')
    free(mutexList);
  else if (syncOption == 's')
    free(spinLockList);
}

void error(char* msg) {
  fprintf(stderr, "%s\n", msg);
  freeMemory();
  exit(1);
}

void corruptedHandler() {
  fprintf(stderr, "Corrupted list\n");
  exit(2);
}

int hash(SortedListElement_t * e) {
  int n, i;
  const char* key = e->key;
  for (i = 0; i < strlen(key); i++)
    n += (int) key[i];
  return n % numLists;
}

int getFinalLength(SortedList_t * lists) {
  int i, length = 0;
  for (i = 0; i < numLists; i++) {
    int listLength = SortedList_length(&lists[i]);
    if (listLength == -1)
      corruptedHandler();
    length += listLength;
  } 
  return length;
}

void* listFunc(void* threadArg) {
  struct thread_data *threadData = (struct thread_data *) threadArg;
  int start = threadData->startIndex;
  int end = threadData->endIndex;

  // Insert
  uint64_t diff;
  long long unsigned int lockTime = 0; 
  struct timespec timeStart, timeEnd;

  int i;
  for (i = start; i < end; i++) {
    int sublistNum = hash(&elements[i]);

    if (syncOption != '\0') {
      if (clock_gettime( CLOCK_REALTIME, &timeStart) == -1)
        error("Error with clock gettime");

      if (syncOption == 'm')
	pthread_mutex_lock(&mutexList[sublistNum]);
      else if (syncOption == 's')
	while (__sync_lock_test_and_set(&spinLockList[sublistNum], 1));

      if (clock_gettime( CLOCK_REALTIME, &timeEnd) == -1)
        error("Error with clock gettime");

      diff = BILLION * (timeEnd.tv_sec - timeStart.tv_sec) + timeEnd.tv_nsec - timeStart.tv_nsec;
      lockTime += (long long unsigned int) diff;
      lockOperations++;
    }

    SortedList_insert(&lists[sublistNum], &elements[i]);

    if (syncOption != '\0') {
      if (syncOption == 'm')
	pthread_mutex_unlock(&mutexList[sublistNum]);
       else if (syncOption == 's')
	__sync_lock_release(&spinLockList[sublistNum]);
    }
  }

  // Get length
  int length = 0;
  for (i = 0; i < numLists; i++) {
    if (syncOption != '\0') {
      if (clock_gettime( CLOCK_REALTIME, &timeStart) == -1)
        error("Error with clock gettime");

      if (syncOption == 'm')
        pthread_mutex_lock(&mutexList[i]);
      else if (syncOption == 's')
        while (__sync_lock_test_and_set(&spinLockList[i], 1));

      if (clock_gettime( CLOCK_REALTIME, &timeEnd) == -1)
        error("Error with clock gettime");

      diff = BILLION * (timeEnd.tv_sec - timeStart.tv_sec) + timeEnd.tv_nsec - timeStart.tv_nsec;
      lockTime += (long long unsigned int) diff;
      lockOperations++;
    }

    int listLength = SortedList_length(&lists[i]);

    if (listLength == -1)
      corruptedHandler();
    length += listLength;
    
    if (syncOption != '\0') {
      if (syncOption == 'm')
        pthread_mutex_unlock(&mutexList[i]);
      else if (syncOption == 's')
        __sync_lock_release(&spinLockList[i]);
    }
  }

  // Lookup and delete
  SortedListElement_t * lookup;
  for (i = start; i < end; i++) {
    int sublistNum = hash(&elements[i]);
    if (syncOption != '\0') {
      if (clock_gettime( CLOCK_REALTIME, &timeStart) == -1)
        error("Error with clock gettime");

      if (syncOption == 'm')
        pthread_mutex_lock(&mutexList[sublistNum]);
      else if (syncOption == 's')
        while (__sync_lock_test_and_set(&spinLockList[sublistNum], 1));
      
      if (clock_gettime( CLOCK_REALTIME, &timeEnd) == -1)
        error("Error with clock gettime");

      diff = BILLION * (timeEnd.tv_sec - timeStart.tv_sec) + timeEnd.tv_nsec - timeStart.tv_nsec;
      lockTime += (long long unsigned int) diff;

      lockOperations++;
    }

    lookup = SortedList_lookup(&lists[sublistNum], elements[i].key);
    if (SortedList_delete(lookup) == 1)
      corruptedHandler();

    if (syncOption != '\0') {
      if (syncOption == 'm')
        pthread_mutex_unlock(&mutexList[sublistNum]);
      else if (syncOption == 's')
        __sync_lock_release(&spinLockList[sublistNum]);
    }
  }

  //  fprintf(stderr, "Locktime for thread %d: %llu\n", threadData->thread_id, lockTime);
  threadData->lockTime = lockTime;

  pthread_exit(NULL);
}

void sigsegv_handler(int signum) {
  fprintf(stderr, "Caught a SIGSEGV signal (segmentation fault) \n");
  exit(2);
}

int main (int argc, char **argv)
{
  srand(time(NULL));

  /* Read options (if any) */
  static struct option long_options[] =
    {
      {"threads", optional_argument, 0, 't'},
      {"iterations", optional_argument, 0, 'i'},
      {"yield", required_argument, 0, 'y'},
      {"sync", required_argument, 0, 's'},
      {"lists", required_argument, 0, 'l'},
      {0, 0, 0, 0}
    };

  // Initialize global values
  numThreads = 1;
  numIterations = 1;
  opt_yield = 0;
  yieldOptions = '\0';
  syncOption = '\0';
  lockOperations = 0;
  numLists = 1;
  
  /* getopt_long stores the option index here. */
  int option_index = 0;
  char c;
  
  while ((c = getopt_long(argc, argv, "t:i:y:s:l:", long_options, &option_index)) != -1 ||
	 (c = getopt(argc, argv, "t:i:y:s:l:")) != -1) {
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
       yieldOptions = optarg;

       int k;
       for (k = 0; k < strlen(yieldOptions); k++) {
	 switch (yieldOptions[k]) {
	 case 'i':
	   opt_yield += INSERT_YIELD;
	   break;
	 case 'd':
	   opt_yield += DELETE_YIELD;
	   break;
	 case 'l':
	   opt_yield += LOOKUP_YIELD;
	   break;
	 default:
	   error("Unrecognized option for yield");
	 }
       }
       break;

     case 's':
       syncOption = optarg[0];
       if (syncOption != 'm' && syncOption != 's')
	 error("Unrecognized option for sync");
       break;

     case 'l':
       numLists = atoi(optarg);
       break;

     case '?':
       /* getopt_long already printed an error message. */
       fprintf(stderr, "Usage: ./lab2_list [OPTIONS]\n");
       exit(1);
       
     default:
       abort ();
     }
  }

  // set testName
  char testName[100] = "list";
  if (opt_yield) {
    strcat(testName, "-");
    strcat(testName, yieldOptions);
  }
  else
    strcat(testName, "-none");

  if (syncOption != '\0') {
    strcat(testName, "-");
    strncat(testName, &syncOption, 1);
  }
  else
    strcat(testName, "-none");

  /* Signal to catch SIGPIPE */
  signal(SIGSEGV, &sigsegv_handler);

  // initialize lists
  lists = (SortedList_t *) malloc(sizeof(SortedListElement_t) * numLists);

  int i;
  for (i = 0; i < numLists; i++) {
    SortedListElement_t head;
    head.prev = NULL;
    head.next = NULL;
    head.key = '\0';
    lists[i] = head;
  }

  // create and initialize (with random keys) the required number
  // (threads x iterations) of list elements
  elements = (SortedList_t *) malloc(sizeof(SortedListElement_t) * numThreads * numIterations);
  numElements = numThreads * numIterations;

  for (i = 0; i < numElements; i++) {
    char * c = (char *) malloc(sizeof(char) * 2);
    *c = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"[random () % 26];
    elements[i].key = c;
  }

  // initialize locks
  if (syncOption == 'm') {
    mutexList = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t) * numLists);
    for (i = 0; i < numLists; i++) {
      if (pthread_mutex_init(&mutexList[i], NULL) != 0)
	error("Unable to initialize mutex");
    }
  }
  else if (syncOption == 's') {
    spinLockList = (int *) malloc(sizeof(int) * numLists);
    for (i = 0; i < numLists; i++)
      spinLockList[i] = 0;
  }
  // note the (high resolution) starting time for the run (using clock_gettime(3))
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

  int startIndex = 0;
  for (i = 0; i < numThreads; i++) {
    threadDataArray[i].thread_id = i;
    threadDataArray[i].startIndex = startIndex;
    threadDataArray[i].endIndex = startIndex + numIterations;
    threadDataArray[i].lockTime = 0;
    startIndex += numIterations;

    if (pthread_create(&threads[i], NULL, listFunc, (void*) &threadDataArray[i]) != 0)
      error("Error with pthread_create");
  }

  long long unsigned int totalLockTime = 0;
  /* Free attribute and wait for the other threads */
  pthread_attr_destroy(&attr);
  void * status;
  for (i = 0; i < numThreads; i++) {
    if (pthread_join(threads[i], &status) != 0)
      error("Error with pthread_join");
    totalLockTime += threadDataArray[i].lockTime;
  }

  //  fprintf(stderr, "Total lock time is %llu\n", totalLockTime);
  
  // Stop clock
  if (clock_gettime( CLOCK_REALTIME, &end) == -1)
    error("Error with clock gettime");

  // check final length of list
  int finalLength = getFinalLength(lists);
  if (finalLength != 0)
    corruptedHandler();

  // Format csv 
  uint64_t diff = BILLION * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
  long long unsigned int runtime = (long long unsigned int) diff;
  int numOperations = numThreads * numIterations * 3;
  long long unsigned int waitForLockTime = 0;
  if (lockOperations > 0)
    waitForLockTime = totalLockTime / lockOperations;

  fprintf(stdout, "%s,%d,%d,%d,%d,%llu,%llu,%llu\n", testName, numThreads, numIterations, numLists, numOperations, runtime, runtime / numOperations, waitForLockTime);

  // Free the list
  freeMemory();

  // Exit main thread
  pthread_exit(NULL);
}
