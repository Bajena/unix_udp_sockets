#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <signal.h>

#include "common.h"

int sethandler( void (*f)(int), int sigNo) {
  struct sigaction act;
  memset(&act, 0, sizeof(struct sigaction));
  act.sa_handler = f;
  if (-1==sigaction(sigNo, &act, NULL))
    return -1;
  return 0;
}

void set_alarm(int sec, int usec) {
  struct itimerval tv;
  memset(&tv, 0, sizeof(struct itimerval));
  tv.it_value.tv_sec = sec;
  tv.it_value.tv_usec=usec;

  setitimer(ITIMER_REAL,&tv,NULL);
}

void cancel_alarm() {
  set_alarm(0,0);
}

struct message* create_message(char type, char *text, struct sockaddr_in *addr) {
     if (strlen(text) == 0) return NULL;
     struct message *msg;
     struct sockaddr_in *addr_clone;
     msg =  (struct message*)malloc(sizeof(struct message));
     msg->type = type;
     msg->text =  (char*)malloc(strlen(text));
     strcpy(msg->text,text);

     if (addr!=NULL){
      addr_clone = (struct sockaddr_in* )malloc(sizeof(struct sockaddr_in ));
      *addr_clone = *addr;
      msg->addr = addr_clone;
      }
     return msg;
}

void destroy_message(struct message* msg) {
  free(msg->text);
  free(msg->addr);
  free(msg);
}

int solve_task(char *task) {
  int i;
  int sum = 0;
  for (i = 0;i<strlen(task);i++) {
    sum += (int)(task[i] - '0');
  }

  return sum;
}