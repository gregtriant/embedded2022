/* 

1. scan for Bluetooth MAC addresses (every 10 sec --> return a 48bit value)
2. Define close contacts for each MAC address: a MAC address found in scans between 4 and 20 mins
3. Save close contacts for 14 days
4. Remeber each MAC address for 20 mins. IF there are some that are not close contacts, forget them after 20 mins.

*/


#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>     // srand
#include <sys/time.h> // gettimeofday
#include "macs.h"
#include <pthread.h> // pthread_t
#include <unistd.h>  // sleep

// ----------------------------------- GLOBALS ------------------------------------------------------
typedef struct macaddress {
  char* address;
  long long timestamp; // in ms
} macaddress;

typedef struct func_call {
  char func_name[15];
  long long timestamp;
} func_call;

int DO_SCAN = 1;
FILE *fptr;
pthread_mutex_t file_mutex;

// ----------------------------------  Function declerations  ----------------------------------
// main functions
macaddress BTnearMe();
bool testCOVID();
void uploadContacts(macaddress*, int);

// for threads
void *scan_bt(void *vargp);
void *test_covid(void *vargp);

// other
long long current_timestamp();
int msleep(unsigned int tms);
void saveFuncCall(char *func_name);

// ----------------------------------- MAIN ------------------------------------------------------
int main() {
    printf("\n");
    srand(time(NULL));

    if (pthread_mutex_init(&file_mutex, NULL) != 0) {
      printf("\n mutex init has failed\n");
      return 1;
    }

    fptr = fopen("function_calls.bin", "wb");
    fprintf(fptr, "\n");
    fclose(fptr);

    pthread_t thread_scan_bt;
    pthread_create(&thread_scan_bt, NULL, scan_bt, NULL);
    
    
    pthread_t thread_test_covid;
    pthread_create(&thread_test_covid, NULL, test_covid, NULL);

    pthread_join(thread_test_covid, NULL);
    pthread_join(thread_scan_bt, NULL);

    printf("\n");
    return 0;
}


// --------------------------------------  Functions  -------------------------------------------

macaddress BTnearMe() {
    int r = rand() % n_macs;
    macaddress mac;
    mac.address = macs[r];
    mac.timestamp = current_timestamp();
    return mac;
}

int msleep(unsigned int tms) {
  return usleep(tms * 1000);
}

void *scan_bt(void *vargp) {
  while(DO_SCAN) {
    macaddress mac = BTnearMe();
    
    saveFuncCall("scan_bt");
    msleep(10);
  }
  return NULL;
}

void *test_covid(void *vargp) {
  while(DO_SCAN) {
    bool result = testCOVID();
    saveFuncCall("test_covid");
    msleep(10);
  }
}

long long current_timestamp() {
    struct timeval te; 
    gettimeofday(&te, NULL); // get current time
    long long milliseconds = te.tv_sec*1000LL + te.tv_usec/1000; // calculate milliseconds
    // printf("milliseconds: %lld\n", milliseconds);
    return milliseconds;
}


bool testCOVID() {
  int r = rand() % 2; // returns 0 or 1
  return r;
}

void saveFuncCall(char *func_name) {
  func_call func;
  memcpy(&func.func_name, func_name, sizeof(char)*strlen(func_name));
  func.timestamp = current_timestamp();
  // save function to binary file
  pthread_mutex_lock(&file_mutex);
  fptr = fopen("function_calls.bin", "ab");
  fwrite(&func, sizeof(func_call), 1, fptr);
  fclose(fptr);
  pthread_mutex_unlock(&file_mutex);
}