#define _BSD_SOURCE
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/time.h>
#include <pthread.h>
#include <signal.h>
#include "meter.h"
#include "log.h"

static int signaled = 0;

/* -------------------------------------------------*/

double get_time()
{
        struct timeval tv;
        gettimeofday(&tv, 0);
        return 1.e-6 * tv.tv_usec + tv.tv_sec;
}

/* -------------------------------------------------*/

static int openServerSocket(const char* address, int port) 
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

static void closeServerSocket(int serverSocket) 
{
        if (serverSocket != -1) {
                close(serverSocket);
        }
}

static int serverSocketAccept(int serverSocket) 
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

static int socketSend(int socket, const char* data) 
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

/* -------------------------------------------------*/


/* -------------------------------------------------*/

// VIEWER 

typedef struct _viewer_t {
        //int port;
        int socket;
        //pthread_t thread;
        
} viewer_t;

/* -------------------------------------------------*/


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

/*
static int viewer_init(int port) 
{
        pthread_attr_t attr;
        int ret;

        viewer_t* vs = new_viewer(port);
        if (vs == NULL) {
                return -1;
        }

        ret = pthread_attr_init(&attr);
        if (ret != 0) {
                log_err("viewer_init: pthread_attr_init failed");
                delete_viewer(vs);
                return -1;
        }
                
        ret = pthread_create(&vs->thread, &attr, &viewer_run, vs);
        if (ret != 0) {
                log_err("viewer_init: pthread_create failed");
                delete_viewer(vs);
                return -1;
        }

        return 0;
}
*/
/* -------------------------------------------------*/

#define MAX_VIEWERS 10

static viewer_t* _viewers[MAX_VIEWERS];
static int _viewers_count = 0;

typedef struct _viewer_server_t {

        int port;
        int socket;
        pthread_t thread;

} viewer_server_t;


static viewer_server_t* viewer_server = NULL;

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

static void* viewer_server_run(void* ptr) 
{
        viewer_server_t* vs = (viewer_server_t*) ptr;

        //char * msg = 0;

        while(1) {

                if (signaled)
                        break;
                
                printf("viewer_server_run serverSocketAccept\n");
                int client = serverSocketAccept(viewer_server->socket);
                printf("viewer_server_run serverSocketAccept B\n");

                if (client == -1) {
                        printf("viewer_server_run: client accept error %d\n", client);
                } else {
                        printf("viewer_server_run: client accept success %d\n", client);
                        viewers_add(client);
                }

                
                /*
                int is_connected = 1;
                while (is_connected) {
                // if (signaled)
                //              break;
                
                        
                        char data[256];
                        memset(data, 0, sizeof(data));
                        sprintf(data, "date;data;1;5.3;4.4;5.5;6.6;7.7;8.8;5.9;10.10;11.11;\n");
                        //printf("send...\n");
                        int data_sent = send(client, data, (int)strlen(data), 0);
                        //printf("debug send\n");
                        if (data_sent == -1) {
                                is_connected = 0;
                                printf("send test error...\n");
                        } else {
                                printf("data sent: %d\n", data_sent);
                        }
                        
                        //create_viewer_message();
        
                        //msg = create_csv_message(0, NULL);
                        //viewers_send(msg);
                        //if (msg) {
                        //        free(msg);
                        //}
                        
                        usleep(1000000);
                }
        */
                //usleep(1000000);


        }

        closeServerSocket(vs->socket);
        vs->socket = -1;        
        return 0;
}

        
static int viewer_server_init(int port) {

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
        printf("viewers_send BEGIN:\n");
        int success = 0;
        for (int i = 0; i < MAX_VIEWERS; i++) {
                viewer_t* v = _viewers[i];
                if (v) {
                        printf("viewers_send socket: %d\n", v->socket);
                        if (v->socket != -1) {
                                if (socketSend(v->socket, msg) == -1) {
                                        if (v) {
                                                printf("free viewer!\n");                                      
                                                delete_viewer(v);
                                                _viewers[i] = NULL;
                                        }
                                } else {
                                        success++;
                                }
                        }
                }
        }
        printf("viewers_send END:\n");
        return success;
}


