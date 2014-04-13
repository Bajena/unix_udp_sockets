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
// struct order {
// 	short time ;
// 	struct sockaddr_in addr;
// 	int status; /*0 not confirmed, 1 confirmed, 2 timeouted/unavailable*/
// };

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

/*
 * Datagramy sa wysylane i odbierane atomowo do wymiaru MTU
 */
// short recv_datagram(int sock,struct sockaddr_in *addr){
// 	short buf;
// 	socklen_t len;
// 	if(TEMP_FAILURE_RETRY(recvfrom(sock,&buf,sizeof(short),0,(struct sockaddr*)addr,&len))<sizeof(short))
// 		ERR("recvfrom");
// 	return ntohs(buf);
// }

// int send_datagram(int sock,struct sockaddr_in addr,short msg){
// 	int status;
// 	short buf=htons(msg);
// 	status=TEMP_FAILURE_RETRY(sendto(sock,&buf,sizeof(short),0,(struct sockaddr*)&addr,sizeof(addr)));
// 	if(status<0&&errno!=EPIPE&&errno!=ECONNRESET) ERR("sendto");
// 	return status;
// }

// int send_datagram(int sock,struct sockaddr_in *addr,char *msg){
//   int status;
//   fprintf(stderr, "Proba wyslania: %s\n",msg);
//   status=TEMP_FAILURE_RETRY(sendto(sock,msg,strlen(msg),0, (struct sockaddr *)addr,sizeof(*addr)));
//   if(status<0&&errno!=EPIPE&&errno!=ECONNRESET) ERR("sendto");
//   return status;
// }

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

// int send_datagram(int sock,struct sockaddr_in *addr,char *msg, int length){
//   int status;
//   struct message *wiadomosc;
//   wiadomosc =  (struct message*)malloc(sizeof(struct message));
//   strcpy(wiadomosc->text,"abc");
//   //wiadomosc.text = "abc";
//   wiadomosc->type = 1;
//   fprintf(stderr, "Proba wyslania: %s\n",wiadomosc->text);
//   status=TEMP_FAILURE_RETRY(sendto(sock,(void*)wiadomosc,sizeof(struct message),0, (struct sockaddr *)addr,sizeof(*addr)));
//   if(status<0&&errno!=EPIPE&&errno!=ECONNRESET) ERR("sendto");

//   free(wiadomosc);
//   return status;
// }


// int send_datagram(int sock,struct sockaddr_in *addr,char *msg, int length){
//   int status;
//   struct message2 *wiadomosc;
//   wiadomosc =  (struct message2*)malloc(sizeof(struct message2));
//   wiadomosc->text =  (char*)malloc(strlen("abc"));
//   strcpy(wiadomosc->text,"abc");
//   wiadomosc->type = 1;

//   char *wiad = (char*)malloc(sizeof(wiadomosc->type) + strlen(wiadomosc->text));

//   strcpy(wiad,wiadomosc->text);
//   memcpy(wiad+strlen(wiadomosc->text), &wiadomosc->type, sizeof(wiadomosc->type));
//   fprintf(stderr, "Proba wyslania: %s\n",wiad);
//   status=TEMP_FAILURE_RETRY(sendto(sock,(void*)wiad,strlen(wiad),0, (struct sockaddr *)addr,sizeof(*addr)));
//   if(status<0&&errno!=EPIPE&&errno!=ECONNRESET) ERR("sendto");

//   free(wiadomosc);
//   free(wiad);
//   return status;
// }

// int send_orders(int sfd,struct order ot[],int otc) {
// 	int i;
// 	int errors=0;
// 	for(i=0;i<otc;i++)
// 		if(!ot[i].status)
// 			if(send_datagram(sfd,ot[i].addr,ot[i].time)<0){
// 				ot[i].status=2;
// 				errors++;
// 			}
// 	return errors;
// }

// short select_recv(int sfd, struct timespec tv,struct sockaddr_in *addr){
// 	fd_set rfds ;
// 	short time ;
// 	int ile;
// 	FD_ZERO(&rfds);
// 	FD_SET(sfd, &rfds);
// 	ile=TEMP_FAILURE_RETRY(pselect(sfd+1,&rfds,NULL,NULL,&tv,NULL));
// 	if(ile<0)ERR("pselect");
// 	if(ile>0) time=recv_datagram(sfd,addr);
// 	else time = -1;
// 	return time;
// }

// void recv_conf(int sfd,struct order ot[],int otc, int *errors, int *confirmed) {
// 	struct sockaddr_in addr;
// 	struct timespec tv={2,0};
// 	short time ;
// 	int i;
// 	do{
// 		time=select_recv(sfd,tv,&addr);
// 		if(time>=0){
// 			for(i=0;i<otc;i++)
// 				if(0==memcmp(&addr,&ot[i].addr,sizeof(struct sockaddr_in))){
// 					if(time!=ot[i].time){
// 						if(1==ot[i].status)(*confirmed)--;
// 						if(2!=ot[i].status)(*errors)++;
// 						ot[i].status=2;
// 					}else {
// 						if(1!=ot[i].status)(*confirmed)++;
// 						ot[i].status=1;
// 					}
// 					break;
// 				}
// 		}
// 	}while(time>=0&&(otc-*confirmed-*errors>0));
// }

// void recv_ends(int sfd,struct order ot[],int otc, int *confirmed) {
// 	struct timespec tv={1,0};
// 	struct sockaddr_in addr;
// 	short time ;
// 	int i;
// 	do{
// 		time=select_recv(sfd,tv,&addr);
// 		if(time>=0){
// 			for(i=0;i<otc;i++)
// 				if(0==memcmp(&addr,&ot[i].addr,sizeof(struct sockaddr_in))){
// 					if(!time){
// 						if(ot[i].status==1)(*confirmed)--;
// 						if(send_datagram(sfd,addr,0)<0) ERR("send");
// 						ot[i].status=2;
// 					}
// 					break;
// 				}
// 		}
// 	}while(time>=0);
// }


void work(int sfd, struct sockaddr_in *addr) {
  struct message *msg;
  msg = create_message('0', "Message",NULL);
  if(send_datagram(sfd,addr,msg)<0){
          fprintf(stderr,"Send error\n");
        }
  destroy_message(msg);

  msg = recv_datagram(sfd, addr);

  destroy_message(msg);
	// int count=0,confirmed=0;
	// int errors=0;

	// while(otc-confirmed-errors){
	// 	if(MAXTRY==count++){
	// 		fprintf(stderr,"%d serwerow nie odpowiada\n",otc-confirmed-errors);
	// 		break;
	// 	}
	// 	errors+=send_orders(sfd,ot,otc);
	// 	recv_conf(sfd,ot,otc,&errors,&confirmed);
	// }
	// while(confirmed>0){
	// 	recv_ends(sfd,ot,otc,&confirmed);
	// 	printf("*");
	// 	if(TEMP_FAILURE_RETRY(fflush(stdout))<0) ERR("fflush");
	// }

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
	// for(i=0;i<(argc-1)/3;i++){
	// 	ot[i].addr=make_address(argv[1+i*3],(short)atoi(argv[2+i*3]));
	// 	ot[i].time=atoi(argv[3+i*3]);
	// 	ot[i].status = 0;
	// }
	//work(sock,ot,i);

       work(sock, &addr);
	if(TEMP_FAILURE_RETRY(close(sock))<0)ERR("close");
	return EXIT_SUCCESS;
}


