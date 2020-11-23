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
#define CLIENT_IP       "172.29.36.184"

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
bool done_recv;
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
}

/* Determine the "gaps" in message stream from server and adds them to */
void gapcheck(char arr[])
{
    for (int j=0; j < 10000; j++)
    {
        if (arr[j] == '0')
        {
            //printf("Gap found at index %d\n", j);
            addnack(chunk_list, j);
        }
    }
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

    while(1)
    {
        /* Yield while UDP thread is still receiving */
        while(!done_recv)
        {
            sched_yield();
        }
        
        pthread_mutex_lock(&lock);

        /* Wait for "all sent" message from server */
        int recv_size = recv(client_socket, buffer, sizeof(long) + 1, 0);
        printf("[TCP] Received \"all sent\" message from server.\n");

        /* Notify server how many chunks it will need to send */
        int num_chunks = listlength(chunk_list);
        sprintf(buffer, "%d", num_chunks);
        printf("[TCP] Notifying server of amount of chunks missing: %d\n", num_chunks);
        send(client_socket, buffer, sizeof(buffer), 0);
        /*
         * Send linked list containing status of missing chunks.
         * Note: can't just send pointers over a network and expect
         * them to be valid on a different machine. This will result
         * in a seg fault. Instead, we have to send each data payload
         * for the server to reconstruct the linked list.
         */
        printf("[TCP] Sending list of missing chunks to server.\n");
        struct nack_message *current = chunk_list;
        while(current != NULL)
        {
            send(client_socket, current->data, sizeof(current->data), 0);
            current = current->next_nack;
        }
        printf("[TCP] Finished sending linked list.\n");

        /* Clear the linked list to avoid headaches later */
        clearlist(&chunk_list);
        chunk_list = (struct nack_message*) malloc(sizeof(struct nack_message));
        chunk_list->next_nack = NULL;

        done_recv = false;
        pthread_mutex_unlock(&lock);
    }

    //close(client_socket);
}

/* UDP thread */
int main()
{
    int client_socket;
    struct sockaddr_in server_addr, client_addr;
    char buffer[128] = "Oi mate, it's time to send messages";
    socklen_t sock_len = sizeof(struct sockaddr_in);
    struct message recv_message;

    int recv_array[10000];
    char ack_array[10000];
    for (int i=0; i < sizeof(recv_array)/sizeof(recv_array[0]); i++)
    {
        recv_array[i] = -1;
        ack_array[i] = '0';
    }

    /* Initialize linked list */
    chunk_list = (struct nack_message*) malloc(sizeof(struct nack_message));
    chunk_list->next_nack = NULL;

    /* Initialize mutex */
    if (pthread_mutex_init(&lock, NULL) != 0) { 
        perror("Mutex init has failed"); 
        exit(EXIT_FAILURE);
    }

    /* Start TCP thread */
    pthread_mutex_lock(&lock);
    done_recv = false;
    pthread_mutex_unlock(&lock);
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
    while(1)
    {
        /* 
         * Receive messages from server.
         * Will break out of loop if there is no data available.
         */
        pthread_mutex_lock(&lock);
        while(msg_count < 100)
        {
            check = recvfrom(client_socket, &recv_message, sizeof(recv_message), 0, (struct sockaddr *)&server_addr, &sock_len);
            //printf("%d ", check);
            if(check == -1)
            {
                if(errno == EAGAIN)
                {
                    printf("GETTING A WOULD BLOCK\n");
                }
                break;
            }
            msg_count += 1;
            recv_array[recv_message.chunk_num] = recv_message.data;
            ack_array[recv_message.chunk_num] = '1';

            printf("[UDP] Received message from server. Chunk number: %d Number: %d\n", recv_message.chunk_num, recv_message.data);
        }
        msg_count = 0;

        /* Check for gaps in message stream */
        gapcheck(ack_array);
        done_recv = true;
        pthread_mutex_unlock(&lock);
        while(done_recv)
        {
            sched_yield();
        }
    }
    pthread_join(tcp_thread, NULL);
    pthread_mutex_destroy(&lock);
    close(client_socket);

    return 0;
}