/* -------------------------------------------------*/

enum {
        MESSAGE_UNKNOWN = 0,
        MESSAGE_START,
        MESSAGE_STOP,
        MESSAGE_SET,
        MESSAGE_UPDATE,
        MESSAGE_SETCOMPINFO,
        MESSAGE_UPDATECOMPINFO
};

typedef struct _message_t {
        int type;
        char name[64];
        double value;
        char string_value[256];
} message_t;

message_t* new_message()
{
        message_t* m = (message_t*) malloc(sizeof(message_t));
        if (m == NULL) {
                log_err("new_message: out of memory");
                return NULL;
        }
        memset(m, 0, sizeof(message_t));
        return m;
}

void delete_message(message_t* m)
{
        if (m) free(m);
}

int message_parse(message_t* m, char** tokens, int num_tokens)
{
        memset(m, 0, sizeof(message_t));

        if (num_tokens == 0)
                return 0;

        if (strcmp(tokens[0], "start") == 0) {
                m->type = MESSAGE_START;
                if (num_tokens != 2)
                        log_warn("message_parse: start message has bad number of arguments");
                else 
                        strncpy(m->name, tokens[1], sizeof(m->name));
                        m->name[sizeof(m->name)-1] = 0;                        
        } else if (strcmp(tokens[0], "stop") == 0) {
                m->type = MESSAGE_STOP;
        } else if (strcmp(tokens[0], "update") == 0) {
                m->type = MESSAGE_UPDATE;
        } else if (strcmp(tokens[0], "set") == 0) {
                m->type = MESSAGE_SET;
                if (num_tokens != 3)
                        log_warn("message_parse: set message has bad number of arguments");
                if (num_tokens >= 2) {
                        strncpy(m->name, tokens[1], sizeof(m->name));
                        m->name[sizeof(m->name)-1] = 0;
                }
                if (num_tokens >= 3) {
                        m->value = atof(tokens[2]);
                }
        } else if (strcmp(tokens[0], "setcompinfo") == 0) {
                //printf("SET COMP INFO");
                m->type = MESSAGE_SETCOMPINFO;
                if (num_tokens != 3)
                        log_warn("message_parse: set message has bad number of arguments");
                if (num_tokens >= 2) {
                        strncpy(m->name, tokens[1], sizeof(m->name));
                        m->name[sizeof(m->name)-1] = 0;
                }
                if (num_tokens >= 3) {
                        //m->value = atof(tokens[2]);
                        strncpy(m->string_value, tokens[2], sizeof(m->string_value));
                        m->string_value[sizeof(m->string_value)-1] = 0;
                
                }
        } else if (strcmp(tokens[0], "updatecompinfo") == 0) {
                //printf("UPDATE COMP INFO");
                m->type = MESSAGE_UPDATECOMPINFO;
        } else {
                return 0; // MESSAGE_UNKNOWN
        }
        

        

        return 0;
}


/* -------------------------------------------------*/

typedef struct _experiment_t {
        // fields set and updated by remote host
        char name[64];
        
        double cpu_usage;
        double cpu_load;
        double cpu_load_idle;
        double cpu_load_sys;
        double cpu_load_user;
        double cpu_load_comp;
        double cpu_frequency;
        double kflops;

        // fields that are updated locally
        double time_start;
        double time_end;
        double time_last;
        double energy;
        double energy_idle;
        double energy_sys;
        double energy_user;
        double energy_comp;

        double energy_last;
} experiment_t;

experiment_t* new_experiment()
{
        experiment_t* m = (experiment_t*) malloc(sizeof(experiment_t));
        if (m == NULL) {
                log_err("new_experiment: out of memory");
                return NULL;
        }
        memset(m, 0, sizeof(experiment_t));
        return m;
}

