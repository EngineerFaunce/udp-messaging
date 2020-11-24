/* Server code */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <stdbool.h>

#define TCP_PORT        45210
#define UDP_PORT        45211
#define SERVER_IP       "130.111.46.105"

/* 
 * Message
 * Contains a chunk number and data
 */
typedef struct message {
    int chunk_num;
    int data;
} Message;

/*
 * Global variables 
 * 
 * done_sending     determines when client is done receiving a batch of messages
 * all_sent         determines when client has received all messages
 * ack_array        array for tracking missing chunks
 * tcp_thread       thread for running TCP protocol
 * lock             mutex used for thread synchronization
 */
bool done_sending, all_sent;
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
    int server_tcp_socket, client_tcp_socket;
    struct sockaddr_in server_tcp_addr, client_tcp_addr;
    socklen_t sock_len = sizeof(struct sockaddr_in);
    char buffer[128] = "All messages sent.";

    /* Zeroing sockaddr_in structs */
    memset(&server_tcp_addr, 0, sizeof(server_tcp_addr));

    /* Create sockets */
    server_tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    client_tcp_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (server_tcp_socket < 0 || client_tcp_socket < 0)
    {
        perror("[TCP] Error creating sockets");
        exit(EXIT_FAILURE);
    }
    printf("[TCP] Sockets created successfully\n");

    /* Construct server_tcp_addr struct */
    server_tcp_addr.sin_family = AF_INET;
    server_tcp_addr.sin_port = htons(TCP_PORT);
    inet_pton(AF_INET, SERVER_IP, &(server_tcp_addr.sin_addr));

    /* TCP Binding */
    if ((bind(server_tcp_socket, (struct sockaddr *)&server_tcp_addr, sizeof(server_tcp_addr))) < 0)
    {
        perror("[TCP] Bind failed. Error");
        exit(EXIT_FAILURE);
    }
    printf("[TCP] Bind completed.\n");

    /* TCP server socket enters listening mode */
    listen(server_tcp_socket, 1);
    printf("[TCP] Server listening on port %d\n", TCP_PORT);
    printf("[TCP] Waiting for incoming connections...\n");

    /* Accepting incoming peers */
    client_tcp_socket = accept(server_tcp_socket, (struct sockaddr *)&client_tcp_addr, (socklen_t*)&sock_len);
    if (client_tcp_socket < 0)
    {
        perror("[TCP] Accept failed.");
        exit(EXIT_FAILURE);
    }
    printf("[TCP] Connection accepted\n");

    while(!all_sent)
    {
        /* Yield the processor until UDP thread finishes sending messages */
        while(!done_sending)
        {
            sched_yield();
        }

        pthread_mutex_lock(&lock);

        /* Inform client TCP thread that all the messages have been sent */
        printf("[TCP] Informing client that all messages have been sent.\n");
        send(client_tcp_socket, buffer, sizeof(buffer), 0);

        /* Receive ACK message from client */
        recv(client_tcp_socket, ack_array, sizeof(ack_array), MSG_WAITALL);
        printf("[TCP] Received ACK message from client.\n");

        if(gapcheck(ack_array) == false)
        {
            printf("\n[TCP] ALL MESSAGES HAVE BEEN RECEIVED BY CLIENT\n");
            all_sent = true;
        }

        /* TCP thread yields and UDP thread begins resending missing chunks */
        done_sending = false;
        pthread_mutex_unlock(&lock);
    }

    shutdown(client_tcp_socket, SHUT_RDWR);
    return NULL;
}

/* UDP thread */
int main()
{
    int server_udp_socket, recv_len = 0;
    struct sockaddr_in server_addr, client_addr;
    socklen_t sock_len = sizeof(struct sockaddr_in);
    char buffer[1024];
    done_sending = false;
    Message data_message;

    int send_array[10000];
    for (int i=0; i < sizeof(send_array)/sizeof(send_array[0]); i++)
    {
        send_array[i] = i;
        ack_array[i] = '0';
    }

    /* Initialize mutex */
    if (pthread_mutex_init(&lock, NULL) != 0) { 
        perror("Mutex init has failed"); 
        exit(EXIT_FAILURE);
    } 

    /* Zeroing sockaddr_in structs */
    memset(&server_addr, 0, sizeof(server_addr));
    memset(&client_addr, 0, sizeof(client_addr));

    /* Create socket */
    server_udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_udp_socket < 0)
    {
        perror("Error creating sockets");
        exit(EXIT_FAILURE);
    }
    printf("Sockets created successfully\n");

    /* Construct server_addr struct */
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(UDP_PORT);
    inet_pton(AF_INET, SERVER_IP, &(server_addr.sin_addr));

    /* UDP Binding */
    if (bind(server_udp_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("[UDP] Bind failed. Error");
        exit(EXIT_FAILURE);
    }
    printf("[UDP] Bind completed.\n");

    /* Start TCP thread */
    pthread_create(&tcp_thread, NULL, tcp_worker, NULL);

    /* Receive incoming message from client */
    printf("[UDP] Waiting for message from client...\n");
    recv_len = recvfrom(server_udp_socket, buffer, 1024, 0, (struct sockaddr*)&client_addr, &sock_len);
    printf("[UDP] Message received from client: %s\n", buffer);

    while(!all_sent)
    {
        /* Begin sending messages */
        printf("[UDP] Server beginning transmission...\n");
        pthread_mutex_lock(&lock);
        for (int j=0; j < 10000; j++)
        {
            if(ack_array[j] == '0')
            {
                data_message.chunk_num = j;
                data_message.data = send_array[j];
                sendto(server_udp_socket, &data_message, sizeof(data_message), 0, (struct sockaddr*)&client_addr, sock_len);
                //printf("[UDP] Sent the following chunk number: %d data: %d\n", data_message.chunk_num, data_message.data);
            }
        }

        /* UDP thread has finished sending messages, now yields */
        printf("[UDP] Server has completed sending messages.\n");
        
        done_sending = true;
        pthread_mutex_unlock(&lock);

        while(done_sending)
        {
            sched_yield();
        }
    }
    pthread_join(tcp_thread, NULL);
    pthread_mutex_destroy(&lock);
    
    return 0;
}