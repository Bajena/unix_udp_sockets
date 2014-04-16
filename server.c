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
#define TASK_TIME 5
#define PLAYERS_COUNT 2

volatile sig_atomic_t last_signal=0;
volatile sig_atomic_t recv_timeout=0;
volatile sig_atomic_t task_timeout=0;

struct player {
	int points;
	int registered;
	int sent_answer;
	struct sockaddr_in addr;
};

void sig_alrm_recv(int i){
	last_signal=i;
	recv_timeout = 1;
}

void sig_alrm_task(int i) {
	last_signal=i;
	task_timeout = 1;
}

int make_socket(int port){
	struct sockaddr_in addr;
	int sock;
	int t = 1;
	sock = socket(PF_INET,SOCK_DGRAM,0);
	if(sock < 0) ERR("socket");
	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_port = htons (port);
	addr.sin_addr.s_addr = htonl (INADDR_ANY);
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,&t, sizeof(t))) ERR("setsockopt");
	if(bind(sock,(struct sockaddr*) &addr,sizeof(struct sockaddr_in)) < 0) ERR("bind");
	fprintf(stderr, "Serwer pracuje pod adresem:%s\n",inet_ntoa(addr.sin_addr));
	return sock;
}

void usage(char * name){
	fprintf(stderr,"USAGE: %s PORT LENGTH POINTS_TO_WIN \n",name);
}

struct message* recv_datagram(int sock){
	char buf[BUFFER_SIZE];
	int len;
	struct sockaddr_in *addr;
	struct message *msg;
	socklen_t addrlen = sizeof(struct sockaddr_in);

	addr = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
	if((len = recvfrom(sock,buf,BUFFER_SIZE,0,(struct sockaddr*) addr,&addrlen))<0) {
		if(EINTR!=errno)ERR("recvfrom");
	}
	buf[len] = '\0';
	msg =  create_message(buf[0], buf+1, addr);

	return msg;
}