void delete_experiment(experiment_t* e)
{
        if (e) {
                free(e);
                e = NULL;
        }
}

/* -------------------------------------------------*/

static char csv_msg[256];

static char * create_csv_message(int id, experiment_t* e, double delta_t, double delta_energy)
{

        memset(csv_msg, 0, sizeof(csv_msg));

        sprintf(csv_msg,"data;%d;%f;%f;%f;%f;%f;%f;%f;%f;\n",
                id,
                e->cpu_load,
                e->cpu_load_idle,
                e->cpu_load_sys,
                e->cpu_load_user,
                e->cpu_load_comp,
                e->cpu_frequency,
                delta_t,
                delta_energy
                );
        /*
        sprintf(msg,"%d;%g;%g;%g;%g;%g;%g;%g;\n",
                id,
                1.1,
                2.2,
                3.3,
                4.4,
                5.5,
                6.6,
                7.7);
        */
        printf("create_csv_message: %s", csv_msg);
        
        return csv_msg;//strdup(msg);
}

static char csv_msg_light[256];

static char * create_csv_message_light(int id, experiment_t* e, double delta_t, double delta_energy)
{

        memset(csv_msg_light, 0, sizeof(csv_msg_light));

        sprintf(csv_msg_light,"data;%d;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f;\n",
                id,
                e->cpu_load,
                e->cpu_load_idle,
                e->cpu_load_sys,
                e->cpu_load_user,
                e->cpu_load_comp,
                e->cpu_frequency,
                delta_t,
                delta_energy
                );
        
        return csv_msg_light;
}

/* -------------------------------------------------*/

time_t get_current_time()
{
        return time(NULL);
}

static char _timestamp_buffer[256];
static unsigned long _timestamp_last = 0;

static const char* get_timestamp()
{
        struct timeval tv;
        gettimeofday(&tv, NULL);
        if (tv.tv_sec != _timestamp_last) {
                struct tm r;
                localtime_r(&tv.tv_sec, &r);
                snprintf(_timestamp_buffer, 256, "%04d-%02d-%02d %02d:%02d:%02d",
                         1900 + r.tm_year, 1 + r.tm_mon, r.tm_mday, 
                         r.tm_hour, r.tm_min, r.tm_sec);
        }
        return _timestamp_buffer;
}

/* -------------------------------------------------*/

static int _hosts_continue = 1;
void hosts_remove(int id);
#define MAX_EXPERIMENTS (10*3)+1

typedef struct _host_t {
        int id;
        int socket;
        meter_t* meter;
        pthread_t thread;
        experiment_t* experiments[MAX_EXPERIMENTS];
        int num_experiments;
        experiment_t* cur_experiment;
        double idle_power;
        char filename[256];
        FILE* file;
        
        // Computer info
        char cpu_vendor[256];
        char cpu_model[256];
        char cpu_number[256];
        char os_name[256];
        char os_version[256];

} host_t;

void host_log(host_t* host, const char * format, ...) {

        char buffer[1024];
        va_list ap;

        va_start(ap, format);
        vsnprintf(buffer, 1024, format, ap);
        buffer[1023] = 0;
        va_end(ap);

        fprintf(host->file, "%s;%s\n", get_timestamp(), buffer);
        fflush(host->file);

}

host_t* new_host(int id, int socket, meter_t* meter)
{
        host_t* host = (host_t*) malloc(sizeof(host_t));
        if (host == NULL) {
                log_err("new_host: out of memory");
                return NULL;
        }
        memset(host, 0, sizeof(host_t));
        host->id = id;
        host->socket = socket;
        host->meter = meter;

        time_t t = get_current_time();
        struct tm tm;
        localtime_r(&t, &tm);
        sprintf(host->filename, "%02d.%02d.%02d.slot%d.data.csv", tm.tm_hour, tm.tm_min, tm.tm_sec, id);

        printf("new host name: %s\n", host->filename);

        host->file = fopen(host->filename, "a");
        if (host->file == NULL) {
                log_err("can't create new host file!\n");
        }

        printf("slot: %d\n", id);
        char msg[256];
        memset(msg, 0, sizeof(msg));
        host_log(host, "connected;slot%d;", id);


        return host;
}

