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
#define ERR(source) (perror(source),\
		     fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
		     exit(EXIT_FAILURE))
#define MAXCONNECTED 10
#define MAXTRY 5
#define MINTIME (-MAXTRY*2-1)

volatile sig_atomic_t alrm=0;

struct message {
      int type;
      char text[20];
};

struct message2 {
      int type;
      char *text;


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

// short recv_datagram(int sock, char **buffer){
// 	char buf[500];
// 	int len;
//       struct sockaddr_in addr;
//       socklen_t addrlen = sizeof(addr);            /* length of addresses */
// 	if((len = TEMP_FAILURE_RETRY(recvfrom(sock,buf,500,0,(struct sockaddr*) &addr,&addrlen)))<1)
// 		ERR("recvfrom");

//        buf[len] = '\0';
//        fprintf(stderr,"Dlugosc:%d , wiadomosc: %s , od: %s:%d\n",len,buf,inet_ntoa(addr.sin_addr) , addr.sin_port);
//        *buffer = buf;
// 	return len;
// }

// short recv_datagram(int sock, char **buffer){
//       struct message wiadomosc;
//       int len;
//       struct sockaddr_in addr;
//       socklen_t addrlen = sizeof(addr);            /* length of addresses */
//   if((len = TEMP_FAILURE_RETRY(recvfrom(sock,&wiadomosc,sizeof(struct message),0,(struct sockaddr*) &addr,&addrlen)))<1)
//     ERR("recvfrom");

//        //buf[len] = '\0';
//        fprintf(stderr,"Dlugosc:%d , wiadomosc: %s , od: %s:%d\n",len,wiadomosc.text,inet_ntoa(addr.sin_addr) , addr.sin_port);
//        //*buffer = buf;
//   return len;
// }

short recv_datagram(int sock, char **buffer){
      struct message2 wiadomosc;
      char buf[500];
      int len;
      struct sockaddr_in addr;
      socklen_t addrlen = sizeof(addr);            /* length of addresses */
  if((len = TEMP_FAILURE_RETRY(recvfrom(sock,buf,500,0,(struct sockaddr*) &addr,&addrlen)))<1)
    ERR("recvfrom");

       buf[len] = '\0';
       fprintf(stderr,"Dlugosc:%d , wiadomosc: %s , od: %s:%d\n",len,buf,inet_ntoa(addr.sin_addr) , addr.sin_port);
       //*buffer = buf;
  return len;
}

int send_datagram(int sock,struct sockaddr_in addr,short msg){
	int status;
	short buf=htons(msg);
	status=TEMP_FAILURE_RETRY(sendto(sock,&buf,sizeof(short),0,(struct sockaddr*)&addr,sizeof(addr)));
	if(status<0&&errno!=EPIPE&&errno!=ECONNRESET) ERR("sendto");
	return status;
}

void process_datagram(short msg,struct sockaddr_in addr, struct order ot[], int sock){
	int i ;
	int free = -1;
	int pos = -1;
	for(i=0;i<MAXCONNECTED;i++){
		if(ot[i].time<=MINTIME) free = i;
		else if(0==memcmp(&addr,&ot[i].addr,sizeof(struct sockaddr_in))) pos=i;
	}
	if(pos!=-1){
		if(msg>0) {
			ot[pos].time=msg;
			if(send_datagram(sock,ot[pos].addr,msg)<0) ot[pos].time=MINTIME;
		}
		else ot[pos].time=MINTIME;
	}else if(msg>0){
		if(free!=-1){
			ot[free].time=msg;
			ot[free].addr=addr;
			if(send_datagram(sock,ot[free].addr,msg)<0) ot[free].time=MINTIME;
		}else send_datagram(sock,addr,-1);
	}
}

void process_orders(struct order ot[],int sock){
	int i;
	for(i=0;i<MAXCONNECTED;i++)
		if(ot[i].time<=0&&ot[i].time>MINTIME&&0==ot[i].time%2)
				send_datagram(sock,ot[i].addr,0);
}

void work(int sfd) {

	fd_set base_rfds, rfds ;
      int fd_count;
      int msg_len;
      FD_ZERO(&base_rfds);
      FD_SET(sfd, &base_rfds);

      char *msg_buf;

      for (;;) {
          rfds = base_rfds;
          fd_count = pselect(sfd+1,&rfds, NULL,NULL,NULL,NULL);

          if (fd_count<0) {
            if (errno==EINTR) {

            } else ERR("SELECT");
          } else {
              msg_len = recv_datagram(sfd,&msg_buf);
              //fprintf(stderr, "Message received: %s\n", msg_buf);
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


