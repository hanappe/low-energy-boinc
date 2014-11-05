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

#include "socket.h"
#include "viewer.h"
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

enum {
        MESSAGE_UNKNOWN = 0,
        MESSAGE_START,
        MESSAGE_STOP,
        MESSAGE_SET,
        MESSAGE_UPDATE,
        MESSAGE_SETCOMPINFO,
        MESSAGE_UPDATECOMPINFO,
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
        if (m) {
                free(m);
        }
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
        
        double cpu_usage; // range [0.;1.0]
        double cpu_load; // range [0.;1.0]
        double cpu_load_idle; // range [0.;1.0]
        double cpu_load_sys; // range [0.;1.0]
        double cpu_load_user; // range [0.;1.0]
        double cpu_load_comp; // range [0.;1.0]
        double cpu_frequency; // in MHz
        double kflops;

        // Portable PC only >>>>>> UNUSED FOR NOW <<<<<<
        double on_battery; // <= 0 not on battery, > 0 on battery 
        double bat_diff_capacity; // in mWh
        double bat_cur_capacity; // in mWh
        double bat_cur_capacity_last; // in mWh
        double bat_diff_percent; // range [0.;1.0]
        double bat_life_percent; // range [0.;1.0]
        double bat_life_percent_last; // range [0.;1.0]

        // fields that are updated locally
        double time_start;
        double time_end;
        double time_last;

        // energy measurements, using Watt meter
        double energy;
        double energy_idle;
        double energy_sys;
        double energy_user;
        double energy_comp;

        // energy measurements, using battery
        double energy_battery;
        double battery_idle;
        double battery_sys;
        double battery_user;
        double battery_comp;

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

