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

#include "common.h"

#define ERR(source) (perror(source),\
		     fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
		     exit(EXIT_FAILURE))
#define MAXCONNECTED 10
#define MAXTRY 5
#define MINTIME (-MAXTRY*2-1)

#define BUFFER_SIZE 512

volatile sig_atomic_t alrm=0;


struct player {
      int points;
      int registered;
      struct sockaddr_in addr;
};

struct order {
	short time ;
	struct sockaddr_in addr;
};

int sethandler( void (*f)(int), int sigNo) {
	struct sigaction act;
	memset(&act, 0, sizeof(struct sigaction));
	act.sa_handler = f;
	if (-1==sigaction(sigNo, &act, NULL))
		return -1;
	return 0;
}

void sig_alrm(int i){
	alrm=1;
}

int make_socket(int port){
	struct sockaddr_in name;
	int sock;
	sock = socket(PF_INET,SOCK_DGRAM,0);
	if(sock < 0) ERR("socket");
	name.sin_family = AF_INET;
	name.sin_port = htons (port);
	name.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
	if(bind(sock,(struct sockaddr*) &name,sizeof(name)) < 0) ERR("bind");
	fprintf(stderr, "Serwer stoi pod adresem:%s\n",inet_ntoa(name.sin_addr));
	return sock;
}

void usage(char * name){
	fprintf(stderr,"USAGE: %s port\n",name);
}

void update_time(struct order ot[]){
	int i;
	for(i=0;i<MAXCONNECTED;i++)
		if(ot[i].time>MINTIME) ot[i].time--;
}
/*
 * Datagramy sa wysylane i odbierane atomowo do wymiaru MTU
 */

struct message* recv_datagram(int sock){
	char buf[BUFFER_SIZE];
	int len;
      struct sockaddr_in *addr;
      struct message *msg;
      socklen_t addrlen = sizeof(struct sockaddr_in);

      addr = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
	if((len = TEMP_FAILURE_RETRY(recvfrom(sock,buf,BUFFER_SIZE,0,(struct sockaddr*) addr,&addrlen)))<1)
		ERR("recvfrom");
       buf[len] = '\0';
      msg =  create_message(buf[0], buf+1, addr);
      if (msg!=NULL) {
       fprintf(stderr,"Dlugosc:%d , wiadomosc: %s , od: %s:%d\n",len,msg->text,inet_ntoa(addr->sin_addr) , addr->sin_port);
      }
	return msg;
}

// int send_datagram(int sock,struct sockaddr_in addr,short msg){
// 	int status;
// 	short buf=htons(msg);
// 	status=TEMP_FAILURE_RETRY(sendto(sock,&buf,sizeof(short),0,(struct sockaddr*)&addr,sizeof(addr)));
// 	if(status<0&&errno!=EPIPE&&errno!=ECONNRESET) ERR("sendto");
// 	return status;
// }

int send_datagram(int sock,struct player pl, char type,char *text){
  int status;

  char *msg;
  msg = (char*)malloc(strlen(text)+sizeof(type));
  sprintf(msg,"%c%s",type,text);

  status=TEMP_FAILURE_RETRY(sendto(sock,msg,strlen(msg),0,(struct sockaddr*)&(pl.addr),sizeof(pl.addr)));
  if(status<0&&errno!=EPIPE&&errno!=ECONNRESET) ERR("sendto");
  free(msg);
  return status;
}

