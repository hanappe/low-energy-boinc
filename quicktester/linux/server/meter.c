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
#include "log.h"
#include "meter.h"

static double get_time()
{
        struct timeval tv;
        gettimeofday(&tv, 0);
        return 1.e-6 * tv.tv_usec + tv.tv_sec;
}

/* -------------------------------------------------*/

typedef struct {
	char cmd;
	char sub_cmd;

	char buf[512];
	int buf_len;

	char* fields[18];
	int field_count;

        time_t write_time;
        char wait_reply;

} packet_t;

static void packet_init(packet_t* p)
{
        bzero(p, sizeof(*p));
}

static packet_t* packet_new()
{
        packet_t* p = malloc(sizeof(*p));
        packet_init(p);
        return p;
}

static void packet_delete(packet_t* p)
{
        free(p);
}

/*
static void packet_version_request(packet_t* p)
{
        packet_init(p);
        p->cmd = 'V';
        p->sub_cmd = 'R';
        p->field_count = 0;
        p->wait_reply = 'v';
}
*/

static void packet_request_fields(packet_t* p)
{
        packet_init(p);
        p->cmd = 'C';
        p->sub_cmd = 'W';
        p->field_count = 18;
        p->fields[0] = "1"; // watts
        p->fields[1] = "0"; // volts
        p->fields[2] = "0"; // amps
        p->fields[3] = "0"; // watt hours
        p->fields[4] = "0"; // cost
        p->fields[5] = "0"; // mo. kWh
        p->fields[6] = "0"; // mo. cost
        p->fields[7] = "0"; // max watts
        p->fields[8] = "0"; // max volts
        p->fields[9] = "0"; // max amps
        p->fields[10] = "0"; // min watts
        p->fields[11] = "0"; // min volts
        p->fields[12] = "0"; // min amps
        p->fields[13] = "0"; // power factor
        p->fields[14] = "0"; // duty cycle
        p->fields[15] = "0"; // power cycle
        p->fields[16] = "0"; // line frequency Hz
        p->fields[17] = "0"; // volt-amps
        p->wait_reply = 'n';
}

static void packet_memory_full_handling(packet_t* p)
{
        packet_init(p);
        p->cmd = 'O';
        p->sub_cmd = 'W';
        p->field_count = 1;
        p->fields[0] = "1";
}

static void packet_external_logging_1second(packet_t* p)
{
        packet_init(p);
        p->cmd = 'L';
        p->sub_cmd = 'W';
        p->field_count = 3;
        p->fields[0] = "E";
        p->fields[1] = "";
        p->fields[2] = "1";
}

static void packet_internal_logging_600seconds(packet_t* p)
{
        packet_init(p);
        p->cmd = 'L';
        p->sub_cmd = 'W';
        p->field_count = 3;
        p->fields[0] = "I";
        p->fields[1] = "";
        p->fields[2] = "600";
}

/* -------------------------------------------------*/

struct meter_s {
        int usb_id;
        char* device;

        int fd;
        char buf[512];
        int buf_len;
        int buf_pos0;
        int buf_pos1;
        double last_read_time;
        packet_t* packet;

        int run;
        pthread_attr_t attr;
        pthread_t thread;

        float energy;
        double last_energy_time;
};

static meter_t* meter_new(int usb_id);
static void meter_delete(meter_t* meter);
static void* meter_run(void* ptr);
static int meter_packet_write(meter_t* meter, packet_t* p);
static int meter_packet_read(meter_t* meter, packet_t* p);
static int meter_read_watts(meter_t* m);

