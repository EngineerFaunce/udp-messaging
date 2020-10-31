/* Server code */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT_NUMBER     45210
#define SERVER_IP       "1.1.1.1"

/* Message struct */
struct Message {
    int chunk_num;
    int number;
};

int main(int argc, char **argv)
{
    int send_array[10000];
    for (int i=0; i < sizeof(send_array)/sizeof(send_array[0]); i++)
        send_array[i] = i;

    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    char buffer[1024];
    int recv_len = 0;
    socklen_t sock_len = sizeof(struct sockaddr_in);

    /* Zeroing sockaddr_in struct */
    memset(&server_addr, 0, sizeof(server_addr));
    memset(&client_addr, 0, sizeof(client_addr));

    /* Create socket */
    server_socket = socket (AF_INET, SOCK_DGRAM, 0);
    if (server_socket < 0)
    {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }
    printf("Socket created succesfully\n");

    /* Construct server_addr struct */
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT_NUMBER);
    inet_pton(AF_INET, SERVER_IP, &(server_addr.sin_addr));

    /* Binds IP and port number */
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Bind failed. Error");
        exit(EXIT_FAILURE);
    }
    printf("Bind completed.\n");

    /* Receive incoming message from client */
    printf("Waiting for message from client...\n");
    recv_len = recvfrom(server_socket, buffer, 1024, 0, (struct sockaddr*)&client_addr, &sock_len);
    printf("Message received from client: %s\n", buffer);

    /* Begin sending messages */
    printf("Server beginning transmission...\n");
    struct Message message;
    for (int j=0; j < 10000; j++)
    {
        message.chunk_num = j;
        message.number = send_array[j];
        sendto(server_socket, &message, sizeof(message), 0, (struct sockaddr*)&client_addr, sock_len);
    }
    printf("Server has completed sending messages.\n");

    return 0;
}