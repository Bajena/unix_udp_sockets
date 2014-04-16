#ifndef common_h
#define common_h

#include <netinet/in.h>

struct message {
      int type;
      char *text;
      struct sockaddr_in *addr;
};

int sethandler( void (*f)(int), int sigNo);
void set_alarm(int sec, int usec);
void cancel_alarm();
struct message* create_message(char type, char *text, struct sockaddr_in *addr) ;
void destroy_message(struct message* msg);
int solve_task(char *task) ;


#endif
