/*
 *	File	: pc.c
 *
 *	Title	: Demo Producer/Consumer.
 *
 *	Short	: A solution to the producer consumer problem using
 *		pthreads.	
 *
 *	Long 	:
 *
 *	Author	: Andrae Muys
 *
 *	Date	: 18 September 1997
 *
 *	Changed	: Triantafillidis Grigoris 8923 @2022
 */

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <math.h>
#include <limits.h>

#define PI 3.14159265
#define QUEUESIZE 10

// int n = PROD_LOOP;
long * times;
int times_index = 0;
pthread_mutex_t times_mut;

typedef struct {
  void * (*work)(void *);
  void * arg;

  int id;
  struct timeval start, end;
  long waitTime; // in microseconds

} workFunction;

typedef struct {
  int production_loop; // amount of jobs each producer makes
  int producers;       // number of producers
  int consumers;       // number of consumers

  workFunction * buf[QUEUESIZE]; // a buffer of workFunction pointers
  long head, tail;
  int full, empty;
  pthread_mutex_t *mut;
  pthread_cond_t *notFull, *notEmpty;
} queue;

long calcTimeInMicroseconds(struct timeval *start, struct timeval *end) {
  return ((end->tv_sec * 1000000 + end->tv_usec) - (start->tv_sec * 1000000 + start->tv_usec));
}

void runProblem(int producers, int consumers, int production_loop, double results[4]);
void *producer (void *args);
void *consumer (void *args);

queue *queueInit (int producers, int consumers, int production_loop);
void queueDelete (queue *q);
void queueAdd (queue *q, workFunction *foo);
void queueDel (queue *q, workFunction *foo);

void calcStats(long * times, int n, double results[4]);
void *work1(void *a);



/************************************/
/*              Main                 /
/************************************/
int main (int argc, char **argv)
{
  int producers;
  int consumers;
  int production_loop = 50000;
  if (argc == 3) {
    producers = atoi(argv[1]);
    consumers = atoi(argv[2]);
  } else {
    producers = 10;
    consumers = 10;
  }
  FILE *f = fopen("results.txt", "w+");
  double results[4];
  int maxConsumers = 101;
  producers = 10;
  fprintf(f, "\n--- Producers: %d ---\n", producers);
  for (int i = 1; i < maxConsumers; i++) {
    consumers = i;
    printf("producers: %d, consumers: %d, production_loop: %d\n", producers, consumers, production_loop);
    runProblem(producers, consumers, production_loop, results);
    //printf("Range: %.0f, Mean: %.2f, Var: %.2f, StDev: %.2f\n", results[0], results[1], results[2], results[3]);
    fprintf(f, "%d,%.0f,%.2f,%.2f,%.2f\n", consumers, results[0], results[1], results[2], results[3]);
    times_index = 0;
  }

  // producers = 15;
  // fprintf(f, "\n--- Producers: %d ---\n", producers);
  // for (int i = 1; i < maxConsumers; i++) {
  //   consumers = i;
  //   printf("producers: %d, consumers: %d, production_loop: %d\n", producers, consumers, production_loop);
  //   runProblem(producers, consumers, production_loop, results);
  //   //printf("Range: %.0f, Mean: %.2f, Var: %.2f, StDev: %.2f\n", results[0], results[1], results[2], results[3]);
  //   fprintf(f, "%d,%.0f,%.2f,%.2f,%.2f\n", consumers, results[0], results[1], results[2], results[3]);
  //   times_index = 0;
  // }

  // producers = 20;
  // fprintf(f, "\n--- Producers: %d ---\n", producers);
  // for (int i = 1; i < maxConsumers; i++) {
  //   consumers = i;
  //   printf("producers: %d, consumers: %d, production_loop: %d\n", producers, consumers, production_loop);
  //   runProblem(producers, consumers, production_loop, results);
  //   //printf("Range: %.0f, Mean: %.2f, Var: %.2f, StDev: %.2f\n", results[0], results[1], results[2], results[3]);
  //   fprintf(f, "%d,%.0f,%.2f,%.2f,%.2f\n", consumers, results[0], results[1], results[2], results[3]);
  //   times_index = 0;
  // }
  
  // producers = 25;
  // fprintf(f, "\n--- Producers: %d ---\n", producers);
  // for (int i = 1; i < maxConsumers; i++) {
  //   consumers = i;
  //   printf("producers: %d, consumers: %d, production_loop: %d\n", producers, consumers, production_loop);
  //   runProblem(producers, consumers, production_loop, results);
  //   //printf("Range: %.0f, Mean: %.2f, Var: %.2f, StDev: %.2f\n", results[0], results[1], results[2], results[3]);
  //   fprintf(f, "%d,%.0f,%.2f,%.2f,%.2f\n", consumers, results[0], results[1], results[2], results[3]);
  //   times_index = 0;
  // }
  
  fclose(f);
  free(times);
  return 0;
}


/************************************/
/*           Functions               /
/************************************/

void runProblem(int producers, int consumers, int production_loop, double results[4]) {
  queue *fifo;
  pthread_t pro;
  pthread_t con;

  //printf(" -- Producer-Consumer problem with PThreads --\n");

  fifo = queueInit (producers, consumers, production_loop);
  // init times array
  int timesSize = producers * production_loop;
  times = (long *)malloc(sizeof(long) * timesSize);
  if (fifo ==  NULL) {
    fprintf (stderr, "main: Queue Init failed.\n");
    exit (1);
  }
  
  for (int i = 0; i < producers; i++) {
    pthread_create (&pro, NULL, producer, fifo);
  }

  for (int i = 0; i < consumers; i++) {
    pthread_t con;
    pthread_create (&con, NULL, consumer, fifo);
    pthread_join (con, NULL);
  }
  pthread_join (pro, NULL);
  queueDelete (fifo);

  calcStats(times, timesSize, results);
}