meter_t* meter_new(int usb_id)
{
        meter_t* m = calloc(1, sizeof(*m));
        if (m == NULL) {
                log_err("meter_new: out of memory");
                return NULL;
        }

        m->usb_id = usb_id;

        m->device = calloc(512, sizeof(char));
        if (m->device == NULL) {
                log_err("meter_new: out of memory");
                free(m);
                return NULL;
        }

        snprintf(m->device, 512, "/dev/ttyUSB%d", usb_id);
        m->device[511] = '\0';

        m->fd = -1;
        m->fd = open(m->device, O_RDWR | O_NONBLOCK);
        if (m->fd < 0) {
                meter_delete(m);
                return NULL;
        }

        tcflush(m->fd, TCIOFLUSH);

        struct termios tios;
        int err = tcgetattr(m->fd, &tios);
        if (err) {
                meter_delete(m);
                return NULL;
        }

        cfmakeraw(&tios);
        cfsetispeed(&tios, B115200);
        cfsetospeed(&tios, B115200);
        tios.c_iflag |= IGNPAR;
        tios.c_cflag &= ~CSTOPB;

        err = tcsetattr(m->fd, TCSANOW, &tios);
        if (err) {
                meter_delete(m);
                return NULL;
        }

        printf("Trying %s...\n", m->device);

        m->packet = packet_new();

        for (int i = 0; ; i++) {
                packet_request_fields(m->packet);
                err = meter_packet_write(m, m->packet);
                if (err) {
                        meter_delete(m);
                        return NULL;
                }

                packet_memory_full_handling(m->packet);
                err = meter_packet_write(m, m->packet);
                if (err) {
                        meter_delete(m);
                        return NULL;
                }

                packet_external_logging_1second(m->packet);
                err = meter_packet_write(m, m->packet);
                if (err) {
                        meter_delete(m);
                        return NULL;
                }

                err = meter_read_watts(m);
                if (!err) break;

                if (i >= 5) {
                        printf("Trying %s... failed\n", m->device);
                        meter_delete(m);
                        return NULL;
                }
        }

        printf("Trying %s... success\n", m->device);

        err = pthread_attr_init(&m->attr);
        if (err) {
                log_err("meter_new: pthread_attr_init failed");
                meter_delete(m);
                return NULL;
        }

        err = pthread_create(&m->thread, &m->attr, &meter_run, m);
        if (err) {
                log_err("meter_new: pthread_create failed");
                meter_delete(m);
                return NULL;
        }

        return m;
}

void meter_delete(meter_t* m)
{
        if (m == NULL) return;

        if (m->run) {
                m->run = 0;
                pthread_join(m->thread, NULL);
        }

        if (m->fd >= 0) {
                packet_internal_logging_600seconds(m->packet);
                meter_packet_write(m, m->packet);

                close(m->fd);
                m->fd = -1;
        }

        if (m->packet != NULL) {
                packet_delete(m->packet);
                m->packet = NULL;
        }

        if (m->device != NULL) {
                free(m->device);
                m->device = NULL;
        }

        free(m);
}

static int meter_packet_write(meter_t* m, packet_t* p) {
        bzero(p->buf, sizeof(p->buf));
        int n = sprintf(p->buf, "#%c,%c,%d", p->cmd, p->sub_cmd, p->field_count);
        char* s = p->buf + n;
        p->buf_len = n;

        for (int i = 0; i < p->field_count; i++) {
                if ((p->buf_len + strlen(p->fields[i]) + 4) >= sizeof(p->buf)) {
                        return -1;
                }

                n = sprintf(s, ",%s", p->fields[i]);
                s += n;
                p->buf_len += n;
        }

        p->buf[p->buf_len++] = ';';

        int pos = 0;
        while (pos < p->buf_len) {
                int res = write(m->fd, p->buf + pos, p->buf_len - pos);
                if (res < 0) {
                        return -1;
                }

                pos += res;
        }

        return 0;
}

