#define _BSD_SOURCE
#include <stdio.h>
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
#include "meter.h"
#include "log.h"

/* -------------------------------------------------*/

static int signaled = 0;

void signal_handler(int signum)
{
        signaled = 1;
        meters_stop();
}

/* -------------------------------------------------*/

double get_time()
{
        struct timeval tv;
        gettimeofday(&tv, 0);
        return 1.e-6 * tv.tv_usec + tv.tv_sec;
}

/* -------------------------------------------------*/

enum {
        MESSAGE_UNKNOWN = 0,
        MESSAGE_START,
        MESSAGE_STOP,
        MESSAGE_SET,
        MESSAGE_UPDATE
};

typedef struct _message_t {
        int type;
        char name[64]; 
        double value; 
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
        if (e) free(e);
}

/* -------------------------------------------------*/

static int _hosts_continue = 1;
void hosts_remove(int id);
#define MAX_EXPERIMENTS 16

typedef struct _host_t {
        int id;
        int socket;
        meter_t* meter;
        pthread_t thread;
        experiment_t* experiments[MAX_EXPERIMENTS];
        int num_experiments;
        experiment_t* cur_experiment;
        double idle_power;
} host_t;

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
        return host;
}

void delete_host(host_t* host)
{
        if (host) {
                free(host);
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
        experiment->energy_last = meter_get_energy(meter);

        host->experiments[host->num_experiments++] = experiment;
        host->cur_experiment = experiment;
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
        float energy = meter_get_energy(meter);
        float delta_energy = energy - e->energy_last;
        double t = get_time();
        double delta_t = t - e->time_last;

        e->energy += delta_energy;
        e->energy_idle += e->cpu_load_idle * delta_energy;
        e->energy_sys += e->cpu_load_sys * delta_energy;
        e->energy_user += e->cpu_load_user * delta_energy;
        e->energy_comp += e->cpu_load_comp * delta_energy;
        e->time_last = t;
        e->energy_last = energy;

        log_info("host_experiment_update: dT %f, dE %f, Watt %f", 
                 delta_t, delta_energy, delta_energy / delta_t);

        return 0;
}

int host_handle_message(host_t* host, message_t* m)
{        
        switch (m->type) {
        case MESSAGE_START: host_start_experiment(host, m); break;
        case MESSAGE_STOP: host_stop_experiment(host, m); break;
        case MESSAGE_SET: host_experiment_set(host, m); break;
        case MESSAGE_UPDATE: host_experiment_update(host, m); break;
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

        int server_socket = openServerSocket(NULL, 2014); 
        if (server_socket == -1) {
                log_err("main: failed to create the server socket. exiting.");
                exit(1);
        }

        while (1) {

                int client = serverSocketAccept(server_socket); 
                if (signaled)
                        break;
                if (client == -1)
                        continue;

                int id = -1;
                meter_t* meter = NULL;
                
                while (id < 0) {
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

        printf("Finalizing...\n");

        if (meters_fini() != 0) {
                log_err("main: failed to finalize the meters");
                exit(1);
        }

        hosts_stop();

        return 0;
}
