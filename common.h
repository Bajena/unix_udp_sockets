#ifndef common_h
#define common_h

#include <netinet/in.h>

#define ERR(source) (perror(source),\
         fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
         exit(EXIT_FAILURE))

#define BUFFER_SIZE 512
#define MAX_ATTEMPTS 3
#define CONFIRM_TIME 500000

#define REGISTER_MESSAGE '0'
#define TASK_MESSAGE '1'
#define RESULT_CONFIRM_MESSAGE '2'
#define END_MESSAGE '3'

struct message {
      int type;
      char *text;
      struct sockaddr_in *addr;
};

int make_socket(void);
void* mymalloc (size_t size);
int sethandler( void (*f)(int), int sigNo);
void set_alarm(int sec, int usec);
void cancel_alarm();
int send_datagram(int sock,struct sockaddr_in *addr, char type,char *text);
struct message* recv_datagram(int sock);
struct message* create_message(char type, char *text, struct sockaddr_in *addr) ;
void destroy_message(struct message* msg);
int solve_task(char *task) ;


#endif