void process_datagram(struct message* msg, int sock, struct player players[]){

       switch(msg->type){
          case '0':
                    fprintf(stderr,"Rejestracja:%s\n",msg->text);
                    process_register_datagram(msg,sock,players);
                    break;
          case '1':
                    fprintf(stderr,"Rozwiazanie:%s\n",msg->text);
                    break;
          // case default:
          //           fprintf(stderr, "Nieznany");
          //           break;
       }
	// int i ;
	// int free = -1;
	// int pos = -1;
	// for(i=0;i<MAXCONNECTED;i++){
	// 	if(ot[i].time<=MINTIME) free = i;
	// 	else if(0==memcmp(&addr,&ot[i].addr,sizeof(struct sockaddr_in))) pos=i;
	// }
	// if(pos!=-1){
	// 	if(msg>0) {
	// 		ot[pos].time=msg;
	// 		if(send_datagram(sock,ot[pos].addr,msg)<0) ot[pos].time=MINTIME;
	// 	}
	// 	else ot[pos].time=MINTIME;
	// }else if(msg>0){
	// 	if(free!=-1){
	// 		ot[free].time=msg;
	// 		ot[free].addr=addr;
	// 		if(send_datagram(sock,ot[free].addr,msg)<0) ot[free].time=MINTIME;
	// 	}else send_datagram(sock,addr,-1);
	// }
}

void process_register_datagram(struct message* msg, int sock, struct player players[]){
    if (players[0].registered && players[1].registered) {
       fprintf(stderr, "Zarejestrowano juz dwoch graczy!\n");
       return;
    }
    int i;
    for (i = 0;i<2;i++) {
      if (!players[i].registered) {
        register_player(players,i,msg);
        fprintf(stderr, "Zarejestrowano gracza nr.%d\n",i+1);
        if (send_datagram(sock,players[i], '2' ,"Zarejestrowano")<0) ERR("Send");
        return;
      }
    }

}

void register_player(struct player players[], int id, struct message* msg) {
    players[id].registered = 1;
    players[id].addr = *(msg->addr);
}

// void process_orders(struct order ot[],int sock){
// 	int i;
// 	for(i=0;i<MAXCONNECTED;i++)
// 		if(ot[i].time<=0&&ot[i].time>MINTIME&&0==ot[i].time%2)
// 				send_datagram(sock,ot[i].addr,0);
// }

void work(int sfd) {

	fd_set base_rfds, rfds ;
      int fd_count;
      FD_ZERO(&base_rfds);
      FD_SET(sfd, &base_rfds);

      struct message *in_msg;
      struct player players[2];

      for (;;) {
          rfds = base_rfds;
          fd_count = pselect(sfd+1,&rfds, NULL,NULL,NULL,NULL);

          if (fd_count<0) {
            if (errno==EINTR) {

            } else ERR("SELECT");
          } else {
              in_msg = recv_datagram(sfd);
              process_datagram(in_msg,sfd, players);
              destroy_message(in_msg);
          }
      }
	// int ile;
	// short msg;
	// struct sockaddr_in addr;
	// sigset_t mask, oldmask;
	// struct order ot[MAXCONNECTED];
	// struct itimerval tv={{1,0},{1,0}};

	// memset(ot,0xF0,sizeof(ot));
	// sigemptyset (&mask);
	// sigaddset (&mask, SIGALRM);
	// setitimer(ITIMER_REAL,&tv,NULL);
	// FD_ZERO(&base_rfds);
	// FD_SET(sfd, &base_rfds);
	// sigprocmask (SIG_BLOCK, &mask, &oldmask);
	// for(;;){
	// 	rfds=base_rfds;
	// 	ile=pselect(sfd+1,&rfds,NULL,NULL,NULL,&oldmask);
	// 	if(ile<0){
	// 		if(EINTR==errno){
	// 			if(alrm) {
	// 				update_time(ot);
	// 				alrm=0;
	// 			}else continue;
	// 		}else ERR("select");
	// 	}else {
	// 		msg=recv_datagram(sfd,&addr);
	// 		process_datagram(msg,addr,ot,sfd);
	// 	}
	// 	process_orders(ot,sfd);
	// }
	// sigprocmask (SIG_UNBLOCK, &mask, NULL);

}

int main(int argc, char** argv) {
	int sock ;
	if(argc!=4) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}
	sock=make_socket(atoi(argv[1]));
	if(sethandler(sig_alrm,SIGALRM)) ERR("Seting SIGALRM:");

	work(sock);
	if(TEMP_FAILURE_RETRY(close(sock))<0)ERR("close");
	return EXIT_SUCCESS;
}


