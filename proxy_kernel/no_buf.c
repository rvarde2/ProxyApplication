#include <stdio.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>

#define MAX_EVENTS 10
#define EPOLL_TIMEOUT_MILLIS 30000

#define PROXY_IP "127.0.0.1"
#define PROXY_PORT 1234
#define REMOTE_IP "127.0.0.1"
#define REMOTE_PORT 5678

unsigned long long upstream = 0;
unsigned long long downstream = 0;
clock_t start_time,difference;

void stats(){
    clock_t difference = clock() - start_time;
    double time_taken = ((double)difference)/CLOCKS_PER_SEC; // in seconds
    upstream = upstream / 1000000;
    downstream = downstream / 1000000;
    printf("UpStream: Data: %llu MB, Rate: %lf Gbps\n",upstream,(upstream*0.008)/time_taken);
    printf("DownStream: Data: %llu MB, Rate: %lf Gbps\n",downstream,(downstream*0.008)/time_taken);
}
int main(){
    int proxy_fd = 0, client_fd = 0, remote_fd = 0;
    struct sockaddr_in proxy_addr,client_addr,remote_addr;
    /*Create Proxy Socket*/
    proxy_fd = socket(AF_INET,SOCK_STREAM,0);
    if(proxy_fd < 0){
        perror("Failed to Create Socket for Proxy Server");
        exit(EXIT_FAILURE);
    }
    proxy_addr.sin_family=AF_INET;
    proxy_addr.sin_port=htons(PROXY_PORT);
    if (inet_pton(AF_INET, PROXY_IP, &(proxy_addr.sin_addr)) <= 0) {
        perror("Failed to convert IP address");
        exit(EXIT_FAILURE);
    }

    if(bind(proxy_fd,(const struct sockaddr*)&proxy_addr,sizeof(struct sockaddr_in))<0){
        perror("Failed to Bind to Proxy Server");
        exit(EXIT_FAILURE);
    }
    
    if(listen(proxy_fd,1) != 0){
        perror("Listen Failure");
        exit(EXIT_FAILURE);
    }
    printf("Waiting for the Client Connection...\n");
    
    /*Handle Client Connection to the Proxy Server*/
    socklen_t client_addr_size = sizeof(client_addr);
    client_fd = accept(proxy_fd,(struct sockaddr*)&client_addr,&client_addr_size);
    if(client_fd<0)
    {
        perror("Proxy Failed to Accept the Client Connection");
        exit(EXIT_FAILURE);
    }
    
    printf("Connection Accepted by the Proxy Server, fd:%d\n",client_fd);
    
    /*Connection to the Remote Server*/
    remote_fd = socket(AF_INET,SOCK_STREAM,0);
    if(remote_fd < 0){
        perror("Failed to Create Socket for Remote Server");
        exit(EXIT_FAILURE);
    }

    remote_addr.sin_family=AF_INET;
    remote_addr.sin_port=htons(REMOTE_PORT);
    if (inet_pton(AF_INET, REMOTE_IP, &(remote_addr.sin_addr)) <= 0) {
        perror("Failed to convert IP address");
        exit(EXIT_FAILURE);
    }

    if(connect(remote_fd,(const struct sockaddr*)&remote_addr,sizeof(struct sockaddr_in))<0){
        perror("Failed to Connect to the REmote Server");
        exit(EXIT_FAILURE);
    }
    printf("Connection to Remote Server Successful\n");

    /*Creating epoll fd*/
    int epoll_fd = epoll_create1(0);
    if(epoll_fd == -1){
        perror("Failed to create epoll file descriptor\n");
        exit(EXIT_FAILURE);
    }
    /*Registering socket fd to epoll*/
    struct epoll_event client_event,remote_event; 
    client_event.events = EPOLLIN;
    client_event.data.fd = client_fd;
    remote_event.events = EPOLLIN;
    remote_event.data.fd = remote_fd;

   
    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_event)!=0){
        perror("Failed to Register Client Socket FD to Epoll");
        close(client_fd);
        close(remote_fd);
        close(epoll_fd);
        close(proxy_fd);
        exit(EXIT_FAILURE);
    }
    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, remote_fd, &remote_event)!=0){
        perror("Failed to Register Remote Socket FD to Epoll");
        close(client_fd);
        close(remote_fd);
        close(epoll_fd);
        close(proxy_fd);
        exit(EXIT_FAILURE);
    }

    printf("Registered Both client_fd and remote_fd to Epoll Successfully\n");
    
    start_time = clock();

    /*Poll For Packets*/
    int event_count= 0;
    struct epoll_event events[MAX_EVENTS];
    int space_in_cb;
    int recv_count = 0,sent_count = 0;
    while(1)
    {
        event_count = epoll_wait(epoll_fd,events,MAX_EVENTS,EPOLL_TIMEOUT_MILLIS);
        for(int i=0;i<event_count;i++)
        {
            if(events[i].data.fd==client_fd)
            {
                if(events[i].events & EPOLLIN)
                {
                       char *buffer = malloc(4096);
                       recv_count = read(client_fd,buffer,4096);
                       if(recv_count>0){
                            sent_count = write(remote_fd,buffer,recv_count);
                            if(sent_count<0){
                                perror("Remote Send Failure");
                                close(client_fd);
                                close(remote_fd);
                                close(proxy_fd);
                                close(epoll_fd);
                                free(buffer);
                                exit(EXIT_FAILURE);
                            }
                            else{
                                upstream += sent_count;
                            } 
                       }
                       else if(recv_count == 0){
                            printf("Client Terminated the Connection\n");
                            close(client_fd);
                            close(remote_fd);
                            close(proxy_fd);
                            close(epoll_fd);
                            stats();
                            free(buffer);
                            return 0;
                       }
                       else{
                            perror("Client Socket Error");
                            close(client_fd);
                            close(remote_fd);
                            close(proxy_fd);
                            close(epoll_fd);
                            free(buffer);
                            exit(EXIT_FAILURE);
                       }
                       free(buffer);
                }
            }
            else if(events[i].data.fd==remote_fd)
            {
                if(events[i].events & EPOLLIN)
                {
                       char *buffer = malloc(4096);
                       recv_count = read(remote_fd,buffer,4096);
                       if(recv_count>0){
                            sent_count = write(client_fd,buffer,recv_count);
                            if(sent_count<0){
                                perror("Client Send Failure");
                                close(client_fd);
                                close(remote_fd);
                                close(proxy_fd);
                                close(epoll_fd);
                                free(buffer);
                                exit(EXIT_FAILURE);
                            }
                            else{
                                downstream += sent_count;
                            }

                       }
                       else if(recv_count == 0){
                            printf("Remote Endpoint Terminated the Connection\n");
                            close(client_fd);
                            close(remote_fd);
                            close(proxy_fd);
                            close(epoll_fd);
                            stats();
                            free(buffer);
                            return 0;
                       }
                       else{
                            perror("Server 2 Socket Error");
                            close(client_fd);
                            close(remote_fd);
                            close(proxy_fd);
                            close(epoll_fd);
                            free(buffer);
                            exit(EXIT_FAILURE);
                       }
                       free(buffer);
                }
            }
        }
    }
    /*Closing Epoll FD*/
    if(close(epoll_fd)){
        perror("Failed to Close Epoll File Descriptor\n");
        exit(EXIT_FAILURE);
    }
    /*Making sure all FDs are closed*/
    close(client_fd);
    close(remote_fd);
    close(proxy_fd);
    stats();
    return 0;
}
