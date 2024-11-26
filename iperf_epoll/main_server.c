#include <stdio.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/timerfd.h>

#define MAX_EVENTS 10
#define EPOLL_TIMEOUT_MILLIS 30000
#define MAX_CLIENTS 25
#define BUFFER_SIZE 131072

#define SERVER_IP "10.10.2.2"
#define SERVER_PORT 5201

struct client_info{
    int fd;
    struct sockaddr_in addr;
    struct timespec start_time;
    unsigned long long bytes_received;
};

struct client_info client_history[MAX_CLIENTS];
int next_client_id = 0;


int get_index_from_fd(int fd){
    int index=0;
    for(;index<next_client_id;index++){
        if(client_history[index].fd==fd){
            break;
        }
    }
    if(index==next_client_id)
    {
        return -1;
    }
    return index;
}

void stats(int client_id){
    printf("Stats for connection from %s:%d\n",
            inet_ntoa(client_history[client_id].addr.sin_addr),
            ntohs(client_history[client_id].addr.sin_port));
    
    struct timespec now = {};
    //Get Current Epoch Time
    if (clock_gettime(CLOCK_REALTIME, &(now)) == -1){
        perror("Error Getting Current Time in Stats");
        return;
    }
    
    double time_taken = now.tv_sec - client_history[client_id].start_time.tv_sec;
    double mb = client_history[client_id].bytes_received/ (1024*1024*1024.0);
    printf("Data Transfered: %lf GB, Rate: %lf Gbps, Duration: %lf s\n",
            mb,(mb*8)/(time_taken),time_taken);
}

int main(){
    int server_fd = 0;
    struct sockaddr_in server_addr;

    /*Create Server Socket*/
    server_fd = socket(AF_INET,SOCK_STREAM,0);
    if(server_fd < 0){
        perror("Failed to Create Socket for Server");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family=AF_INET;
    server_addr.sin_port=htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &(server_addr.sin_addr)) <= 0) {
        perror("Failed to convert IP address");
        exit(EXIT_FAILURE);
    }

    if(bind(server_fd,(const struct sockaddr*)&server_addr,sizeof(struct sockaddr_in))<0){
        perror("Failed to Bind to Server");
        exit(EXIT_FAILURE);
    }
    
    if(listen(server_fd,1) != 0){
        perror("Listen Failure");
        exit(EXIT_FAILURE);
    }
    printf("Waiting for the Client Connection...\n");
    
    /*Creating epoll fd*/
    int epoll_fd = epoll_create1(0);
    if(epoll_fd == -1){
        perror("Failed to create epoll file descriptor\n");
        exit(EXIT_FAILURE);
    }
    /*Registering socket fd to epoll*/
    struct epoll_event server_event; 
    server_event.events = EPOLLIN;
    server_event.data.fd = server_fd;
   
    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &server_event)!=0){
        perror("Failed to Register Server Socket FD to Epoll");
        close(server_fd);
        close(epoll_fd);
        exit(EXIT_FAILURE);
    }

    printf("Registered server_fd to Epoll Successfully\n");

    /*Poll For Packets*/
    int event_count= 0;
    struct epoll_event events[MAX_EVENTS];
    while(1)
    {
        event_count = epoll_wait(epoll_fd,events,MAX_EVENTS,EPOLL_TIMEOUT_MILLIS);
        if(event_count == -1){
            perror("Error waiting for the evet");
        }
        for(int i=0;i<event_count;i++)
        {
            int fd = events[i].data.fd;
            if(fd==server_fd)
            {
                if((events[i].events & EPOLLHUP) || (events[i].events & EPOLLERR))
                {
                    close(server_fd);
                    close(epoll_fd);
                    perror("Error on Server FD");
                    exit(EXIT_FAILURE);
                }
                else{
                    struct sockaddr_in client_addr;
                    memset(&client_addr, 0, sizeof(client_addr));
                    socklen_t client_addr_size = sizeof(client_addr);
                    int client_fd = accept(server_fd,(struct sockaddr*)&client_addr,&client_addr_size);
                    if(client_fd<0)
                    {
                        perror("Server Failed to Accept the Client Connection");
                        continue;
                    } 
                    client_history[next_client_id].fd = client_fd;
                    memcpy(&client_history[next_client_id].addr, &client_addr, sizeof(client_addr));
                    printf("New connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                    
                    if (clock_gettime(CLOCK_REALTIME, &(client_history[next_client_id].start_time)) == -1){
                        perror("Error in getting Clock");
                        close(client_fd);
                        continue;
                    }

                    struct epoll_event client_event;
                    client_event.events = EPOLLIN | EPOLLET;
                    client_event.data.fd = client_fd;
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_event) == -1) {
                        perror("Error Adding Client Socket to Epoll");
                        close(client_fd);
                        continue;
                    }
                    client_history[next_client_id].bytes_received = 0; 
                    next_client_id++;
                }
            }
            else{
                int client_id = get_index_from_fd(fd);
                if(client_id==-1){
                    perror("Client Record Missing for this FD");
                    continue;
                }
                char buffer[BUFFER_SIZE];
                int bytes_received = recv(fd, buffer, BUFFER_SIZE, 0);
                if (bytes_received <= 0) {
                    if (bytes_received == 0) {
                        printf("Connection closed by client\n");
                    } else {
                        perror("Error receiving data for client\n");

                    }
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                    close(fd);
                    stats(client_id);
                    memset(&(client_history[client_id]), 0, sizeof(struct client_info));
                } else {
                    client_history[client_id].bytes_received += bytes_received;
                }
            }
        }
    }
    
    /*Making sure all FDs are closed*/
    close(server_fd);

    /*Closing Epoll FD*/
    close(epoll_fd);
    return 0;
}
