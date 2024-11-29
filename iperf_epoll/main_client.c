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
#include <netinet/tcp.h>
#include <errno.h>
#include <time.h>

#define MAX_EVENTS 10
#define EPOLL_TIMEOUT_MILLIS 30000

//#define BUFFER_SIZE 2097152 //2MB
//#define BUFFER_SIZE 1048576 //1MB
//#define BUFFER_SIZE 524288    //512KB
//#define BUFFER_SIZE 131072    //128KB
//#define BUFFER_SIZE 65536    //64KB
#define BUFFER_SIZE 32768    //32KB
//#define BUFFER_SIZE 16384    //16KB 
//#define BUFFER_SIZE 8192    //8KB 

#define SERVER_IP "10.10.1.2"
#define SERVER_PORT 5201
#define CLIENT_IP "10.10.1.1"
#define CLIENT_PORT 6201
#define DURATION 10
#define FREQUENCY 10
#define MAX_SAMPLES DURATION * FREQUENCY

unsigned long long total_data_sent = 0;
int sample_counter = 0;

uint32_t previous_reordering = 0;
uint32_t previous_retransmits = 0;

void print_tcp_info(int sockfd) {
    struct tcp_info info;
    socklen_t info_len = sizeof(info);
    if (getsockopt(sockfd, IPPROTO_TCP, TCP_INFO, &info, &info_len) == 0) {
        printf("%.3f,%u,%u,%u,%u,%u,%u,%u\n",sample_counter/(FREQUENCY*1.0),
                                      info.tcpi_snd_cwnd, 
                                      info.tcpi_unacked,
                                      info.tcpi_reordering - previous_reordering, 
                                      info.tcpi_retransmits - previous_retransmits, 
                                      info.tcpi_lost,
                                      info.tcpi_rtt,
                                      info.tcpi_ca_state);

        previous_reordering = info.tcpi_reordering;
        previous_retransmits = info.tcpi_retransmits;  
    } else {
        perror("Failed to get TCP info");
    }
}

int main(){
    int client_fd = 0;
    struct sockaddr_in server_addr,client_addr;
    char buffer[BUFFER_SIZE];
    memset(buffer, 'A', BUFFER_SIZE);

    /*Create client Socket*/
    client_fd = socket(AF_INET,SOCK_STREAM,0);
    if(client_fd < 0){
        perror("Failed to Create Socket for Client");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family=AF_INET;
    server_addr.sin_port=htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &(server_addr.sin_addr)) <= 0) {
        perror("Failed to convert IP address");
        exit(EXIT_FAILURE);
    }
    
    client_addr.sin_family=AF_INET;
    client_addr.sin_port=htons(CLIENT_PORT);
    if (inet_pton(AF_INET, CLIENT_IP, &(client_addr.sin_addr)) <= 0) {
        perror("Failed to convert IP address");
        exit(EXIT_FAILURE);
    }


    /*Creating epoll fd*/
    int epoll_fd = epoll_create1(0);
    if(epoll_fd == -1){
        perror("Failed to create epoll file descriptor\n");
        exit(EXIT_FAILURE);
    }
    
    /*Registering socket fd to epoll*/
    struct epoll_event client_event; 
    client_event.events = EPOLLOUT;
    client_event.data.fd = client_fd;
   
    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_event)!=0){
        perror("Failed to Register Client Socket FD to Epoll");
        close(client_fd);
        close(epoll_fd);
        exit(EXIT_FAILURE);
    }
    
    int timer_fd = timerfd_create(CLOCK_REALTIME, 0);

    struct epoll_event timer_event = {};
    //Configuring Event for Fd created
    timer_event.events=EPOLLIN | EPOLLET; //Edge Trigger Mode
    timer_event.data.fd = timer_fd;

    //Registered timerfd to epfd for monitoring
    if(epoll_ctl(epoll_fd,EPOLL_CTL_ADD,timer_fd,&timer_event)==-1)
    {
        perror("Failed to Register TimerFD to Epoll");
	    close(client_fd);
	    close(timer_fd);
	    close(epoll_fd);
        exit(EXIT_FAILURE);
    }

    struct timespec now = {};
    //Get Current Epoch Time
    if (clock_gettime(CLOCK_REALTIME, &(now)) == -1){
        perror("Error in getting Clock");
        close(client_fd);
	    close(timer_fd);
	    close(epoll_fd);
        exit(EXIT_FAILURE);
    }

    struct itimerspec timer_expiry = {};
    //Configuring Duration of timer
    timer_expiry.it_value.tv_sec = 0;
    timer_expiry.it_value.tv_nsec = (1000000000.0/FREQUENCY);
    timer_expiry.it_interval.tv_sec = 0;
    timer_expiry.it_interval.tv_nsec = (1000000000.0/FREQUENCY);

    if (timerfd_settime(timer_fd, 0, &timer_expiry,NULL) == -1)
    {
        perror("Failed to Start the Timer");
        close(client_fd);
	    close(timer_fd);
	    close(epoll_fd);
        exit(EXIT_FAILURE);

    }
    //printf("Registered client_fd and timer_fd to Epoll Successfully\n");
    if(bind(client_fd,(const struct sockaddr*)&client_addr,sizeof(struct sockaddr_in))<0){
        perror("Failed to Bind to Client");
        exit(EXIT_FAILURE);
    }

    if(connect(client_fd,(const struct sockaddr*)&server_addr,sizeof(struct sockaddr_in))<0){
        perror("Failed to Connect to the Server");
        close(client_fd);
	    close(timer_fd);
	    close(epoll_fd);
        exit(EXIT_FAILURE);
    }
    printf("Time\tCwnd\tUnack\tReorder\tRetx\tLost\tRtt\tCA\n");

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
            if(fd==timer_fd)
            {
                uint64_t expirations;
                read(timer_fd, &expirations, sizeof(expirations)); // Read to re-arm the timer
                sample_counter++;
                if(sample_counter==MAX_SAMPLES){
                    print_tcp_info(client_fd);
                    close(client_fd);
                    close(timer_fd);
                    close(epoll_fd);
                    double gb = total_data_sent/(1024*1024*1024.0);
                    double rate = (gb*8)/DURATION;
                    printf("\nTotal Data Sent: %lf GB, Rate: %lf Gbps\n",gb,rate);
                    return 0;
                }
                else{
                    print_tcp_info(client_fd);
                }
                
            }
            else if(fd==client_fd)
            {
                if((events[i].events & EPOLLHUP) || (events[i].events & EPOLLERR))
                {
                    close(client_fd);
                    close(timer_fd);
                    close(epoll_fd);
                    perror("Error on Server FD");
                    exit(EXIT_FAILURE);
                }
                else{
                     int sent = send(client_fd, buffer, BUFFER_SIZE, 0);
                     if(sent<=0){
                        perror("Cannot Send any more Data to the Server");
                        break;
                     }
                     total_data_sent +=sent;
                }
            }
        }
    }
    /*Making sure all FDs are closed*/
    close(client_fd);
    close(timer_fd);
    /*Closing Epoll FD*/
    close(epoll_fd);
    return 0;
}
