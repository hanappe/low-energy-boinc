#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <stdio.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "winsocket.h"

#define SUCCESS 0

static SOCKET _socket = INVALID_SOCKET;

int winsocket_init()
{
	struct addrinfo* _result = NULL;
    struct addrinfo* _ptr = NULL;
    struct addrinfo _hints;
	// Temp var, for testing
	char address[] = "10.0.1.114";
	//char address[] = "10.0.1.202";
	char port[] = "2014";
    WSADATA InitData;
	int res;

	WSAStartup(MAKEWORD(2,2), &InitData);

	_socket = INVALID_SOCKET;

	ZeroMemory( &_hints, sizeof(_hints) );
	_hints.ai_family = AF_UNSPEC;
	_hints.ai_socktype = SOCK_STREAM;
	_hints.ai_protocol = IPPROTO_TCP;

	res = getaddrinfo(address, port, &_hints, &_result);
	if (res != SUCCESS) {
		printf("getaddrinfo failed with error: %d\n", res);
		WSACleanup();
		return -1;
	}

	// Attempt to connect to an address until one succeeds
	for(_ptr = _result; _ptr != NULL ;_ptr = _ptr->ai_next) {

    // Create a SOCKET for connecting to server
    _socket = socket(_ptr->ai_family, _ptr->ai_socktype, 
        _ptr->ai_protocol);
    if (_socket == INVALID_SOCKET) {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        WSACleanup();
        return -1;
    }

    // Connect to server.
    res = connect(_socket, _ptr->ai_addr, (int)_ptr->ai_addrlen);
    if (res == SOCKET_ERROR) {
        closesocket(_socket);
        _socket = INVALID_SOCKET;
        continue;
    }
    break;
}

	freeaddrinfo(_result);

	if (_socket == INVALID_SOCKET) {
		printf("Unable to connect to server!\n");
		WSACleanup();
		return -1;
	}

	return 0;
}

int winsocket_send(const char * data)
{
	int data_sent;

	printf("send data");

	data_sent = send(_socket, data, (int)strlen(data), 0 );
	if (data_sent == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		WSACleanup();
		return -1;
	}

	printf("data sent: %d\n", data_sent);
	return data_sent;
}

int winsocket_end()
{
	if (_socket >= 0 && _socket != INVALID_SOCKET) {
		// shutdown the connection since no more data will be sent
		int res = shutdown(_socket, SD_SEND);
		if (res == SOCKET_ERROR) {
			printf("shutdown failed with error: %d\n", WSAGetLastError());
			closesocket(_socket);
			WSACleanup();
			return 0;
		}

		// cleanup
		closesocket(_socket);

		_socket = -1;
	}

 	WSACleanup();

	return 0;
}