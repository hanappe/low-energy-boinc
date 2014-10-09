#ifndef _SOCKET_H
#define _SOCKET_H


 
int openServerSocket(const char* address, int port);
void closeServerSocket(int serverSocket);
int serverSocketAccept(int serverSocket);
int socketSend(int socket, const char* data);

#endif // _SOCKET_H