static int meter_packet_read(meter_t* m, packet_t* p) {
        packet_init(p);

        while (1) {
                // Read as many new bytes as possible.
                fd_set read_set;
                FD_ZERO(&read_set);
                FD_SET(m->fd, &read_set);

                struct timeval tv;
                tv.tv_sec = 0;
                tv.tv_usec = 100000;

                int res = select(m->fd + 1, &read_set, 0, 0, &tv);
                if (res < 0) {
                        break;

                } else if (res == 0) {
                        break;
                }

                res = read(m->fd, m->buf + m->buf_len, sizeof(m->buf) - 1 - m->buf_len);
                if (res < 0) {
                        break;
                }

                if (res == 0) {
                        break;
                }

                m->buf_len += res;
                m->last_read_time = get_time();
        }

        if (m->buf_pos1 <= m->buf_pos0) {
                // Look for '#'.
                int found_sharp = 0;

                while (m->buf_pos0 < m->buf_len) {
                        if (m->buf[m->buf_pos0] == '#') {
                                found_sharp = 1;
                                break;
                        }

                        m->buf_pos0++;
                }

                if (!found_sharp) {
                        // Don't mind the useless characters.
                        m->buf_len = m->buf_pos0 = m->buf_pos1 = 0;
                        return -1;
                }

                m->buf_pos1 = m->buf_pos0 + 1;
        }

        {
                // Look for ';'.
                int found_semi_colon = 0;

                while (m->buf_pos1 < m->buf_len) {
                        if (m->buf[m->buf_pos1] == ';') {
                                found_semi_colon = 1;
                                break;
                        }

                        m->buf_pos1++;
                }

                if (!found_semi_colon) {
                        return -1;
                }
        }

        {
                // Copy message to packet.
                p->buf_len = m->buf_pos1 + 1 - m->buf_pos0;
                memcpy(p->buf, m->buf + m->buf_pos0, p->buf_len);
                p->buf[p->buf_len] = '\0';

                // Shift buf.
                m->buf_len -= m->buf_pos1 + 1;
                memmove(m->buf, m->buf + m->buf_pos1 + 1, m->buf_len);
                m->buf_pos0 = m->buf_pos1 = 0;
        }

        {
                // Parse packet.
                char* s = p->buf + 1;
                char* next = strchr(s, ',');
                if (next) {
                        p->cmd = *s;
                        s = ++next;
                } else {
                        // Invalid command field
                        return -1;
                }

                /*
                 * Next character is the subcommand, and should be '-'
                 * Though, it doesn't matter, because we just
                 * discard it anyway.
                 */
                next = strchr(s, ',');
                if (next) {
                        p->sub_cmd = *s;
                        s = ++next;
                } else {
                        // Invalid 2nd field
                        return -1;
                }

                /*
                 * Next is the number of parameters,
                 * which should always be > 0.
                 */
                next = strchr(s, ',');
                if (next) {
                        *next++ = '\0';
                        p->field_count = atoi(s);
                        s = next;
                } else {
                        // Couldn't determine number of parameters
                        return -1;
                }

                if (p->field_count <= 0) return -1;

                /*
                 * Now, we loop over the rest of the string,
                 * storing a pointer to each in p->fields[].
                 *
                 * The last character was originally a ';', but may have been
                 * overwritten with a '\0', so we make sure to catch
                 * that when converting the last parameter.
                 */
                for (int i = 0; i < p->field_count; i++) {
                        next = strpbrk(s, ",;");
                        if (next) {
                                *next++ = '\0';
                        } else {
                                if (i < (p->field_count - 1)) {
                                        // Malformed parameter string
                                        return -1;
                                }
                        }

                        /*
                         * Skip leading white space in fields
                         */
                        while (isspace(*s)) s++;
                        p->fields[i] = s;
                        if (next) s = next;
                }

                return 0;
        }
}

int meter_read_watts(meter_t* m)
{
        double t = get_time();

        while (get_time() - t < 1.2) {
                int err = meter_packet_read(m, m->packet);
                if (err) continue;

                if (m->packet->cmd == 'd' && m->packet->sub_cmd == '-' && m->packet->fields[0] != NULL) {
                        double watts = strtod(m->packet->fields[0], NULL) / 10.0;

                        if (m->last_energy_time > 0) {
                                double delta_t = m->last_read_time - m->last_energy_time;
                                m->energy += watts * delta_t;
                        }

                        m->last_energy_time = m->last_read_time;
                        return 0;
                }
        }

        return -1;
}

float meter_get_energy(meter_t* m)
{
        return m->energy;
}

void* meter_run(void* ptr)
{
        meter_t* m = (meter_t*) ptr;
        m->run = 1;
        int nerrors = 0;
        while (m->run && nerrors < 4) {
                int err = meter_read_watts(m);
                if (err) nerrors++;
        }
        printf("%s stopped\n", m->device);
        return NULL;
}

/* -------------------------------------------------*/

#define MAX_METERS 10

static meter_t* _meters[MAX_METERS];
static int _meters_count = 0;

meter_t* meters_get(int meter_id)
{
        if ((meter_id < 0) || (meter_id >= _meters_count))
                return NULL;
        return _meters[meter_id];
}

static int meters_add(meter_t* m)
{
        if (_meters_count >= MAX_METERS) {
                log_err("meters_add: maximum number of meters reached");
                return -1;
        }

        _meters[_meters_count++] = m;
        return 0;
}

int meters_init()
{
        for (int i = 0; i < MAX_METERS; i++)
                _meters[i] = NULL;

        for (int i = 0; i < MAX_METERS; i++) {
                meter_t* m = meter_new(i);
                if (m == NULL) {
                        continue;
                }

                if (meters_add(m) < 0) {
                        meter_delete(m);
                }
        }

        return meters_count() == 0;
}

int meters_fini()
{
        for (int i = 0; i < MAX_METERS; i++) {
                if (_meters[i] == NULL) continue;
                meter_delete(_meters[i]);
                _meters[i] = NULL;
        }

        return 0;
}

int meters_count()
{
        return _meters_count;
}