void delete_host(host_t* host)
{
        if (host) {
                closeServerSocket(host->socket);
                host->socket = -1;
                free(host);
                host = NULL;
        }
}

int host_start_experiment(host_t* host, message_t* m)
{        
        if (host->num_experiments >= MAX_EXPERIMENTS) {
                log_err("host_start_experiment: host %d: reached max number of experiments.", host->id);
                return -1;
        }
        log_info("host_start_experiment: new experiment %s.", m->name);

        experiment_t* experiment = new_experiment();
        if (experiment == NULL)
                return -1;

        strncpy(experiment->name, m->name, 64);
        experiment->name[63] = 0;
        experiment->time_start = get_time();

        meter_t* meter = host->meter;
        experiment->energy_last = meter_get_energy(meter, NULL, NULL);

        host->experiments[host->num_experiments++] = experiment;
        host->cur_experiment = experiment;

        host_log(host, "start_experiment;%s;", experiment->name);

        return 0;
}

int host_stop_experiment(host_t* host, message_t* m)
{        
        if (host->cur_experiment == NULL) {
                log_err("host_stop_experiment: host %d: no experiment running?!...", host->id);
                return -1;
        }

        double t = get_time();
        experiment_t* e = host->cur_experiment;

        if (strcmp(e->name, "idle") == 0) {
                host->idle_power = e->energy / (t - e->time_start);
        }

        e->time_end = t;

        log_info("host_stop_experiment: "
                 "T %f, E %f, Eidle %f, Esys %f, "
                 "E_user %f, E_comp %f, avg Watt %f, kflops %f, kflops/J %f.",
                 e->time_end - e->time_start,
                 e->energy, e->energy_idle, e->energy_sys,
                 e->energy_user, e->energy_comp,  
                 e->energy / (e->time_end - e->time_start), 
                 e->kflops, e->kflops / e->energy);

        host_log(host, "stop_experiment;%s;"
                 "%f;%f;%f;%f;"
                 "%f;%f;%f;%f;%f;",
                 e->name,
                 e->time_end - e->time_start,
                 e->energy, e->energy_idle, e->energy_sys,
                 e->energy_user, e->energy_comp,  
                 e->energy / (e->time_end - e->time_start), 
                 e->kflops, e->kflops / e->energy
                 );

        host->cur_experiment = NULL;

        return 0;
}

int host_experiment_set(host_t* host, message_t* m)
{        
        if (host->cur_experiment == NULL) {
                log_err("host_stop_experiment: host %d: no experiment running?!...", host->id);
                return -1;
        }

        //log_info("host_experiment_set: %s=%f.", m->name, m->value);

        if (strcmp(m->name, "cpu_usage") == 0) 
                host->cur_experiment->cpu_usage = m->value;
        else if (strcmp(m->name, "cpu_load") == 0) 
                host->cur_experiment->cpu_load = m->value;
        else if (strcmp(m->name, "cpu_load_idle") == 0) 
                host->cur_experiment->cpu_load_idle = m->value;
        else if (strcmp(m->name, "cpu_load_sys") == 0) 
                host->cur_experiment->cpu_load_sys = m->value;
        else if (strcmp(m->name, "cpu_load_user") == 0) 
                host->cur_experiment->cpu_load_user = m->value;
        else if (strcmp(m->name, "cpu_load_comp") == 0) 
                host->cur_experiment->cpu_load_comp = m->value;
        else if (strcmp(m->name, "cpu_frequency") == 0) 
                host->cur_experiment->cpu_frequency = m->value;
        else if (strcmp(m->name, "kflops") == 0) 
                host->cur_experiment->kflops = m->value;
        return 0;
}

