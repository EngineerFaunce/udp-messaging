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
 * Contains a chunk number and data number
 */
struct message {
    int chunk_num;
    int data;
};

/*
 * Negative Acknowledgement Message (NACK)
 * Represented as a singly linked list
 */
struct nack_message {
    int data;
    struct nack_message *next_nack;
};

/* Global variables */
bool done_sending;
struct nack_message *chunk_list;
pthread_t tcp_thread;
pthread_mutex_t lock;

/*
 * Function for creating and adding a new
 * node to the end of nack_message linked list.
 */
void addnack(struct nack_message *head, int data)
{
    //printf("Adding chunk %d to list\n", data);
    struct nack_message *current = head;
    while(current->next_nack != NULL)
    {
        current = current->next_nack;
    }
    current->next_nack = (struct nack_message*) malloc(sizeof(struct nack_message));
    current->next_nack->data = data;
    current->next_nack->next_nack = NULL;
}

/* Clears a linked list */
void clearlist(struct nack_message **head)
{
    struct nack_message *current = *head;
    struct nack_message *next;

    while(current != NULL)
    {
        next = current->next_nack;
        free(current);
        current = next;
    }

    *head = NULL;
    struct nack_message *temp = (struct nack_message*) malloc(sizeof(struct nack_message));
    temp->next_nack = NULL;
    head = temp;
}

/* Returns the amount of nodes in a linked list */
int listlength(struct nack_message *head)
{
    struct nack_message *current = head;
    int count = 0;
    while(current->next_nack != NULL)
    {
        count += 1;
        current = current->next_nack;
    }
    return count;
}

/* TCP thread */
void *tcp_worker(void *socketarg)
{
    //int *sock = (int*)socketarg;
    //int client_socket = *sock;
    int server_tcp_socket, client_tcp_socket;
    struct sockaddr_in server_tcp_addr, client_tcp_addr;
    socklen_t sock_len = sizeof(struct sockaddr_in);
    char buffer[128];
    char ack_message[128] = "All messages sent.";

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

    while(1)
    {
        /* Yield the processor until UDP thread finishes sending messages */
        while(!done_sending)
        {
            sched_yield();
        }

        pthread_mutex_lock(&lock);

        /* Notify client TCP thread that all the messages have been sent */
        printf("[TCP] Notifying client that all messages have been sent.\n");
        send(client_tcp_socket, ack_message, sizeof(ack_message), 0);

        /* Receive number of chunks about to be sent from client */
        recv(client_tcp_socket, buffer, sizeof(buffer), 0);
        int num_chunks = atoi(buffer);
        memset(buffer, 0, sizeof(buffer));

        /* Receive acknowledgement messages from client TCP thread and rebuild list */
        int data = 0;
        clearlist(&chunk_list);
        for(int i=0; i<num_chunks; i++)
        {
            recv(client_tcp_socket, data, sizeof(data), 0);
            addnack(chunk_list, data);
        }
        printf("[TCP] Received nack message from client. Amount of chunks missing: %d\n", listlength(chunk_list));

        /* TCP thread yields and UDP thread begins resending missing chunks */
        done_sending = false;
        pthread_mutex_unlock(&lock);
    }
    
}

int main()
{
    int server_udp_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t sock_len = sizeof(struct sockaddr_in);
    char buffer[1024];
    int recv_len = 0;
    done_sending = false;

    chunk_list = (struct nack_message*) malloc(sizeof(struct nack_message));
    chunk_list->next_nack = NULL;

    int send_array[10000];
    for (int i=0; i < sizeof(send_array)/sizeof(send_array[0]); i++)
        send_array[i] = i;

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

    /* Begin sending messages */
    printf("[UDP] Server beginning transmission...\n");
    struct message data_message;
    for (int j=0; j < 10000; j++)
    {
        data_message.chunk_num = j;
        data_message.data = send_array[j];
        sendto(server_udp_socket, &data_message, sizeof(data_message), 0, (struct sockaddr*)&client_addr, sock_len);
        //printf("[UDP] Sent the following chunk number: %d data: %d\n", data_message.chunk_num, data_message.data);
    }

    while(1)
    {
        /* UDP thread has finished sending messages, now yields */
        printf("[UDP] Server has completed sending messages.\n");
        pthread_mutex_lock(&lock);
        done_sending = true;
        pthread_mutex_unlock(&lock);

        while(done_sending)
        {
            sched_yield();
        }

        /* 
         * If list head is NULL, then there are no missing chunks so system may exit.
         * Else, resend missing chunks reported by client.
         */
        struct nack_message *current = chunk_list;
        if(current == NULL)
        {
            printf("List is empty!\n");
            break;
        }
        else
        {
            printf("[UDP] Server resending missing chunks...\n");
            while(current != NULL)
            {
                data_message.chunk_num = current->data;
                data_message.data = current->data;
                sendto(server_udp_socket, &data_message, sizeof(data_message), 0, (struct sockaddr*)&client_addr, sock_len);
                //printf("[UDP] Sent the following chunk number: %d data: %d\n", data_message.chunk_num, data_message.data);
                current = current->next_nack;
            }
        } 
    }
    pthread_join(tcp_thread, NULL);
    pthread_mutex_destroy(&lock);
    
    return 0;
}