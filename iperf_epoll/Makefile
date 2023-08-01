all: iperf

iperf: main_server.c main_client.c
	gcc -Wall -Werror -o server_epoll main_server.c
	gcc -Wall -Werror -o client_epoll main_client.c
clean:
	rm -f server_epoll client_epoll 

