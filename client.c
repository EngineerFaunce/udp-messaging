/* Client code */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT_NUMBER     45210
#define SERVER_IP       "1.1.1.1"
#define CLIENT_IP       "2.2.2.2"

/* Message struct */
struct Message {
    int chunk_num;
    int number;
};

int main(int argc, char **argv)
{
    int recv_array[10000];
    char ack_array[10000];
    for (int i=0; i < sizeof(recv_array)/sizeof(recv_array[0]); i++)
    {
        recv_array[i] = -1;
        ack_array[i] = '0';
    }

    int client_socket;
    struct sockaddr_in server_addr, client_addr;
    char buffer[128] = "Oi mate, it's time to send messages";
    socklen_t sock_len = sizeof(struct sockaddr_in);
    struct Message recv_message;

    /* Zeroing sockaddr_in struct */
    memset(&server_addr, 0, sizeof(server_addr));
    memset(&client_addr, 0, sizeof(server_addr));

    /* Create socket */
    client_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (client_socket < 0)
    {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }
    printf("Socket created succesfully\n");

    /* Construct client_addr struct */
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(PORT_NUMBER);
    inet_pton(AF_INET, CLIENT_IP, &(client_addr.sin_addr));

    /* Binds IP and port number for client */
    if (bind(client_socket, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0)
    {
        perror("Bind failed. Error");
        exit(EXIT_FAILURE);
    }
    printf("Bind completed.\n");
    
    /* Construct server_addr struct */
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT_NUMBER);
    inet_pton(AF_INET, SERVER_IP, &(server_addr.sin_addr));
    
    /* Send message to server */
    printf("Sending message to server...\n");
    if (sendto(client_socket, buffer, strlen(buffer), 0, (struct sockaddr*)&server_addr, sock_len) < 0)
    {
        perror("sendto error");
        exit(EXIT_FAILURE);
    }  
    printf("Message sent\n");

    /* Receive messages from server */
    int msg_count = 0;
    while(msg_count < 3000)
    {
        recvfrom(client_socket, &recv_message, sizeof(recv_message), 0, (struct sockaddr *)&server_addr, &sock_len);
        msg_count++;
        recv_array[recv_message.chunk_num] = recv_message.number;
        ack_array[recv_message.chunk_num] = '1';

        printf("Received message from server. Chunk number: %d Number: %d"
            " Current message count: %d\n", recv_message.chunk_num, recv_message.number, msg_count);
    }

    /* Determine the first "gap" in message stream from server */
    for (int j=0; j < (sizeof(ack_array)/sizeof(ack_array[0])); j++)
    {
        if (ack_array[j] == '0')
        {
            printf("First gap in message stream found at index %d\n", j);
            break;
        }
    }
    close(client_socket);

    return 0;
}