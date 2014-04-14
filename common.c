#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

struct message* create_message(char type, char *text, struct sockaddr_in *addr) {
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