/* Client code */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <fcntl.h>
#include <errno.h>

#define TCP_PORT        45210
#define UDP_PORT        45211
#define SERVER_IP       "130.111.46.105"
#define CLIENT_IP       "172.29.34.6"

/* 
 * Message
 * Contains a chunk number and data payload
 */
typedef struct message {
    int chunk_num;
    int data;
} Message;

/*
 * Global variables 
 * 
 * done_recv        determines when client is done receiving a batch of messages
 * all_recv         determines when client has received all messages
 * ack_array        array for tracking missing chunks
 * tcp_thread       thread for running TCP protocol
 * lock             mutex used for thread synchronization
 */
bool done_recv, all_recv;
char ack_array[10000];
pthread_t tcp_thread;
pthread_mutex_t lock;

/*
 * Checks acknowledgement array for a "gap"
 * Returns true if there is a gap, else false.
 */
bool gapcheck(char arr[])
{
    for (int k=0; k < 10000; k++)
    {
        if(ack_array[k] == '0')
        {
            return true;
        }
    }
    return false;
}

/* TCP thread */
void *tcp_worker()
{
    int client_socket;
    struct sockaddr_in remote_addr;
    char buffer[128] = {0};

    /* Zeroing remote_addr struct */
    memset(&remote_addr, 0, sizeof(remote_addr));

    /* Construct remote_addr struct */
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(TCP_PORT);
    inet_pton(AF_INET, SERVER_IP, &(remote_addr.sin_addr));

    /* Create client socket */
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0)
    {
        perror("[TCP] Error creating socket");
        exit(EXIT_FAILURE);
    }
    printf("[TCP] Socket created successfully.\n");

    /* Connect to the server */
    if (connect(client_socket, (struct sockaddr *)&remote_addr, sizeof(struct sockaddr)) < 0)
    {
        perror("[TCP]Error connecting to server");
        exit(EXIT_FAILURE);
    }
    printf("[TCP] Client connected to server at port %d\n", TCP_PORT);

    while(!all_recv)
    {
        /* Yield while UDP thread is still receiving */
        while(!done_recv)
        {
            sched_yield();
        }
        
        pthread_mutex_lock(&lock);

        /* Wait for "all sent" message from server */
        recv(client_socket, buffer, sizeof(long) + 1, 0);
        printf("[TCP] Received \"all sent\" message from server.\n");
        
        /* Check status of missing chunks */
        if(gapcheck(ack_array) == false)
        {
            printf("[TCP] ALL MESSAGES RECEIVED\n");
            all_recv = true;
        }

        /* Send ACK message showing status of chunks */
        printf("[TCP] Sending ACK message to server\n");
        send(client_socket, ack_array, sizeof(ack_array), 0);
        done_recv = false;
        pthread_mutex_unlock(&lock);
    }
    shutdown(client_socket, SHUT_RD);
    return NULL;
}

/* UDP thread */
int main()
{
    int client_socket;
    struct sockaddr_in server_addr, client_addr;
    char buffer[128] = "Oi mate, it's time to send messages";
    socklen_t sock_len = sizeof(struct sockaddr_in);
    Message recv_message;
    done_recv = false, all_recv = false;

    int recv_array[10000];
    for (int i=0; i < sizeof(recv_array)/sizeof(recv_array[0]); i++)
    {
        recv_array[i] = -1;
        ack_array[i] = '0';
    }

    /* Initialize mutex */
    if (pthread_mutex_init(&lock, NULL) != 0) { 
        perror("Mutex init has failed"); 
        exit(EXIT_FAILURE);
    }

    /* Start TCP thread */
    pthread_create(&tcp_thread, NULL, tcp_worker, NULL);

    /* Zeroing sockaddr_in struct */
    memset(&server_addr, 0, sizeof(server_addr));
    memset(&client_addr, 0, sizeof(server_addr));

    /* Create socket */
    client_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (client_socket < 0)
    {
        perror("[UDP] Error creating socket");
        exit(EXIT_FAILURE);
    }
    printf("[UDP] Socket created succesfully\n");

    /* Construct client_addr struct */
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(UDP_PORT);
    inet_pton(AF_INET, CLIENT_IP, &(client_addr.sin_addr));

    /* Binds IP and port number for client */
    if (bind(client_socket, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0)
    {
        perror("[UDP] Bind failed. Error");
        exit(EXIT_FAILURE);
    }
    printf("[UDP] Bind completed.\n");
    
    /* Construct server_addr struct */
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(UDP_PORT);
    inet_pton(AF_INET, SERVER_IP, &(server_addr.sin_addr));

    /* Set receiving socket into non-blocking mode */
    int flags = fcntl(client_socket, F_GETFL, 0);
    fcntl(client_socket, F_SETFL, flags | O_NONBLOCK);
    perror("Set client socket to non-blocking mode");
    
    /* Send message to server */
    printf("[UDP] Sending message to server...\n");
    if (sendto(client_socket, buffer, strlen(buffer), 0, (struct sockaddr*)&server_addr, sock_len) < 0)
    {
        perror("sendto error");
        exit(EXIT_FAILURE);
    }  
    printf("[UDP] Message sent\n");

    int msg_count = 0;
    int check = 0;
    while(!all_recv)
    {
        /* Receive messages from server. */
        pthread_mutex_lock(&lock);
        while(msg_count < 3000)
        {
            check = recvfrom(client_socket, &recv_message, sizeof(recv_message), 0, (struct sockaddr *)&server_addr, &sock_len);
            if(check == -1 && errno == EAGAIN)
            {
                printf("GETTING A WOULD BLOCK\n");
                break;
            }
            msg_count += 1;
            recv_array[recv_message.chunk_num] = recv_message.data;
            ack_array[recv_message.chunk_num] = '1';

            printf("[UDP] Received message from server. Chunk number: %d Number: %d\n", recv_message.chunk_num, recv_message.data);
        }
        msg_count = 0;

        
        done_recv = true;
        pthread_mutex_unlock(&lock);
        while(done_recv)
        {
            sched_yield();
        }
    }
    pthread_join(tcp_thread, NULL);
    pthread_mutex_destroy(&lock);
    sleep(5);
    return 0;
}