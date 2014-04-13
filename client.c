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
#include <signal.h>
#include <netdb.h>

#include "common.h"


#define ERR(source) (perror(source),\
		     fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
		     exit(EXIT_FAILURE))
#define MAXTRY 3
#define MAXCONNECTED 10
#define BUFFER_SIZE 512

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

struct message* recv_datagram(int sock,struct sockaddr_in *addr){
  char buf[BUFFER_SIZE];
  int len;
  struct message *msg;
  struct sockaddr_in *addr_clone = (struct sockaddr_in* )malloc(sizeof(struct sockaddr_in ));
  *addr_clone = *addr;
  socklen_t addrlen = sizeof(struct sockaddr_in);

  if((len = TEMP_FAILURE_RETRY(recvfrom(sock,buf,BUFFER_SIZE,0,(struct sockaddr*) addr,&addrlen)))<1)
    ERR("recvfrom");
       buf[len] = '\0';
      msg =  create_message(buf[0], buf+1, addr_clone);
      if (msg!=NULL) {
       fprintf(stderr,"Dlugosc:%d , wiadomosc: %s , od: %s:%d\n",len,msg->text,inet_ntoa(addr->sin_addr) , addr->sin_port);
      }
  return msg;
}

int send_datagram(int sock,struct sockaddr_in *addr,struct message *msg){
  int status;
  fprintf(stderr, "Proba wyslania: %s\n",msg->text);
  char *wiad;
  wiad = (char*)malloc(strlen(msg->text)+sizeof(msg->type));
  sprintf(wiad,"%c%s",msg->type,msg->text);
  status=TEMP_FAILURE_RETRY(sendto(sock,wiad,strlen(wiad),0, (struct sockaddr *)addr,sizeof(*addr)));
  if(status<0&&errno!=EPIPE&&errno!=ECONNRESET) ERR("sendto");
  free(wiad);
  return status;
}

void work(int sfd, struct sockaddr_in *addr) {
  struct message *msg;
  msg = create_message('0', "Message",NULL);
  if(send_datagram(sfd,addr,msg)<0){
          fprintf(stderr,"Send error\n");
        }
  destroy_message(msg);

  msg = recv_datagram(sfd, addr);

  destroy_message(msg);

	printf("\n");
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

       work(sock, &addr);
	if(TEMP_FAILURE_RETRY(close(sock))<0)ERR("close");
	return EXIT_SUCCESS;
}


