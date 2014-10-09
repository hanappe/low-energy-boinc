#include "socket.h"

#define _BSD_SOURCE
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <arpa/inet.h>

#include "log.h"

int openServerSocket(const char* address, int port) 
{
        struct sockaddr_in sockaddr;
        struct in_addr addr;
        int serverSocket = -1;

        if ((address != NULL) && (strcmp(address, "any") != 0)) {
                int r = inet_aton(address, &addr);
                if (r == 0) {
                        log_err("Daemon: Failed to convert '%s' to an IP address...?!\n", address);
                        return -1;
                }
        } else {
                addr.s_addr = htonl(INADDR_ANY);
        }

        serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket == -1) {
                log_err("Daemon: Failed to create server socket\n");
                return -1;
        }

        memset((char *)&sockaddr, 0, sizeof(struct sockaddr_in));
        sockaddr.sin_family = AF_INET;
        sockaddr.sin_addr = addr;
        sockaddr.sin_port = htons(port);

        if (bind(serverSocket, (const struct sockaddr *) &sockaddr, 
                 sizeof(struct sockaddr_in)) == -1) {
                close(serverSocket);
                serverSocket = -1;
                log_err("Daemon: Failed to bind server socket\n");
                return -1;
        }
        
        if (listen(serverSocket, 10) == -1) {
                close(serverSocket);
                serverSocket = -1;
                log_err("Daemon: Failed to bind server socket\n");
                return -1;
        }

        return serverSocket;
}

void closeServerSocket(int serverSocket) 
{
        if (serverSocket != -1) {
                close(serverSocket);
        }
}

int serverSocketAccept(int serverSocket) 
{
        struct sockaddr_in addr;
        socklen_t addrlen = sizeof(addr);

        if (serverSocket == -1) {
                log_err("Daemon: Invalid server socket\n");
                return -1;
        }
  
        int clientSocket = accept(serverSocket, (struct sockaddr*) &addr, &addrlen);
  
        if (clientSocket == -1) {
                log_err("Daemon: Accept failed\n");
                return -1;
        } 

        int flag = 1; 
        setsockopt(clientSocket, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));

        return clientSocket;
}

int socketSend(int socket, const char* data) 
{
        //printf("serverSocketSend BEGIN: %d\n", socket);
        //printf("serverSocketSend data: %s\n", data);
        int data_sent = send(socket, data, (int)strlen(data), 0);
        //printf("serverSocketSend data: %s\n", data);
        if (data_sent == -1) {
                printf("send test error...\n");
        } else {
                printf("data sent: %d\n", data_sent);
        }

        //printf("serverSocketSend END\n");

        return data_sent;
}
