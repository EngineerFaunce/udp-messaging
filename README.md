# UDP Messaging

A simple socket program that uses UDP to send messages between a server and client.

## How it works

The application works under the client-server paradigm. The server sends messages to the client and the client responds with a message stating which
messages it missed. Each program will utilize two threads; One for UDP and one for TCP.

#### Server
1. The server declares an array of 10,000 integers initialized to 0-9999.
2. The server then waits for a message from the client.
3. When it receives this message, it begins transmitting 10,000 messages using UDP that consist of a chunk number and single integer to the client.
4. Once the server has finished sending messages, the UDP thread yields to the TCP thread which
informs the client that it has finished sending its messages.
5. The server then waits to receive an ACK message from the client's TCP thread that tracks
the chunks that the client missed.
6. Once this ACK message is received, the TCP thread yields and the UDP thread begins resending
the missing chunks.
7. This cycle continues until all 10,000 messages are received from the client.

#### Client
1. The client creates an array of 10,000 integers with all indexes intialized to -1 for receiving from the server. The client also sets up an array of 10,000 chars, each initialized to '0'. This array represents an acknowledgement message to keep track of exactly which messages have been received from the server.
2. The client then sends a message to the server stating it is ready to start receiving.
3. The client then enters into a receive loop where it receives the messages being sent by the server. Once a message has been received, the client performs the following actions:
    * Stores the integer into `recv_array[chunk_num]`
    * Tracks message receipt by setting `ack_array[chunk_num]` = '1'
4. After receiving 3000 messages, the client breaks out of the loop and yields to the TCP thread.
5. The TCP thread then waits for the "all sent" message from the server. Once received, it sends 
`ack_array` to the TCP server thread which specifies which chunks are missing.
6. The TCP thread then yields to the UDP thread which enters into the receive loop for the next
batch of messages.

## Compiling
The user must first edit the following define statements:
    
* `TCP_PORT`
* `UDP_PORT`
* `SERVER_IP`
* `CLIENT_IP`

The provided `Makefile` can then be used to compile both the server and client programs. The compiled files will then need to be copied to their respective machines.

## Usage
The user must start the server program first and then start the client program using the following commands in a terminal:
* `./server`
* `./client`

The two programs will log their outputs in the console window.