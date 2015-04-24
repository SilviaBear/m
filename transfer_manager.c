#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "data_structure.h"

extern SIZE_PER_JOB;
extern int data_sockfd;
extern int accept_data_sockfd;
extern int isLocal;
//Data structre for remote socket if it is local node
extern struct sockaddr_storage their_addr;
//Data structure for local socket information if it is remote node
extern struct addrinfo hints, *servinfo, *p;
extern socklen_t addr_len;

extern double* queue_head;
extern double* jobs_head;
extern status_info* local_status;

extern pthread_mutex_t queue_m;

int sockfd;

void transfer_manager_init() {
  sockfd = isLocal? data_sockfd : accept_data_sockfd;
}

double* find_end();

void* transfer_job(int num) {
  addr_len = sizeof(their_addr);
  int numbytes;
  char sendBuf[256];
  memcpy(sendBuf, "START:", 6);
  memcpy(sendBuf + 6, &num, sizeof(int));
  //Send sentinel packet for job transfer start
  if((numbytes = sendto(sockfd, sendBuf, 6 + sizeof(int), 0, p->ai_addr, p->ai_addrlen)) == -1) {
    perror("sendto");
    exit(1);
  }
  pthread_mutex_lock(&queue_m);
  double* queue_end = find_end();
  int i;
  //Send each job trunk
  for(i = 0; i < num; i++) {
    //If local node, transfer the jobs from last significant bit
    if(isLocal) {
      if((numbytes = sendto(sockfd, queue_end + i * SIZE_PER_JOB, SIZE_PER_JOB * sizeof(double), 0, p->ai_addr, p->ai_addrlen)) == -1) {
        perror("sendto");
        exit(1);
      }
    }
    //If remote node, transfer the jobs from most significant bit
    else {
      if((numbytes = sendto(sockfd, queue_end - i * SIZE_PER_JOB, SIZE_PER_JOB * sizeof(double), 0, p->ai_addr, p->ai_addrlen)) == -1) {
        perror("sendto");
        exit(1);
      }
    }
  }
  //Decrease the local work queue length
  local_status->queue_length -= num;
  pthread_mutex_unlock(&queue_m);
  printf("Transfer work to remote node: %d trunks\n", num);
}

void accept_job() {
  int numbytes;
  char* recvBuf = (char*)malloc(256 * sizeof(char));
  while(1) {
    if((numbytes = recvfrom(sockfd, recvBuf, 256, 0, (struct sockaddr *)&their_addr, &addr_len)) == -1) {
      perror("recvfrom");
      exit(1);
    }
    if(strstr(recvBuf, "START")) {
      int num = *((int*)recvBuf);
      int i;
      pthread_mutex_lock(&queue_m);
      double* end_of_queue = find_end();
      for(i = 0; i < num; i++) {
        //For local node, put new jobs after the least significant bit of current job
        if(isLocal) {
          if((numbytes = recvfrom(sockfd, end_of_queue + i * SIZE_PER_JOB, SIZE_PER_JOB * sizeof(double), 0, (struct sockaddr *)&their_addr, &addr_len)) == -1) {
            perror("recvfrom");
            exit(1);
          }
        }
        //For remote node, put the new jobs before the most significant bit of current job list
        else{
          if((numbytes = recvfrom(sockfd, end_of_queue - i * SIZE_PER_JOB, SIZE_PER_JOB * sizeof(double), 0, (struct sockaddr *)&their_addr, &addr_len)) == -1) {
            perror("recvfrom");
            exit(1);
          }
        }
      }
      local_status->queue_length += num;
      pthread_mutex_unlock(&queue_m);
    }
  }
}

double* find_end() {
  if(isLocal) {
    return queue_head + local_status->queue_length * SIZE_PER_JOB;
  }
  else {
    return queue_head - local_status->queue_length * SIZE_PER_JOB;
  }
}
