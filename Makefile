all: client server
client: client.c
	gcc  -o client client.c common.c
server: server.c
	gcc  -o server server.c common.c
.PHONY: clean
clean:
	-rm client server