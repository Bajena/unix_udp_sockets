#ifndef common_h
#define common_h

#include <netinet/in.h>

struct message* create_message(char type, char *text, struct sockaddr_in *addr) ;
void destroy_message(struct message* msg);

struct message {
      int type;
      char *text;
      struct sockaddr_in *addr;
};


#endif
