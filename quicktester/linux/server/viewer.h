#ifndef _VIEWER_H
#define _VIEWER_H

typedef struct _viewer_t {
        //int port;
        int socket;
        //pthread_t thread;
        
} viewer_t;

viewer_t* new_viewer(int client_socket);
void delete_viewer(viewer_t* v);

void* viewer_server_run(void* ptr); 
int viewer_server_init(int port);
int viewer_server_end();

int viewers_init();
int viewers_add(int socket);
int viewers_send(const char* msg);
int viewers_clear();

#endif // _VIEWER_H
