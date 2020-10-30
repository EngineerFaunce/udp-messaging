/* Server code */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT_NUMBER     45210
#define SERVER_IP       "1.22.333.4444" // replace with IP address of server

int main(int argc, char **argv)
{
    int arr[10000];
    for (int i=0; i < sizeof(arr)/sizeof(arr[0]); i++)
        arr[i] = i;
}