        sprintf(csv_msg,"data;"
                "%d;"
                "%f;%f;%f;"
                "%f;%f;%f;"
                "%f;%f;"
                "%d;%f;%f;"
                "\n",
                id,
                e->cpu_load, e->cpu_load_idle, e->cpu_load_sys,
                e->cpu_load_user, e->cpu_load_comp, e->cpu_frequency,
                delta_t, delta_energy,
                e->on_battery > 0 ? 1 : 0, e->bat_cur_capacity, e->bat_life_percent
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
        //printf("create_csv_message: %s", csv_msg);
        
        return csv_msg;//strdup(msg);
}

static char csv_viewer_msg[256];

// For Viewer
static char * create_viewer_message(int id, experiment_t* e, double delta_t, double delta_energy)
{

        memset(csv_viewer_msg, 0, sizeof(csv_viewer_msg));

        sprintf(csv_viewer_msg,"data;%d;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f;\n",
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
        
        return csv_viewer_msg;
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

static const char* get_timestamp_without_space()
{
        struct timeval tv;
        gettimeofday(&tv, NULL);
        if (tv.tv_sec != _timestamp_last) {
                struct tm r;
                localtime_r(&tv.tv_sec, &r);
                snprintf(_timestamp_buffer, 256, "%04d-%02d-%02d_%02d:%02d:%02d",
                         1900 + r.tm_year, 1 + r.tm_mon, r.tm_mday, 
                         r.tm_hour, r.tm_min, r.tm_sec);
        }
        return _timestamp_buffer;
}

/* -------------------------------------------------*/

static int _hosts_continue = 1;
void hosts_remove(int id);

#define MAX_EXPERIMENTS 60

typedef struct _host_t {
        int id;
        int socket;
        meter_t* meter;
        pthread_t thread;
        experiment_t* experiments[MAX_EXPERIMENTS];
        int num_experiments;
        //experiment_t* experiments_to_save[MAX_EXPERIMENTS];
        //int num_experiments_to_save;
        experiment_t* cur_experiment;
        double idle_power;
        FILE* file_csv;
        FILE* file_info;
        
        // Computer info
        char cpu_vendor[256];
        char cpu_model[256];
        char cpu_number[256];
        char os_name[256];
        char os_version[256];
        double bat_max_capacity; // in mWh

} host_t;

void host_log_csv(host_t* host, const char * format, ...) {

        char buffer[1024];
        va_list ap;

        va_start(ap, format);
        vsnprintf(buffer, 1024, format, ap);
        buffer[1023] = 0;
        va_end(ap);

        fprintf(host->file_csv, "%s;%s\n", get_timestamp(), buffer);
        fflush(host->file_csv);

}

void host_log_info(host_t* host, const char * format, ...) {

        char buffer[1024];
        va_list ap;

        va_start(ap, format);
        vsnprintf(buffer, 1024, format, ap);
        buffer[1023] = 0;
        va_end(ap);

        // Print to standard output
        fprintf(stdout, "%s\n", buffer);

        // Write to file
        fprintf(host->file_info, "%s\n", buffer);
        
        fflush(host->file_info);

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


        // File

        char filename[256];
        memset(filename, 0, sizeof(filename));

        // File csv

        time_t t = get_current_time();
        struct tm tm;
        localtime_r(&t, &tm);
        sprintf(filename, "%s.slot%d.csv", get_timestamp_without_space(), id);


        printf("new host name: %s\n", filename);

        host->file_csv = fopen(filename, "a");
        if (host->file_csv == NULL) {
                log_err("can't create new host file!\n");
        }

        host_log_csv(host, "connected;slot%d;", id);

        // File summary

        memset(filename, 0, sizeof(filename));
        sprintf(filename, "%s.slot%d.info", get_timestamp_without_space(), id);

        host->file_info = fopen(filename, "a");
        if (host->file_info == NULL) {
                log_err("can't create new host file_summary!\n");
        }


        /*
        host_log_info(host,
                 "T\tE\tEidle\tEsys\t"
                 "E_user\tE_comp\tavg Watt\tkflops\tkflops/J"
                 );
        host_log_info(host,
                 "");
        */
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

        host_log_csv(host, "start_experiment;%s;", experiment->name);

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

        host_log_csv(host, "stop_experiment;%s;"
                 "%f;%f;%f;%f;"
                 "%f;%f;%f;%f;%f;",
                 e->name,
                 e->time_end - e->time_start,
                 e->energy, e->energy_idle, e->energy_sys,
                 e->energy_user, e->energy_comp,  
                 e->energy / (e->time_end - e->time_start), 
                 e->kflops, e->kflops / e->energy
                 );

        /*
        host_log_info(host, "%s:", e->name);
        host_log_info(host,
                 "%.2f\t%.2f\t%.2f\t%.2f\t"
                 "%.2f\t%.2f\t%.2f\t%.2f\t%.2f",
                 e->time_end - e->time_start,
                 e->energy, e->energy_idle, e->energy_sys,
                 e->energy_user, e->energy_comp,  
                 e->energy / (e->time_end - e->time_start), 
                 e->kflops, e->kflops / e->energy
                 );
        */
        //host->experiments_to_save[host->num_experiments_to_save] = e;
        //host->num_experiments_to_save++;

        host->cur_experiment = NULL;

        return 0;
}

int host_experiment_set(host_t* host, message_t* m)
{        
        if (host->cur_experiment == NULL) {
                log_err("host_experiment_set: host %d: no experiment running?!...", host->id);
                return -1;
        }

        //log_info("host_experiment_set: %s=%f.", m->name, m->value);

        if (strcmp(m->name, "cpu_usage") == 0)
                host->cur_experiment->cpu_usage = m->value / 100.;
        else if (strcmp(m->name, "cpu_load") == 0)
                host->cur_experiment->cpu_load = m->value / 100.;
        else if (strcmp(m->name, "cpu_load_idle") == 0) 
                host->cur_experiment->cpu_load_idle = m->value / 100.;
        else if (strcmp(m->name, "cpu_load_sys") == 0) 
                host->cur_experiment->cpu_load_sys = m->value / 100.;
        else if (strcmp(m->name, "cpu_load_user") == 0) 
                host->cur_experiment->cpu_load_user = m->value / 100.;
        else if (strcmp(m->name, "cpu_load_comp") == 0) 
                host->cur_experiment->cpu_load_comp = m->value / 100.;
        else if (strcmp(m->name, "cpu_frequency") == 0) 
                host->cur_experiment->cpu_frequency = m->value;
        else if (strcmp(m->name, "kflops") == 0) 
                host->cur_experiment->kflops = m->value;
        else if (strcmp(m->name, "on_battery") == 0)
                host->cur_experiment->on_battery = m->value;
        else if (strcmp(m->name, "bat_cur_capacity") == 0) 
                host->cur_experiment->bat_cur_capacity = m->value;
        else if (strcmp(m->name, "bat_life_percent") == 0)
                host->cur_experiment->bat_life_percent = m->value / 100.;
        
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
        char * viewer_msg = NULL;

        e->energy += delta_energy;
        
        e->energy_idle += e->cpu_load_idle * delta_energy / 100.0;
        e->energy_sys += e->cpu_load_sys * delta_energy / 100.0;
        e->energy_user += e->cpu_load_user * delta_energy / 100.0;
        e->energy_comp += e->cpu_load_comp * delta_energy / 100.0;
        e->time_last = t;
        e->energy_last = energy;

        log_info("host_experiment_update: "
                 "dT %f, dE %f, Watt %f,"
                 " Idle %f, Sys %f,"
                 "User %f, Comp %f,",
                 delta_t, delta_energy, delta_energy / delta_t, 
                 e->cpu_load_idle, e->cpu_load_sys,
                 e->cpu_load_user, e->cpu_load_comp);

        // Battery
        if (e->bat_cur_capacity_last == 0) {
                e->bat_cur_capacity_last = e->bat_cur_capacity;
        }
        if (e->bat_life_percent_last == 0) {
                e->bat_life_percent_last = e->bat_life_percent;
        }
        double delta_bat_capacity = e->bat_cur_capacity - e->bat_cur_capacity_last;
        double delta_bat_percent = e->bat_life_percent - e->bat_life_percent_last;
        delta_energy = 3.6 * delta_bat_capacity; // Conversion from mWh to J

        e->bat_diff_capacity += delta_bat_capacity;
        e->bat_diff_percent += delta_bat_percent;
        log_info("host_experiment_update: "
                 "dBat %f, dBatp %f,"
                 "BatDiff %f, BatDiffp %f",
                 delta_bat_capacity, delta_bat_percent,
                 e->bat_diff_capacity, e->bat_diff_percent);

        e->energy_battery += delta_energy;
        e->battery_idle += e->cpu_load_idle * delta_energy / 100.0;
        e->battery_sys += e->cpu_load_sys * delta_energy / 100.0;
        e->battery_user += e->cpu_load_user * delta_energy / 100.0;
        e->battery_comp += e->cpu_load_comp * delta_energy / 100.0;

        e->bat_cur_capacity_last = e->bat_cur_capacity;
        e->bat_life_percent_last = e->bat_life_percent;
        
        //e->bat_used += delta_battery_capacity;
        //e->bat_cur_capacity_last = e->bat_cur_capacity;

        // Create csv line
        msg = create_csv_message(host->id, e, delta_t, delta_energy);
        viewer_msg = create_viewer_message(host->id, e, delta_t, delta_energy);
        
        // Write msg  in file
        host_log_csv(host, msg);

        // Send msg to all connected viewers
        viewers_send(viewer_msg);

        return 0;
}

int host_experiment_setcompinfo(host_t* host, message_t* m)
{
        if (strcmp(m->name, "cpu_vendor") == 0) { 
                sprintf(host->cpu_vendor, m->string_value);
        } else if (strcmp(m->name, "cpu_model") == 0) { 
                sprintf(host->cpu_model, m->string_value);
        } else if (strcmp(m->name, "cpu_number") == 0) {
                sprintf(host->cpu_number, m->string_value);
        } else if (strcmp(m->name, "os_name") == 0) {
                sprintf(host->os_name, m->string_value);
        } else if (strcmp(m->name, "os_version") == 0) {
                sprintf(host->os_version, m->string_value);        
        } else if (strcmp(m->name, "bat_max_capacity") == 0) {
                host->bat_max_capacity = atof(m->string_value);
        }

        return 0;
}

int host_experiment_updatecompinfo(host_t* host, message_t* m)
{

        // Write csv line in file
        host_log_csv(host, "computer_info;%s;%s;%s;%s;%s;", 
                     host->cpu_vendor, host->cpu_model, 
                     host->cpu_number, host->os_name, host->os_version);
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
                //case MESSAGE_SAVE_AVERAGE: host_experiment_save_average(host, m); break;
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

void host_print_summary(host_t* host) 
{
        double start_time = 0;
        double total_duration = 0.0;
        double total_total_energy = 0.0;
        double total_performance = 0.0;
        double total_bat_diff = 0.0;
        double total_bat_diff_percent = 0.0;
        double idle = 0.0;
        double energy = 0.0;
        double duration = 0.0;

        for (int i = 0; i < host->num_experiments; i++) {
                experiment_t* e = host->experiments[i];
                if (strcmp(e->name, "idle") == 0) {
                        energy += e->energy;
                        duration += (e->time_end - e->time_start);
                }
        }

        if (duration > 0)
                idle = energy / duration;

        // Host info
        {
                host_log_info(host, "HostInfo:------------------------------------");
                host_log_info(host, "CPU vendor:  %s", host->cpu_vendor);
                host_log_info(host, "CPU model:   %s", host->cpu_model);
                host_log_info(host, "CPU number:  %s", host->cpu_number);
                host_log_info(host, "OS name:     %s", host->os_name);
                host_log_info(host, "OS version:  %s", host->os_version);
                host_log_info(host, "Battery Max Capacity: %f mWh", host->bat_max_capacity);
        }

        for (int i = 0; i < host->num_experiments; i++) {
                double power, xtra_power;
                experiment_t* e = host->experiments[i];

                /*log_info("----------------------------------------");
                log_info("Experiment:         %s", e->name);
                log_info("Idle consumption:   %f W", idle);
                log_info("Duration:           %f sec", e->time_end - e->time_start);
                log_info("Total energy:       %f J", e->energy);
                log_info("Idle energy:        %f J", e->energy_idle);
                log_info("System energy:      %f J", e->energy_sys);
                log_info("User energy:        %f J", e->energy_user);
                log_info("Computation energy: %f J", e->energy_comp);
                log_info("Average power:      %f W", power);
                log_info("Performance:        %f kflops", e->kflops);
                log_info("Performance/power   %f kflops/W", e->kflops / power);
                log_info("Performance/extra P %f kflops/W", e->kflops / xtra_power);
                log_info("Battery diff %f mWh", e->bat_diff_capacity);
                log_info("Battery diff percent %f", e->bat_diff_percent);
                */
                host_log_info(host, "ExperimentInfo--------------------------");
                host_log_info(host, "Experiment:            %s", e->name);
                host_log_info(host, "Idle consumption:      %f W", idle);
                host_log_info(host, "Duration:              %f sec", e->time_end - e->time_start);

                power = e->energy / (e->time_end - e->time_start);
                xtra_power = e->energy / (e->time_end - e->time_start) - idle;
                host_log_info(host, "WATT-METER");
                host_log_info(host, "Total energy:          %f J", e->energy);
                host_log_info(host, "Idle energy:           %f J", e->energy_idle);
                host_log_info(host, "System energy:         %f J", e->energy_sys);
                host_log_info(host, "User energy:           %f J", e->energy_user);
                host_log_info(host, "Computation energy:    %f J", e->energy_comp);
                host_log_info(host, "Average power:         %f W", power);
                host_log_info(host, "Performance:           %f kflops", e->kflops);
                if (power > 0)
                        host_log_info(host, "Performance/power      %f kflops/W", 
                                      e->kflops / power);
                else
                        host_log_info(host, "Performance/power      NA");

                if (xtra_power > 0)
                        host_log_info(host, "Performance/extra P    %f kflops/W", 
                                      e->kflops / xtra_power);
                else if (xtra_power < 0)
                        host_log_info(host, "Performance/extra P    ERROR (<0)", 
                                      e->kflops / xtra_power);
                else 
                        host_log_info(host, "Performance/extra P    NA");

                power = e->energy_battery / (e->time_end - e->time_start);
                xtra_power = e->energy_battery / (e->time_end - e->time_start) - idle;
                host_log_info(host, "BATTERY");
                host_log_info(host, "Total energy:          %f J", e->energy_battery);
                host_log_info(host, "Idle energy:           %f J", e->battery_idle);
                host_log_info(host, "System energy:         %f J", e->battery_sys);
                host_log_info(host, "User energy:           %f J", e->battery_user);
                host_log_info(host, "Computation energy:    %f J", e->battery_comp);
                host_log_info(host, "Average power:         %f W", power);
                host_log_info(host, "Performance:           %f kflops", e->kflops);
                if (power > 0)
                        host_log_info(host, "Performance/power      %f kflops/W", 
                                      e->kflops / power);
                else
                        host_log_info(host, "Performance/power      NA");

                if (xtra_power > 0)
                        host_log_info(host, "Performance/extra P    %f kflops/W", 
                                      e->kflops / xtra_power);
                else if (xtra_power < 0)
                        host_log_info(host, "Performance/extra P    ERROR (<0)", 
                                      e->kflops / xtra_power);
                else 
                        host_log_info(host, "Performance/extra P    NA");

                host_log_info(host, "Battery diff percent   %f", e->bat_diff_percent);
                                
                if (i == 0) {
                        start_time = e->time_start;
                } else if (i == host->num_experiments - 1) {
                        total_duration =  e->time_end - start_time;
                }
                
                total_total_energy += e->energy;
                total_performance += e->kflops;
                total_bat_diff += e->bat_diff_capacity;
                total_bat_diff_percent += e->bat_diff_percent;
        }

        // Final Info
        {
                host_log_info(host, "FinalInfo:------------------------------------");
                host_log_info(host, "Total duration: %f sec", total_duration);
                host_log_info(host, "Total energy: %f J", total_total_energy);
                host_log_info(host, "Total performance: %f kflops", total_performance);
                host_log_info(host, "Total battery diff: %f mWh", total_bat_diff);
                host_log_info(host, "Total battery diff percent %f", total_bat_diff_percent);
        }
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

        host_print_summary(host);
        
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
                host_t * h = _hosts[i];
                if (h) {
                        pthread_join(h->thread, &retval);
                }        
        }
}


void hosts_delete()
{
        _hosts_continue = 0;

        for (int i = 0; i < _hosts_count; i++) {
                hosts_remove(i);
        }
}

/* -------------------------------------------------*/

int read_slot(int client) {
        // Reading slot
                
        int r;
        char c;
        char last_char = 0;
        int count = 0;

        // We have to take only 2 chars, the slot number et the end of line

        while (count < 2) {                
                
                r = read(client, &c, 1);
                if (r == 0) {
                        log_warn("read_slot: host %d: disconnected.", client);
                        break;
                }
                if (r == -1) {
                        log_warn("read_slot: host %d: parsing failed. ending client connection.", client);
                        break;
                }

                if (c == '\n' && last_char >= '0' && last_char <= '9') {
                        return (int) (last_char - '0');
                } else if (c >= '0' && c <= '9') {
                        last_char = c;
                } else {
                        break;
                }

                count++;
        }
        return -1;
}

/* -------------------------------------------------*/

static int server_socket = -1;

/* -------------------------------------------------*/


static int main_ended = 0;

void main_end() {

        if (!main_ended) {

                printf("Finalizing...\n");
                
                closeServerSocket(server_socket);
                server_socket = -1;
                
                viewer_server_end();
                
                hosts_stop();
                
                hosts_delete();
                
                meters_stop();
                
                if (meters_fini() != 0) {
                        log_err("main: failed to finalize the meters");
                        exit(1);
                }
                main_ended = 1;
        }
}



/* -------------------------------------------------*/
static int lock = 0; // very dirty

void signal_handler(int signum)
{
        if (!lock) {
                lock = 1;

                signaled = 1;
                main_end();

                lock = 0;
        }
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
                
                printf("waiting for client...\n");
                int client = serverSocketAccept(server_socket);

                
                if (client == -1)
                        continue;

                int slot = -1;
                meter_t* meter = NULL;
                
                slot = read_slot(client);
                
                if (slot == -1) {

                        log_err("slot not found: %d...", slot);
                        continue;

                } else {

                        meter = meters_get(slot);
                        if (meter == NULL) {
                                log_err("No associated meter found for slot ID %d.", slot);
                                slot = -1;
                        }

                        log_info("New connection with slot: %d", slot);
                        hosts_add(slot, client, meter);
                }

                /*
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

                        
                }
                */
                
        }
        
        // Main ending 
        main_end();

        return 0;
}
