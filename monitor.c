#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <time.h>
#include <sys/types.h>
#include "data_structure.h"

//CPU time in microsecond
clock_t last_cpu_time;
//Elapse time in microsecond
long last_total_time;

//Interval for hardware monitor to update in ms
long monitor_interval = 50;

extern pthread_cond_t status_update_cv;
extern pthread_mutex_t status_update_m;
extern status_info* local_status;

struct timeval temp_time;

void* getCPUUsage();
//Function for thread listening to user's real-time input of trottling value
void listenToUserCommand(void* unusedParam);

void* startMonitor() {
  pthread_t inputThread;
  pthread_create(&inputThread, 0, listenToUserCommand, (void*)0);
  struct timespec sleepFor;
  sleepFor.tv_sec = 0;
  sleepFor.tv_nsec = monitor_interval * 1000 * 1000;
  while(1) {
    getCPUUsage();
    nanosleep(&sleepFor, 0);
    pthread_cond_broadcast(&status_update_cv);
  }
}

void* getCPUUsage() {
  clock_t current_cpu_time = clock() / CLOCKS_PER_SEC * 1000 * 1000;
  gettimeofday(&temp_time, 0);
  long current_total_time = temp_time.tv_sec * 1000 * 1000 + temp_time.tv_usec; 
  if(!last_cpu_time) {
    local_status->cpu_usage = 0;
  }
  else {
    local_status->cpu_usage = (current_cpu_time - last_cpu_time) / (current_total_time - last_total_time);
  }
  last_cpu_time = current_cpu_time;
  last_total_time = current_total_time;
}

void listenToUserCommand(void* unusedParam) {
  char command[100];
  while(read(STDIN_FILENO, command, 100) > 0) {
    local_status->trottling_value = atoi(command);
    pthread_cond_broadcast(&status_update_cv);
  }
}