int host_experiment_update(host_t* host, message_t* m)
{        
        if (host->cur_experiment == NULL) {
                log_err("host_stop_experiment: host %d: no experiment running?!...", host->id);
                return -1;
        }

        experiment_t* e = host->cur_experiment;
        meter_t* meter = host->meter;
        float energy = meter_get_energy(meter, NULL, NULL);
        float delta_energy = energy - e->energy_last;
        double t = get_time();
        double delta_t = t - e->time_last;
        char * msg = NULL;
        char * msg_light = NULL;

        e->energy += delta_energy;
        e->energy_idle += e->cpu_load_idle * delta_energy;
        e->energy_sys += e->cpu_load_sys * delta_energy;
        e->energy_user += e->cpu_load_user * delta_energy;
        e->energy_comp += e->cpu_load_comp * delta_energy;
        e->time_last = t;
        e->energy_last = energy;

        log_info("host_experiment_update: dT %f, dE %f, Watt %f", 
                 delta_t, delta_energy, delta_energy / delta_t);

        // Create csv line
        msg = create_csv_message(host->id, e, delta_t, delta_energy);
        msg_light = create_csv_message_light(host->id, e, delta_t, delta_energy);

        
        // Write msg (csv line) in file
        host_log(host, msg);

        // Send the light version to all connected viewers
        viewers_send(msg_light);

        //free(msg);

        // reinit experiment row data
        //e->cpu_load = ;

        return 0;
}

int host_experiment_setcompinfo(host_t* host, message_t* m)
{

        if (strcmp(m->name, "cpu_vendor") == 0) { 
                sprintf(host->cpu_vendor, m->string_value);
        }else if (strcmp(m->name, "cpu_model") == 0) { 
                //host->cpu_load = m->value;
                sprintf(host->cpu_model, m->string_value);
        } else if (strcmp(m->name, "cpu_number") == 0) {
                //host->cpu_loa = m->value;
                sprintf(host->cpu_number, m->string_value);
        } else if (strcmp(m->name, "os_name") == 0) {
                //host->cur_experiment->cpu_load_sys = m->value;
                sprintf(host->os_name, m->string_value);
        } else if (strcmp(m->name, "os_version") == 0) {
                //host->cur_experiment->cpu_load_user = m->value;
                sprintf(host->os_version, m->string_value);        
        }

        return 0;
}

int host_experiment_updatecompinfo(host_t* host, message_t* m)
{

        // Write csv line in file
        host_log(host, "compinfo;%s;%s;%s;%s;%s;", host->cpu_vendor, host->cpu_model, host->cpu_number, host->os_name, host->os_version);

        return 0;
}


int host_handle_message(host_t* host, message_t* m)
{        
        switch (m->type) {
        case MESSAGE_START: host_start_experiment(host, m); break;
        case MESSAGE_STOP: host_stop_experiment(host, m); break;
        case MESSAGE_SET: host_experiment_set(host, m); break;
        case MESSAGE_UPDATE: host_experiment_update(host, m); break;
        case MESSAGE_SETCOMPINFO: host_experiment_setcompinfo(host, m); break;
        case MESSAGE_UPDATECOMPINFO: host_experiment_updatecompinfo(host, m); break;
        default: break;
        }
        return 0;
}

