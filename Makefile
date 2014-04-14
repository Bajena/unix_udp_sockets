all: client server
client: client.c
	gcc -Wall -o client client.c common.c
server: server.c
	gcc -Wall  -o server server.c common.c
.PHONY: clean
clean:
	-rm client server