# UDP Messaging

A simple socket program that uses UDP to send messages between a server and client.

## How it works

#### Server
1. The server declares an array of 10,000 integers initialized to 0-9999
2. The server then waits for an in-coming message via `recvfrom()`
3. When it receives a message, it begins transmitting 10,000 messages that consist of a chunk number and single integer to the client
4. Once the server has finished sending messages, it reports it and exits.

#### Client
1. The client creates an array of 10,000 integers with all indexes intialized to -1 for receiving from the server
2. The client also sets up an array of 10,000 chars, each initialized to '0'. This array is a form of acknowledgement to keep track of exactly which messages have been received from the server.
3. The client then sends a message to the server.
4. The client then enters into a receive loop where it receives the messages being sent by the server. Once a message has been received, the client performs the following actions:
    * Stores the integer into recv_array[chunk_num]
    * Tracks message receipt by setting ack_array[chunk_num] = '1'
5. After receiving 3000 messages, the client breaks out of the loop and determines the first "gap" in the message stream and prints out its value. The "gap" in this case is the first non-consecutive chunk number.

## Compiling
The user must first edit the following define statements:
    
* `PORT`
* `SERVER_IP`
* `CLIENT_IP`

The provided `Makefile` can then be used to compile both the server and client programs. The compiled files will then need to be copied to their respective machines.

## Usage
The user must start the server program first and then start the client program using the following commands in a terminal:
* `./server`
* `./client`

The two programs will log their outputs in the console window.