int host_read_message(host_t* host, message_t* m)
{
        enum {
                WHITE_SPACE,
                BACKSLASH_R,
                READING_TOKEN,
                FINISHED
        };

#define BUFSIZE 512
#define MAXTOKENS 16
        char buffer[BUFSIZE];
        int buflen = 0;
        char c;
        int r;
        char* tokens[MAXTOKENS];
        int num_tokens = 0;
        int state = WHITE_SPACE;
        
        while (state != FINISHED) {                

                r = read(host->socket, &c, 1);
                if (r == 0) {
                        log_warn("host_read_message: host %d: disconnected.", host->id);
                        return -1;
                }
                if (r == -1) {
                        log_warn("host_read_message: host %d: parsing failed. ending client connection.", host->id);
                        return -1;
                }

                switch (state) {
                case WHITE_SPACE: 
                        if (c == ' ') {
                                state = WHITE_SPACE;

                        } else if (c == '\n') {
                                if (buflen < BUFSIZE - 1) {
                                        buffer[buflen++] = 0;
                                        state = FINISHED;
                                } else {
                                        log_err("host_read_message: host %d: message too long", host->id);
                                        return -1;
                                }
                        } else if (c == '\r') {
                                if (buflen < BUFSIZE - 1) {
                                        buffer[buflen++] = 0;
                                        state = BACKSLASH_R;
                                } else {
                                        log_err("host_read_message: host %d: message too long", host->id);
                                        return -1;
                                }
                        } else {
                                if (buflen < BUFSIZE - 1) {
                                        buffer[buflen] = c;
                                        if (num_tokens < MAXTOKENS - 1) {
                                                tokens[num_tokens++] = &buffer[buflen];
                                        } else {
                                                log_err("host_read_message: host %d: message too long", host->id);
                                                return -1;
                                        }
                                        buflen++;
                                        state = READING_TOKEN;
                                } else {
                                        log_err("host_read_message: host %d: message too long", host->id);
                                        return -1;
                                }
                        }
                        break;
                case READING_TOKEN: 
                        if (c == ' ') {
                                if (buflen < BUFSIZE - 1) {
                                        buffer[buflen++] = 0;
                                        state = WHITE_SPACE;
                                } else {
                                        log_err("host_read_message: host %d: message too long", host->id);
                                        return -1;
                                }

                        } else if (c == '\n') {
                                if (buflen < BUFSIZE - 1) {
                                        buffer[buflen++] = 0;
                                        state = FINISHED;
                                } else {
                                        log_err("host_read_message: host %d: message too long", host->id);
                                        return -1;
                                }
                        } else if (c == '\r') {
                                if (buflen < BUFSIZE - 1) {
                                        buffer[buflen++] = 0;
                                        state = BACKSLASH_R;
                                } else {
                                        log_err("host_read_message: host %d: message too long", host->id);
                                        return -1;
                                }
                        } else {
                                if (buflen < BUFSIZE - 1) {
                                        buffer[buflen++] = c;
                                        state = READING_TOKEN;
                                } else {
                                        log_err("host_read_message: host %d: message too long", host->id);
                                        return -1;
                                }
                        }
                        break;
                case BACKSLASH_R: 
                        if (c == '\n') {
                                state = FINISHED;
                        } else {
                                log_err("host_read_message: host %d: invalid message", host->id);
                                return -1;
                        } 
                }
        }
        
        return message_parse(m, tokens, num_tokens);
}

void* host_run(void* ptr)
{
        host_t* host = (host_t*) ptr;

        message_t* m = new_message();        
        if (m == NULL) {
                return NULL;
        }

        while (_hosts_continue) {
                int err = host_read_message(host, m);
                if (err != 0)
                        break;

                host_handle_message(host, m);
        } 

        shutdown(host->socket, 2);
        close(host->socket);
        hosts_remove(host->id);

        delete_message(m);

        return NULL;
}

/* -------------------------------------------------*/

#define MAX_HOSTS 10

static host_t* _hosts[MAX_HOSTS];
static int _hosts_count = 0;

int hosts_init()
{
        for (int i = 0; i < MAX_HOSTS; i++)
                _hosts[i] = NULL;
        return 0;
}

host_t* hosts_get(int host_id)
{
        if ((host_id < 0) || (host_id >= MAX_HOSTS))
                return NULL;
        return _hosts[host_id];
}