void *producer (void *q)
{
  queue *fifo;
  fifo = (queue *)q;
  
  for (int i = 0; i < fifo->production_loop; i++) {
    pthread_mutex_lock (fifo->mut);
    while (fifo->full) {
      //printf ("producer: queue FULL.\n");
      pthread_cond_wait (fifo->notFull, fifo->mut);
    }

    // add function to queue
    workFunction *foo = (workFunction *)malloc(sizeof(workFunction));
    foo->work = work1;
    char *mystring = (char *)malloc(sizeof(char) * 20);
    sprintf(mystring, "hello: %d", i);
    foo->arg = (void *)mystring;
    foo->id = i;
    gettimeofday(&foo->start, NULL); // save the timestamp
    queueAdd (fifo, foo);

    //printf ("producer: Added %d. Head: %ld, Tail: %ld\n", i, fifo->head, fifo->tail);
    pthread_mutex_unlock (fifo->mut);
    pthread_cond_signal (fifo->notEmpty);
  }
  return (NULL);
}


void *consumer (void *q)
{
  queue *fifo;
  fifo = (queue *)q;
  
  while(1) {
    // check if all jobs are finished
    if (times_index >= fifo->production_loop * fifo->producers) {
      //printf("Reached the total number of Works\n");
      //pthread_cond_signal(fifo->notEmpty);
      break;
    }

    // else get a new job from the queue or wait for a new job if queue is empty
    pthread_mutex_lock (fifo->mut);
    while (fifo->empty) {
      //printf ("consumer: queue EMPTY.\n");
      pthread_cond_wait (fifo->notEmpty, fifo->mut);
    }
    workFunction funcRecieved;
    queueDel (fifo, &funcRecieved);
    gettimeofday(&funcRecieved.end, NULL); // get the timestamp
    funcRecieved.waitTime = calcTimeInMicroseconds(&funcRecieved.start, &funcRecieved.end); // calc time spend waiting in queue
    pthread_mutex_unlock (fifo->mut);
    pthread_cond_signal (fifo->notFull);

    // add the time to the times array
    pthread_mutex_lock(&times_mut);
    times[times_index] = funcRecieved.waitTime;
    times_index++;
    pthread_mutex_unlock(&times_mut);

    //printf ("consumer: recieved func %d, in %ld us. Head: %ld, Tail: %ld\n", funcRecieved.id, funcRecieved.waitTime, fifo->head, fifo->tail);
    // consumer does the work to waste some time
    funcRecieved.work(funcRecieved.arg);
  }
  return (NULL);
}


queue *queueInit (int producers, int consumers, int production_loop)
{
  queue *q;

  q = (queue *)malloc (sizeof (queue));
  if (q == NULL) {
    printf("Error: queue was not initialized!\n");
    return (NULL);
  };

  q->producers = producers;
  q->consumers = consumers;
  q->production_loop = production_loop;
  q->empty = 1;
  q->full = 0;
  q->head = 0;
  q->tail = 0;
  q->mut = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));
  pthread_mutex_init (q->mut, NULL);
  q->notFull = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
  pthread_cond_init (q->notFull, NULL);
  q->notEmpty = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
  pthread_cond_init (q->notEmpty, NULL);
	
  return (q);
}


void queueDelete (queue *q)
{
  pthread_mutex_destroy (q->mut);
  free (q->mut);	
  pthread_cond_destroy (q->notFull);
  free (q->notFull);
  pthread_cond_destroy (q->notEmpty);
  free (q->notEmpty);
  free (q);
}

void queueAdd (queue *q, workFunction *foo)
{
  q->buf[q->tail] = foo;
  q->tail++;
  if (q->tail == QUEUESIZE)
    q->tail = 0;
  if (q->tail == q->head)
    q->full = 1;
  q->empty = 0;

  return;
}

void queueDel (queue *q, workFunction *foo)
{
  *foo = *q->buf[q->head];
  q->head++;
  if (q->head == QUEUESIZE)
    q->head = 0;
  if (q->head == q->tail)
    q->empty = 1;
  q->full = 0;

  return;
}

void *work1(void *a) {
  //printf("work: %s\n", (char *)a);
  double x, ret, val;
  for (int i = 0; i < 70; i++) {
    x = 45.0 + (double)i;
    val = PI / 180;
    ret = sin(x * val);
  }
  return a;
}


void calcStats(long * times, int n, double results[4]) {
  // calc Mean Value
  long min = LONG_MAX;
  long max = 0;
  float sum = 0;
  for (int i = 0; i < n; i++) {
    sum += (float)times[i];
    // find min and max
    if (times[i] > max) {
      max = times[i];
    }
    if (times[i] < max) {
      min = times[i];
    }
  }
  double range = (double)(max - min);
  float meanValue = sum/n;

  // calc Variance
  float sum2 = 0;
  for (int i = 0; i < n; i++) {
    sum2 += ((float)times[i] - meanValue) * ((float)times[i] - meanValue);
  }
  float variance = sum2/n;
  float standardDev = sqrt(variance);
  
  if (n == 1) {
    range = 0;
  }
  results[0] = range;
  results[1] = meanValue;
  results[2] = variance;
  results[3] = standardDev;
}