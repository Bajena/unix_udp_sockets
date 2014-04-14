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
              process_datagram(in_msg,sfd, players);
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

void send_task(int sfd, int task_length, struct player players[]) {
  int i;
  char task[task_length];
  prepare_task(task, task_length);
  struct message *in_msg;

  fprintf(stderr, "Task: %s\n",task);
  for (i = 0;i<2;i++) {
    send_datagram(sfd, &players[i].addr, '1', task);
  }
  in_msg = recv_datagram(sfd);
  fprintf(stderr,"Gracz %d przyslal rozwiazanie: %s\n",  get_player_id(players,in_msg->addr), in_msg->text);
  destroy_message(in_msg);
}

void work(int sfd, int task_length, int points_to_win) {
    struct player players[2];

    wait_for_players(sfd, players);
    while (game_result(players, points_to_win) < 0) {
        send_task(sfd, task_length, players);
        break;
    }
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