int hosts_add(int id, int socket, meter_t* meter)
{
        pthread_attr_t attr;
        int ret;

        if (_hosts_count >= MAX_HOSTS) {
                log_err("hosts_add: maximum number of hosts reached");                
                return -1;
        }

        host_t* host = new_host(id, socket, meter);
        if (host == NULL) {
                return -1;
        }

        ret = pthread_attr_init(&attr);
        if (ret != 0) {
                log_err("hosts_add: pthread_attr_init failed");
                delete_host(host);
                return -1;
        }
                
        ret = pthread_create(&host->thread, &attr, &host_run, host);
        if (ret != 0) {
                log_err("hosts_add: pthread_create failed");
                delete_host(host);
                return -1;
        }

        _hosts[_hosts_count++] = host;

        return host->id;
}

void hosts_remove(int id)
{
        delete_host(_hosts[id]);
        _hosts[id] = NULL;;
}

void hosts_stop()
{
        _hosts_continue = 0;

        for (int i = 0; i < _hosts_count; i++) {
                void *retval;
                pthread_join(_hosts[i]->thread, &retval);
        }
}


void hosts_delete()
{
        _hosts_continue = 0;

        for (int i = 0; i < _hosts_count; i++) {
                hosts_remove(i);
        }
}

int server_socket = -1;

/* -------------------------------------------------*/



void signal_handler(int signum)
{
        printf("signal_handler begin\n");

        signaled = 1;
        meters_stop();
        //hosts_stop();
        closeServerSocket(server_socket);
        server_socket = -1;
        if (viewer_server) {
                closeServerSocket(viewer_server->socket);
                viewer_server->socket = -1;
        }
        hosts_delete();

        printf("signal_handler end\n");
}

/* -------------------------------------------------*/

int main(int argc, char** argv)
{
        signal(SIGINT, signal_handler);
        signal(SIGHUP, signal_handler);
        signal(SIGQUIT, signal_handler);
        signal(SIGTERM, signal_handler);
        signal(SIGPWR, signal_handler);

        log_set_filep(stdout);

        if (meters_init() != 0) {
                log_err("main: failed to initialise the meters. exiting.");
                exit(1);
        }

        if (hosts_init() != 0) {
                log_err("main: failed to initialise the hosts. exiting.");
                exit(1);
        }

        server_socket = openServerSocket(NULL, 2014); 
        if (server_socket == -1) {
                log_err("main: failed to create the server socket. exiting.");
                exit(1);
        }

        meters_start();


        if (viewers_init() != 0) {
                log_err("main: failed to initialise the viewers. exiting.");
                exit(1);
        }
        
        if (viewer_server_init(2015) != 0) {
                log_err("main: failed to initialise the viewer server. exiting.");
                exit(1);
        }
        

        while (1) {

                if (signaled)
                        break;
                
                printf("main serverSocketAccept\n");
                int client = serverSocketAccept(server_socket);
                printf("main serverSocketAccept B\n");
                if (signaled)
                        break;
                if (client == -1)
                        continue;

                int id = -1;
                meter_t* meter = NULL;
                
                while (id < 0) {

                        if (signaled)
                                break;
                
                        char buf[80];
                        memset(buf, 0, sizeof(buf));

                        printf("New connection. Please enter the host ID [0-9]: ");
                        fgets(buf, 79, stdin);
                        if ((buf[0] >= '0') && (buf[0] <= '9')) {
                                id = buf[0] - '0';
                        }

                        meter = meters_get(id);
                        if (meter == NULL) {
                                printf("No associated meter found for ID %d.\n", id);
                                id = -1;
                        }
                }

                hosts_add(id, client, meter);
        }

        closeServerSocket(server_socket);
        server_socket = -1;
        printf("Finalizing...\n");

        meters_stop();

        if (meters_fini() != 0) {
                log_err("main: failed to finalize the meters");
                exit(1);
        }

        hosts_stop();

        return 0;
}
