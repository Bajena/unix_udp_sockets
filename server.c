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
#include <time.h>
#include "common.h"

#define ERR(source) (perror(source),\
				 fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
				 exit(EXIT_FAILURE))
#define BUFFER_SIZE 512
#define MAX_ATTEMPTS 3
#define CONFIRM_TIME 500000

volatile sig_atomic_t last_signal=0;


struct player {
	int points;
	int registered;
	int sent_answer;
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
	last_signal=i;
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

struct message* recv_datagram(int sock){
	char buf[BUFFER_SIZE];
	int len;
	struct sockaddr_in *addr;
	struct message *msg;
	socklen_t addrlen = sizeof(struct sockaddr_in);

	addr = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
	if((len = TEMP_FAILURE_RETRY(recvfrom(sock,buf,BUFFER_SIZE,0,(struct sockaddr*) addr,&addrlen)))<1) ERR("recvfrom");
	buf[len] = '\0';
	msg =  create_message(buf[0], buf+1, addr);
	if (msg!=NULL) {
		fprintf(stderr,"Dlugosc:%d , wiadomosc: %s , od: %s:%d\n",len,msg->text,inet_ntoa(msg->addr->sin_addr) , msg->addr->sin_port);
	}
	return msg;
}

int send_datagram(int sock,struct sockaddr_in *addr, char type,char *text){
	int status;

	char msg[BUFFER_SIZE];
	sprintf(msg,"%c%s",type,text);

	status=TEMP_FAILURE_RETRY(sendto(sock,msg,strlen(msg),0,(struct sockaddr*)addr,sizeof(*addr)));
	if(status<0&&errno!=EPIPE&&errno!=ECONNRESET) ERR("sendto");
	return status;
}
int send_and_confirm(int sock, struct sockaddr_in *addr, char type, char* text) {
		int attempts = 0;
		char recv_buffer[BUFFER_SIZE];
		struct itimerval tv;
		memset(&tv, 0, sizeof(struct itimerval));
		tv.it_value.tv_usec=CONFIRM_TIME;

		for (attempts = 0; attempts< MAX_ATTEMPTS; attempts++) {
					last_signal = 0;
					setitimer(ITIMER_REAL,&tv,NULL);
					if(send_datagram(sock,addr,type, text)<0){
								fprintf(stderr,"Send error\n");
					}
					if (recv(sock,recv_buffer,BUFFER_SIZE,0)<0){
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



void register_player(struct player players[], int id, struct message* msg) {
	players[id].registered = 1;
	players[id].addr = *(msg->addr);
}

int get_player_id(struct player players[], struct sockaddr_in *addr) {
	int i;
	for (i = 0;i<2;i++) {
		if (players[i].addr.sin_port==addr->sin_port) {
			return i;
		}
	}
	return -1;
}

void process_register_datagram(struct message* msg, int sock, struct player players[]){
	if (get_player_id(players,msg->addr)!=-1) {
		fprintf(stderr, "Gracz juz sie zarejestrowal\n");
		return;
	}
	if (players[0].registered && players[1].registered) {
		 fprintf(stderr, "Zarejestrowano juz dwoch graczy!\n");
		 return;
	}
	int i;
	for (i = 0;i<2;i++) {
		if (!players[i].registered) {
			register_player(players,i,msg);
			fprintf(stderr, "Zarejestrowano gracza nr.%d\n",i+1);
			if (send_datagram(sock,&players[i].addr, '0' ,"Zarejestrowano")<0) ERR("Send");
			return;
		}
	}
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
}

void wait_for_players(int sfd, struct player players[]) {
	fd_set base_rfds, rfds ;
	int fd_count;
	FD_ZERO(&base_rfds);
	FD_SET(sfd, &base_rfds);

	struct message *in_msg;

	for (;;) {
		rfds = base_rfds;
		fd_count = pselect(sfd+1,&rfds, NULL,NULL,NULL,NULL);
		if (fd_count<0) {
			if (errno!=EINTR)  ERR("SELECT");
		} else {
			in_msg = recv_datagram(sfd);
			if (in_msg->type=='0') {
				fprintf(stderr,"Rejestracja:%s\n",in_msg->text);
				process_register_datagram(in_msg,sfd,players);
			}
			destroy_message(in_msg);

			if (players[0].registered && players[1].registered) {
				return;
			}
		}
	}
}

int game_result(struct player players[], int points_to_win) {
	 int i;
	 for (i = 0;i<2;i++) {
			if (players[i].points == points_to_win)
				return i;
	 }
	 return -1;
}

void prepare_task(char *task, int task_length) {
	int i;
	int digit;
	for (i = 0;i<task_length;i++) {
		digit = rand()%10;
		task[i] = (char)(((int)'0')+digit);
	}
}

void play_round(int sfd, int task_length, struct player players[]) {
	int i;
	char task[task_length];
	prepare_task(task, task_length);
	struct message *in_msg;
	int player_id = -1;

	fprintf(stderr, "Task: %s\n",task);
	for (i = 0;i<2;i++) {
		players[i].sent_answer = 0;
		send_datagram(sfd, &players[i].addr, '1', task);
	}
	for (i = 0;i<2;) {
		in_msg = recv_datagram(sfd);
		player_id = get_player_id(players,in_msg->addr);
		if (players[player_id].sent_answer==1) {
			destroy_message(in_msg);
			continue;
		}
		fprintf(stderr,"Gracz %d przyslal rozwiazanie: %s\n",  player_id + 1, in_msg->text);
		if (i==0) players[player_id].points++;
		destroy_message(in_msg);
		send_datagram(sfd, &players[i].addr, '2', "ok");
		players[player_id].sent_answer = 1;
		i++;
	}
}

void work(int sfd, int task_length, int points_to_win) {
		struct player players[2];
		int winner = -1;
		wait_for_players(sfd, players);
		while ((winner = game_result(players, points_to_win)) < 0) {
				play_round(sfd, task_length, players);
				fprintf(stderr, "Gracz 1: %d Gracz2: %d\n", players[0].points, players[1].points);
		}
		send_and_confirm(sfd, &players[winner].addr, '3', "WIN");
		send_and_confirm(sfd, &players[winner == 0 ? 1 : 0].addr, '3', "LOSE");
}

int main(int argc, char** argv) {
	int sock ;
			int task_length;
			int points_to_win;

	if(argc!=4) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	srand(time(NULL));
	task_length = atoi(argv[2]);
	points_to_win = atoi(argv[3]);
	sock=make_socket(atoi(argv[1]));
	if(sethandler(sig_alrm,SIGALRM)) ERR("Seting SIGALRM:");

	work(sock, task_length,points_to_win);
	if(TEMP_FAILURE_RETRY(close(sock))<0)ERR("close");
	return EXIT_SUCCESS;
}