int send_datagram(int sock,struct sockaddr_in *addr, char type,char *text){
	int status;
	char msg[BUFFER_SIZE];
      snprintf(msg,BUFFER_SIZE,"%c%s",type,text);

	status=TEMP_FAILURE_RETRY(sendto(sock,msg,strlen(msg),0,(struct sockaddr*)addr,sizeof(*addr)));
	if(status<0&&errno!=EPIPE&&errno!=ECONNRESET) ERR("sendto");
	return status;
}
int send_and_confirm(int sock, struct sockaddr_in *addr, char type, char* text) {
	int attempts = 0;
	char recv_buffer[BUFFER_SIZE];

	if(sethandler(sig_alrm_recv,SIGALRM)) ERR("Seting SIGALRM:");
	for (attempts = 0; attempts< MAX_ATTEMPTS; attempts++) {
		last_signal = 0;
		recv_timeout  = 0;
		set_alarm(0,CONFIRM_TIME);
		if(send_datagram(sock,addr,type, text)<0){
			fprintf(stderr,"Błąd wysyłania, probuję ponownie...\n");
			continue;
		}
		if (recv(sock,recv_buffer,BUFFER_SIZE,0)<0){
			if(EINTR!=errno)ERR("recv:");
			if(last_signal==SIGALRM && recv_timeout == 1) {
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
	for (i = 0;i<PLAYERS_COUNT;i++) {
		if (players[i].addr.sin_port==addr->sin_port) {
			return i;
		}
	}
	return -1;
}

int process_register_datagram(struct message* msg, int sock, struct player players[]){
	int i;
	if (get_player_id(players,msg->addr)!=-1) {
		fprintf(stderr, "Gracz juz sie zarejestrowal\n");
		return 0;
	}

	for (i = 0;i<PLAYERS_COUNT;i++) {
		if (players[i].registered==0) {
			register_player(players,i,msg);
			fprintf(stderr, "Zarejestrowano gracza nr.%d\n",i+1);
			if (send_datagram(sock,&players[i].addr, '0' ,"Zarejestrowano")<0) ERR("Send");
			return 1;
		}
	}

	fprintf(stderr, "Zarejestrowano juz wymagana liczbe graczy!\n");
	return 0;
}

void wait_for_players(int sfd, struct player players[]) {
	struct message *in_msg;
	int registered_players = 0;

	while(registered_players < PLAYERS_COUNT) {
		in_msg = recv_datagram(sfd);
		if (in_msg->type=='0') {
			fprintf(stderr,"Rejestracja:%s\n",in_msg->text);
			registered_players += process_register_datagram(in_msg,sfd,players);
		}
		destroy_message(in_msg);
	}
}

// -1 - gra nieskonczona | i - numer zwyciezcy
int game_result(struct player players[], int points_to_win) {
	 int i;
	 for (i = 0;i<PLAYERS_COUNT;i++) {
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
	task[i] = '\0';
}

//0 - nieprawidlowe | 1 - prawidlowe
int check_solution(int solution, int sent_solution) {
	return solution== sent_solution ? 1 : 0;
}

// -1 - nikt nie przyslal rozwiazania | 0 - przynajmniej jeden gracz przyslal rozwiazanie
int play_round(int sfd, int task_length, struct player players[]) {
	int i;
	char task[task_length+1];
	struct message *in_msg;
	int player_id = -1;
	int current_solution;
	int point_given = 0;

	prepare_task(task, task_length);
	current_solution = solve_task(task);
	fprintf(stderr, "Zadanie: %s , rozwiazanie:%d\n",task, current_solution);

	if(sethandler(sig_alrm_task,SIGALRM)) ERR("Seting SIGALRM:");
	task_timeout = 0;
	last_signal = 0;
	set_alarm(TASK_TIME,0);

	for (i = 0;i<PLAYERS_COUNT;i++) {
		players[i].sent_answer = 0;
		send_datagram(sfd, &players[i].addr, '1', task);
	}
	for (i = 0;i<PLAYERS_COUNT;) {
		in_msg = recv_datagram(sfd);

		if ((in_msg==NULL || in_msg->type!='1') && last_signal==SIGALRM && task_timeout==1) {
			if (i == 0) return -1;
			else return 0;
		}

		player_id = get_player_id(players,in_msg->addr);
		if (players[player_id].sent_answer==1) {
			destroy_message(in_msg);
			continue;
		}
		fprintf(stderr,"Gracz %d przyslal rozwiazanie: %s\n",  player_id + 1, in_msg->text);
		if (!point_given && check_solution(current_solution, atoi(in_msg->text)))  {
			players[player_id].points++;
			point_given = 1;
		}
		send_datagram(sfd, &players[player_id].addr, '2', "Odebrano rozwiazanie");
		destroy_message(in_msg);
		players[player_id].sent_answer = 1;
		i++;
	}

	cancel_alarm();
	return 0;
}
void send_game_result(int sfd, struct player players[], int winner) {
	int send_result;
	int i;

	for (i = 0;i<PLAYERS_COUNT;i++) {
		fprintf(stderr, "Wysylam podsumowanie do gracza: %d\n", i+1);
		if (i==winner) send_result = send_and_confirm(sfd, &players[i].addr, '3', "WIN");
		else send_result = send_and_confirm(sfd,&players[i].addr, '3', "LOSE");

		if (send_result < 0) fprintf(stderr, "Wysylanie nie powiodlo sie\n");
	}
}

void work(int sfd, int task_length, int points_to_win) {
	struct player players[PLAYERS_COUNT];
	int winner = -1;
	int i;

	for (i = 0;i<PLAYERS_COUNT;i++) {
		players[i].registered = 0;
		players[i].points = 0;
	}

	wait_for_players(sfd, players);
	while ((winner = game_result(players, points_to_win)) < 0) {
		if (play_round(sfd, task_length, players) < 0)  {
			fprintf(stderr,"Brak odpowiedzi - koniec gry.\n");
			ERR("play_round");
		}
		fprintf(stderr, "Wynik - Gracz 1: %d Gracz2: %d\n", players[0].points, players[1].points);
	}

	send_game_result(sfd,players,winner);
	fprintf(stderr, "***KONIEC\n");
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

	work(sock, task_length,points_to_win);

	if(TEMP_FAILURE_RETRY(close(sock))<0)ERR("close");
	return EXIT_SUCCESS;
}


