/*******************************************************************************

Rozwiazanie zadania z socket'ow udp
								Marcin Borkowski
********************************************************************************/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netdb.h>

#include "common.h"


#define ERR(source) (perror(source),\
		     fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
		     exit(EXIT_FAILURE))
#define MAX_ATTEMPTS 3
#define MAXCONNECTED 10
#define BUFFER_SIZE 512

volatile sig_atomic_t alrm=0;

void sig_alrm(int i){
  alrm=1;
}

int sethandler( void (*f)(int), int sigNo) {
  struct sigaction act;
  memset(&act, 0, sizeof(struct sigaction));
  act.sa_handler = f;
  if (-1==sigaction(sigNo, &act, NULL))
    return -1;
  return 0;
}

int make_socket(void){
	int sock;
	sock = socket(PF_INET,SOCK_DGRAM,0);
	if(sock < 0) ERR("socket");
	return sock;
}

struct sockaddr_in make_address(char *address, int port){
	struct sockaddr_in addr;
	struct hostent *hostinfo;
	addr.sin_family = AF_INET;
	addr.sin_port = htons (port);
	hostinfo = gethostbyname(address);
	if(hostinfo == NULL)ERR("gethost:");
	addr.sin_addr = *(struct in_addr*) hostinfo->h_addr;
	return addr;
}

void usage(char * name){
	fprintf(stderr,"USAGE: %s address port",name);
}

int send_datagram(int sock,struct sockaddr_in *addr, char type, char* text){
  int status;
  char wiad[BUFFER_SIZE];
  fprintf(stderr, "Proba wyslania: %s\n",text);
  sprintf(wiad,"%c%s",type,text);
  status=TEMP_FAILURE_RETRY(sendto(sock,wiad,strlen(wiad),0, (struct sockaddr *)addr,sizeof(*addr)));
  if(status<0&&errno!=EPIPE&&errno!=ECONNRESET) ERR("sendto");
  return status;
}

void register_player(int sock, struct sockaddr_in *addr) {
    int attempts = 0;
    char recv_buffer[BUFFER_SIZE];
    sigset_t mask, oldmask;
    struct itimerval tv={{1,0},{1,0}};

    sigemptyset (&mask);
    sigaddset (&mask, SIGALRM);
    sigprocmask (SIG_BLOCK, &mask, &oldmask);

    for (attempts = 0; attempts< MAX_ATTEMPTS; attempts++) {
          setitimer(ITIMER_REAL,&tv,NULL);
          if(send_datagram(sock,addr,'0', "Register")<0){
                fprintf(stderr,"Send error\n");
          }
          if (recv(sock,recv_buffer,BUFFER_SIZE,0)<0){
              if(EINTR!=errno)ERR("recv:");
              if(alrm) {
                alrm = 0;
                continue;
              }
        }
        break;
    }
    fprintf(stderr, "Zarejestrowano pomyslnie\n" );
    sigprocmask (SIG_UNBLOCK, &mask, NULL);
}


struct message* recv_datagram(int sock,struct sockaddr_in *addr){
  char buf[BUFFER_SIZE];
  int len;
  struct message *msg;
  socklen_t addrlen = sizeof(struct sockaddr_in);

  if((len = TEMP_FAILURE_RETRY(recvfrom(sock,buf,BUFFER_SIZE,0,(struct sockaddr*) addr,&addrlen)))<1)
    ERR("recvfrom");
      buf[len] = '\0';
      msg =  (struct message*)create_message(buf[0], buf+1, addr);
      if (msg!=NULL) {
       fprintf(stderr,"Dlugosc:%d , wiadomosc: %s , od: %s:%d\n",len,msg->text, (char*)inet_ntoa(msg->addr->sin_addr) , msg->addr->sin_port);
      }
  return msg;
}


void work(int sfd, struct sockaddr_in *addr) {
    register_player(sfd,addr);
 //  struct message *msg;
 //  if(send_datagram(sfd,addr,'0', "Message")<0){
 //          fprintf(stderr,"Send error\n");
 //  }

 //  msg = recv_datagram(sfd, addr);

 //  destroy_message(msg);

	// printf("\n");
}

int main(int argc, char** argv) {
	int sock;
	if (argc != 3) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}
	sock=make_socket();
       struct sockaddr_in addr;

       addr = make_address(argv[1],(short)atoi(argv[2]));

       if(sethandler(sig_alrm,SIGALRM)) ERR("Seting SIGALRM:");
       work(sock, &addr);
	if(TEMP_FAILURE_RETRY(close(sock))<0)ERR("close");
	return EXIT_SUCCESS;
}


