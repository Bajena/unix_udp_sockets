#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netdb.h>

#include "common.h"

#define ERR(source) (perror(source),\
				 fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
				 exit(EXIT_FAILURE))
#define MAX_ATTEMPTS 3
#define CONFIRM_TIME 500000
#define BUFFER_SIZE 512

volatile sig_atomic_t last_signal=0;

void sig_alrm(int i){
	last_signal=i;
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
	fprintf(stderr,"USAGE: %s ADDRESS PORT",name);
}

void sleep_nanoseconds(int seconds,unsigned long nanoseconds)
{
	struct timespec tt, t = {seconds,nanoseconds};
	for (tt = t;nanosleep(&tt,&tt);)
		if (errno!=EINTR) ERR("nanosleep:");
}

int send_datagram(int sock,struct sockaddr_in *addr, char type, char* text){
	int status;
	char wiad[BUFFER_SIZE];
	sprintf(wiad,"%c%s",type,text);
	status=TEMP_FAILURE_RETRY(sendto(sock,wiad,strlen(wiad),0, (struct sockaddr *)addr,sizeof(*addr)));
	if(status<0&&errno!=EPIPE&&errno!=ECONNRESET) ERR("sendto");
	return status;
}

int send_and_confirm(int sock, struct sockaddr_in *addr, char type, char* text) {
	int attempts = 0;
	int confirm_len = 0;
	char recv_buffer[BUFFER_SIZE];

	for (attempts = 0; attempts< MAX_ATTEMPTS; attempts++) {
		last_signal = 0;
		set_alarm(0,CONFIRM_TIME);
		if(send_datagram(sock,addr,type, text)<0){
					fprintf(stderr,"Send error\n");
		}
		if ((confirm_len = recv(sock,recv_buffer,BUFFER_SIZE,0))<0){
			if(EINTR!=errno)ERR("recv:");
			if(last_signal==SIGALRM) {
				fprintf(stderr, "Timeout, wysylam ponownie\n");
				continue;
			}
		}
		return 0;
	}
	return -1;
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
	return msg;
}

void register_player(int sock, struct sockaddr_in *addr) {
		if (send_and_confirm(sock, addr, '0', "Rejestracja") < 0) ERR("Send and confirm");
		fprintf(stderr, "Zarejestrowano pomyslnie\n" );
}

int get_digit_count(int number) {
		int length = 1;
		while ((number /= 10) >= 1)
				length++;
		return length;
}

int play(int sfd,struct sockaddr_in *addr) {
	struct message *in_msg;
	int result;
	char *result_string;
	for (;;) {
		in_msg = recv_datagram(sfd, addr);
		if (in_msg==NULL) continue;

		if (in_msg->type == '1') {
			sleep_nanoseconds(rand()%2, rand()%500000000 + 500000000);
			fprintf(stderr,"Zadanie: %s\n", in_msg->text);
			result = solve_task(in_msg->text);
			result_string = (char*)malloc(get_digit_count(result));
			sprintf(result_string,"%d",result);
			fprintf(stderr, "Rozwiazanie: %s\n", result_string);
			if (send_and_confirm(sfd,addr,'1',result_string) < 0) {
				fprintf(stderr, "Nie udalo sie wyslac rozwiazania. Wychodze...\n");
				ERR("send_and_confirm");
			}
		} else if (in_msg->type=='3') {
			if (strcmp(in_msg->text, "WIN")==0) return 1;
			else return -1;
		}
	}
}

void work(int sfd, struct sockaddr_in *addr) {
	int result;
	register_player(sfd,addr);
	result = play(sfd,addr);

	if (result==1) fprintf(stderr, "Wygrana\n");
	else fprintf(stderr, "Przegrana\n" );

	send_datagram(sfd,addr,'3', "Done");
}

int main(int argc, char** argv) {
	int sock;
	if (argc != 3) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}
			srand(time(NULL));
	sock=make_socket();
			 struct sockaddr_in addr;

			 addr = make_address(argv[1],(short)atoi(argv[2]));

			 if(sethandler(sig_alrm,SIGALRM)) ERR("Seting SIGALRM:");
			 work(sock, &addr);
	if(TEMP_FAILURE_RETRY(close(sock))<0)ERR("close");
	return EXIT_SUCCESS;
}


