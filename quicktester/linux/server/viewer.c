#include "viewer.h"

#define _BSD_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <termios.h>
#include <ctype.h>

#include "socket.h"
#include "log.h"


viewer_t* new_viewer(int client_socket)
{
        viewer_t* v = (viewer_t*)malloc(sizeof(viewer_t));
        if (v == NULL) {
                log_err("new_viewer: out of memory");
                return NULL;
        }
        memset(v, 0, sizeof(viewer_t));
        v->socket = client_socket;
        return v;
}

void delete_viewer(viewer_t* v)
{
        if (v) {
                closeServerSocket(v->socket);
                v->socket = -1;
                free(v);
                v = NULL;
        }
}

#define MAX_VIEWERS 10

static viewer_t* _viewers[MAX_VIEWERS];
static int _viewers_count = 0;

typedef struct _viewer_server_t {

        int port;
        int socket;
        pthread_t thread;

} viewer_server_t;


static viewer_server_t* viewer_server = NULL;
static int run = 1;
viewer_server_t* new_viewer_server(int socket)
{
        viewer_server_t* vs = (viewer_server_t*) malloc(sizeof(viewer_server_t));
        if (vs == NULL) {
                log_err("new_viewer_server: out of memory");
                return NULL;
        }
        memset(vs, 0, sizeof(viewer_server_t));
        vs->socket = socket;
        return vs;
}

void delete_viewer_server(viewer_server_t* vs)
{
        if (vs) {
                closeServerSocket(vs->socket);
                vs->socket = -1;
                free(vs);
        }
}

void* viewer_server_run(void* ptr) 
{
        viewer_server_t* vs = (viewer_server_t*) ptr;

        while(1) {

                if (!run)
                        break;
                
                printf("waiting for view client...\n");
                int client = serverSocketAccept(viewer_server->socket);

                if (client == -1) {
                        printf("viewer_server_run: client accept error %d\n", client);
                } else {
                        printf("viewer_server_run: client accept success %d\n", client);
                        viewers_add(client);
                }
        }

        delete_viewer_server(vs);
        viewers_clear();
        
        return 0;
}

        
int viewer_server_init(int port) {

        pthread_attr_t attr;
        int ret;

        int socket = openServerSocket(NULL, port); 
        if (socket == -1) {
                log_err("viewer_server_init: failed to create the server socket. exiting.");
                return -1;
        }

        viewer_server = new_viewer_server(socket);
        
        if (viewer_server == NULL) {
                return -1;
        }

        ret = pthread_attr_init(&attr);
        if (ret != 0) {
                log_err("viewer_server_init: pthread_attr_init failed");
                delete_viewer_server(viewer_server);
                return -1;
        }
                
        ret = pthread_create(&viewer_server->thread, &attr, &viewer_server_run, viewer_server);
        if (ret != 0) {
                log_err("viewer_init: pthread_create failed");
                delete_viewer_server(viewer_server);
                return -1;
        }

        return 0;

}

int viewer_server_end() {
        run = 0;

        return 0;
}

int viewers_init()
{
        for (int i = 0; i < MAX_VIEWERS; i++)
                _viewers[i] = NULL;

        return 0;
}


int viewers_add(int socket)
{
        if (_viewers_count >= MAX_VIEWERS) {
                log_err("viewers_add: maximum number of viewers reached");                
                return -1;
        }

        viewer_t* v = new_viewer(socket);
        if (v == NULL) {
                return -1;
        }

        _viewers[_viewers_count++] = v;

        return 0;
}


int viewers_send(const char* msg)
{
        int success = 0;
        for (int i = 0; i < MAX_VIEWERS; i++) {
                viewer_t* v = _viewers[i];
                if (v) {
                        if (v->socket != -1) {
                                if (socketSend(v->socket, msg) == -1) {
                                        if (v) {
                                                delete_viewer(v);
                                                _viewers[i] = NULL;
                                        }
                                } else {
                                        success++;
                                }
                        }
                }
        }
        return success;
}

int viewers_clear() {

        printf("viewers_clear\n");

        int success = 0;
        for (int i = 0; i < MAX_VIEWERS; i++) {
                viewer_t* v = _viewers[i];
                if (v) {
                        delete_viewer(v);
                        _viewers[i] = NULL;
                        
                } else {
                        success++;
                }                
        }


        return success;

}
