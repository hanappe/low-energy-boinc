#include "Wattsup.hpp"

#include <iostream>
#include <sstream>
#include <vector>
#include <deque>

#include "serial.h"


using namespace std;

static const bool debug = true;

static const string ERRORS[] = {
#define ERROR_NO_ERROR 0
        "no error",
#define ERROR_DEVICE_SEARCH_ABORTED 1
        "device search aborted",
};

struct Packet {
        static const size_t buf_len = 512;
        static const size_t num_fields = 18;

		char m_cmd;
		char m_sub_cmd;

		char m_buf[buf_len];
		int m_len;

		char* m_fields[num_fields];
		int m_count;

        Packet() {
                m_cmd = m_sub_cmd = 0;
                m_len = m_count = 0;
				memset(m_buf, 0, buf_len);
				memset(m_fields, 0, num_fields);
        }
}; // end class Packet

struct WattsupCommand {
        Packet m_p;
        time_t m_write_time;
        char m_wait_reply;

        WattsupCommand() {
                m_write_time = 0;
                m_wait_reply = 0;
        }

		// Version request
		static void SetupVersionRequestion(WattsupCommand & c) {
			    c.m_p.m_cmd = 'V';
                c.m_p.m_sub_cmd = 'R';
                c.m_p.m_count = 0;
                c.m_wait_reply = 'v';
		}

		// Request fields
		static void SetupRequestField(WattsupCommand & c) {
				c.m_p.m_cmd = 'C';
                c.m_p.m_sub_cmd = 'W';
                c.m_p.m_count = 18;
                c.m_p.m_fields[0] = (char*) "1"; // watts
                c.m_p.m_fields[1] = (char*) "0"; // volts
                c.m_p.m_fields[2] = (char*) "0"; // amps
                c.m_p.m_fields[3] = (char*) "0"; // watt hours
                c.m_p.m_fields[4] = (char*) "0"; // cost
                c.m_p.m_fields[5] = (char*) "0"; // mo. kWh
                c.m_p.m_fields[6] = (char*) "0"; // mo. cost
                c.m_p.m_fields[7] = (char*) "0"; // max watts
                c.m_p.m_fields[8] = (char*) "0"; // max volts
                c.m_p.m_fields[9] = (char*) "0"; // max amps
                c.m_p.m_fields[10] = (char*) "0"; // min watts
                c.m_p.m_fields[11] = (char*) "0"; // min volts
                c.m_p.m_fields[12] = (char*) "0"; // min amps
                c.m_p.m_fields[13] = (char*) "0"; // power factor
                c.m_p.m_fields[14] = (char*) "0"; // duty cycle
                c.m_p.m_fields[15] = (char*) "0"; // power cycle
                c.m_p.m_fields[16] = (char*) "0"; // line frequency Hz
                c.m_p.m_fields[17] = (char*) "0"; // volt-amps
                c.m_wait_reply = 'n';
		}

		// Memory full handling
		static void SetupMemoryFullHandeling(WattsupCommand & c) {
				c.m_p.m_cmd = 'O';
				c.m_p.m_sub_cmd = 'W';
				c.m_p.m_count = 1;
				c.m_p.m_fields[0] = (char*) "1";
		}

		// External logging 1s
		static void SetupExternalLogging1s(WattsupCommand & c) {
			    c.m_p.m_cmd = 'L';
                c.m_p.m_sub_cmd = 'W';
                c.m_p.m_count = 3;
                c.m_p.m_fields[0] = (char*) "E";
                c.m_p.m_fields[1] = (char*) "";
                c.m_p.m_fields[2] = (char*) "1";
		}
		// Internal logging 600s
		static void SetupInternalLogging600s(Packet & p) {
			    p.m_cmd = 'L';
				p.m_sub_cmd = 'W';
				p.m_count = 3;
				p.m_fields[0] = (char*) "I";
				p.m_fields[1] = (char*) "";
				p.m_fields[2] = (char*) "600";
		}
}; // end class WattsupCommand

typedef deque<WattsupCommand> CommandQ;

#ifdef _WIN32

#include <windows.h>

#define PORT_PREFIX "\\\\.\\COM"

struct WattsupSensor : Sensor {

        string m_device;
        //int m_fd;
        char m_buf[Packet::buf_len];
        int m_buf_len;
        int m_buf_pos0;
        int m_buf_pos1;
        time_t m_last_read_time;
        CommandQ m_commands;
        bool m_closing;
        double m_last_watts;

		serial::Serial m_serial;

        WattsupSensor(const string& device) {
				//std::cout << "WattsupSensor()" << std::endl;
                m_name = "wattsup";
                m_description = device;
                m_device = device;
                //m_fd = -1;
                m_buf_len = 0;
                m_buf_pos0 = 0;
                m_buf_pos1 = 0;
                m_last_read_time = 0;
                m_closing = false;
                m_last_watts = -1;

                open();
                if (!is_open()) return;

                if (debug) {
                        // Version request
                        WattsupCommand c;
						WattsupCommand::SetupVersionRequestion(c);
                        m_commands.push_back(c);
                }

                {
                        // Request fields
                        WattsupCommand c;
                        WattsupCommand::SetupRequestField(c);
                        m_commands.push_back(c);
                }

                {
                        // Memory full handling
                        WattsupCommand c;
                        WattsupCommand::SetupMemoryFullHandeling(c);
                        m_commands.push_back(c);
                }

                {
                        // External logging 1s
                        WattsupCommand c;
						WattsupCommand::SetupExternalLogging1s(c);
                        m_commands.push_back(c);
                }
        }

        ~WattsupSensor() {
                close();
        }

        void open() {
				std::cout << "WattsupSensor::open" << std::endl;
                if (is_open()) return;

                m_buf_len = 0;
                m_closing = false;
				
				m_serial.setPort(m_device);
				m_serial.setBaudrate(115200);
				//serial::Timeout t(1000, 1000);
				//m_serial.setTimeout(t);
				m_serial.open();

				if (m_serial.isOpen() && identify()) {
					// Wattsup is identify
					std::cout << "WattsupSensor::open end success ?" << std::endl;
				} else {
					m_serial.close();
                    close();
					std::cout << "WattsupSensor:: close A" << std::endl;
                    return;
				}

				
        }

		// Send 

		bool identify() {
			
			WattsupCommand cmd;
			WattsupCommand::SetupVersionRequestion(cmd);

			time_t t = Datapoint::get_current_time();
            write(cmd.m_p);
            cmd.m_write_time = t;

			Packet p;

			// Waiting for the reply 'v'

			while (!read(p) || p.m_cmd != cmd.m_wait_reply) {
					t = Datapoint::get_current_time();
					
                    if (t > cmd.m_write_time + 5) {
                            // Reply timed out.
                            close();
                            return false;
                    }
					Sleep(16);
            }

			return true;
		}

        bool is_open() {
				return m_serial.isOpen();
        }

        void close() {
				//std::cout << "WattsupSensor::close()" << std::endl; 
                if (m_closing) return;
                m_closing = true;

				if (is_open()) {
                        // Internal logging 600s
                        Packet p;
						WattsupCommand::SetupInternalLogging600s(p);

						write(p);
                        
						m_serial.close();
                }	 

                m_buf_len = 0;
                m_commands.clear();
                m_closing = false;
        }

        bool write(Packet& p) {
                memset(p.m_buf, 0, sizeof(p.m_buf));
                int n = sprintf(p.m_buf, "#%c,%c,%d", p.m_cmd, p.m_sub_cmd, p.m_count);
                char* s = p.m_buf + n;
                p.m_len = n;

                for (int i = 0; i < p.m_count; i++) {
                        if ((p.m_len + strlen(p.m_fields[i]) + 4) >= sizeof(p.m_buf)) {
                                return false;
                        }

                        n = sprintf(s, ",%s", p.m_fields[i]);
                        s += n;
                        p.m_len += n;
                }

                p.m_buf[p.m_len++] = ';';

                if (debug) {
                        cout << "[DEBUG-WRITE]: " << m_device << ": "
                             << p.m_buf << endl;
                }

                int pos = 0;
                while (pos < p.m_len) {
						int res = m_serial.write((const uint8_t *)p.m_buf + pos, p.m_len - pos);
						if (res < 0) {
                                return false;
                        }

                        pos += res;
                }

                return true;
        }

        bool read(Packet& p) {
                while (true) {
						
						int res = 0;
						size_t available = m_serial.available();
						if (!available) {
							break;
						} else {
							res = m_serial.read((uint8_t*)m_buf + m_buf_len, available);
						}
						
                        if (res <= 0) {
                                break;
                        }

                        m_buf_len += res;
                        m_last_read_time = Datapoint::get_current_time();

                }

                if (m_buf_pos1 <= m_buf_pos0) {
                        // Look for '#'.
                        bool found_sharp = false;

                        while (m_buf_pos0 < m_buf_len) {
                                if (m_buf[m_buf_pos0] == '#') {
                                        found_sharp = true;
                                        break;
                                }

                                m_buf_pos0++;
                        }

                        if (!found_sharp) {
                                // Don't mind the useless characters.
                                m_buf_len = m_buf_pos0 = m_buf_pos1 = 0;
                                return false;
                        }

                        m_buf_pos1 = m_buf_pos0 + 1;
                }

                {
                        // Look for ';'.
                        bool found_semi_colon = false;

                        while (m_buf_pos1 < m_buf_len) {
                                if (m_buf[m_buf_pos1] == ';') {
                                        found_semi_colon = true;
                                        break;
                                }

                                m_buf_pos1++;
                        }

                        if (!found_semi_colon) {
                                return false;
                        }
                }

                {
                        // Copy message to packet.
                        p.m_len = m_buf_pos1 + 1 - m_buf_pos0;
                        memcpy(p.m_buf, m_buf + m_buf_pos0, p.m_len);
                        p.m_buf[p.m_len] = '\0';

                        // Shift m_buf.
                        m_buf_len -= m_buf_pos1 + 1;
                        memmove(m_buf, m_buf + m_buf_pos1 + 1, m_buf_len);
                        m_buf_pos0 = m_buf_pos1 = 0;
                }

                {
                        // Parse packet.
                        char* s = p.m_buf + 1;
                        char* next = strchr(s, ',');
                        if (next) {
                                p.m_cmd = *s;
                                s = ++next;
                        } else {
                                // Invalid command field
                                return false;
                        }

                        /*
                         * Next character is the subcommand, and should be '-'
                         * Though, it doesn't matter, because we just
                         * discard it anyway.
                         */
                        next = strchr(s, ',');
                        if (next) {
                                p.m_sub_cmd = *s;
                                s = ++next;
                        } else {
                                // Invalid 2nd field
                                return false;
                        }

                        /*
                         * Next is the number of parameters,
                         * which should always be > 0.
                         */
                        next = strchr(s, ',');
                        if (next) {
                                *next++ = '\0';
                                p.m_count = atoi(s);
                                s = next;
                        } else {
                                // Couldn't determine number of parameters
                                return false;
                        }

                        if (p.m_count <= 0) return false;

                        /*
                         * Now, we loop over the rest of the string,
                         * storing a pointer to each in p.m_fields[].
                         *
                         * The last character was originally a ';', but may have been
                         * overwritten with a '\0', so we make sure to catch
                         * that when converting the last parameter.
                         */
                        for (int i = 0; i < p.m_count; i++) {
                                next = strpbrk(s, ",;");
                                if (next) {
                                        *next++ = '\0';
                                } else {
                                        if (i < (p.m_count - 1)) {
                                                // Malformed parameter string
                                                return false;
                                        }
                                }

                                /*
                                 * Skip leading white space in fields
                                 */
                                while (isspace(*s)) s++;
                                p.m_fields[i] = s;
                                if (next) s = next;
                        }

                        if (debug) {
                                cout << "[DEBUG-READ]: " << m_device << ": "
                                     << '#'
                                     << p.m_cmd << ','
                                     << p.m_sub_cmd << ','
                                     << p.m_count;

                                for (int i = 0; i < p.m_count; i++) {
                                        cout << ',' << p.m_fields[i];
                                }

                                cout << ';' << endl;
                        }

                        return true;
                }
        }

        void update() {
				//std::cout << "WattsupSensor::update" << std::endl;
				
                if (!is_open()) return;

                Packet p;

                while (!m_commands.empty()) {
                        WattsupCommand& command = m_commands.front();
                        time_t t = Datapoint::get_current_time();

                        if (command.m_write_time == 0) {
                                // Send command.
								//std::cout << "Send command" << std::endl;
                                write(command.m_p);
                                command.m_write_time = t;
                        }

                        if (command.m_wait_reply == '\0') {
                                // No need to wait for a reply.
                                m_commands.pop_front();
                                continue;
                        }

                        if (t > command.m_write_time + 5) {
                                // Reply timed out.
								//std::cout << "Reply timed out" << std::endl;
                                m_commands.pop_front();
                                close();
                                return;
                        }

                        if (read(p) && p.m_cmd == command.m_wait_reply) {
                                // Reply received. Proceed forward.
								//std::cout << "Reply received. Proceed forward." << std::endl;
                                m_commands.pop_front();
                                continue;
                        }

                        return;
                }

                double watts = -1;

                while (read(p)) {
                        if (p.m_cmd == 'd' && p.m_sub_cmd == '-') {
                                watts = strtod(p.m_fields[0], NULL) / 10.0;
                        }
                }

                time_t t = Datapoint::get_current_time();

                if (watts != -1) {
						//std::cout << "Datapoint(t, watts): " << t << " : " << watts << std::endl;
                        m_datapoints.push_back(Datapoint(t, watts));
                        m_last_watts = watts;
                }

                if (t > m_last_read_time + 5) {
                        //m_datapoints.push_back(Datapoint(t, NAN));
                        close();
                }
        }

};


struct WattsupManager : SensorManager {

        int m_error;
        bool m_error_reported;
        WattsupSensor* m_wattsup;
        int m_find_wattsups_period;
        time_t m_find_wattsups_time;
        int m_find_wattsups_attempts;
        int m_find_wattsups_max_attempts;
        int m_update_period;
        time_t m_update_time;

        WattsupManager() {
                m_name = "WattsupManager";
                m_error = 0;
                m_error_reported = false;
				m_wattsup = 0; //NULL
                m_find_wattsups_period = 10;
                m_find_wattsups_time = 0;
                m_find_wattsups_attempts = 0;
                m_find_wattsups_max_attempts = 3;
                m_update_period = 1;
                m_update_time = 0;
        }

        ~WattsupManager() {
                close_wattsups();
        }

        void find_wattsups() {
                if (m_error) return;

                time_t t = Datapoint::get_current_time();
                if (t < m_find_wattsups_time + m_find_wattsups_period) return;
                m_find_wattsups_time = t;

                if (m_wattsup != 0) {
                        delete m_wattsup;
                        m_wattsup = 0;
                }

				for (int i = 0; i < 16; i++)
				{
                        ostringstream ss;
                        ss << PORT_PREFIX << i;

                        WattsupSensor* wattsup = new WattsupSensor(ss.str());
                        if (!wattsup->is_open()) {
                                delete wattsup;
    
                        } else {
							m_wattsup = wattsup;
							break;
						}
                       
                }
				
				// Unix
                // An opened ttyUSB might not be a valid Wattsup.
                // Don't reset m_find_wattsups_attempts here.

                m_find_wattsups_attempts++;
                if (false && m_find_wattsups_attempts >= m_find_wattsups_max_attempts) {
                        if (debug) cout << "ERROR_ABORTED_DEVICE_SEARCH" << endl;
                        m_error = ERROR_DEVICE_SEARCH_ABORTED;
                }
        }

        void close_wattsups() {
                if (m_wattsup == 0) return;
                delete m_wattsup;
                m_wattsup = 0;
        }

        void add_sensors(SensorV& sensors, ErrorV& errors) {
                if (m_error) {
                        if (!m_error_reported) {
                                errors.push_back(Error(__FILE__, m_error, ERRORS[m_error]));
                                m_error_reported = true;
                        }
                        return;
                }

                if (m_wattsup == 0) return;
                sensors.push_back(m_wattsup);
        }

        void update_sensors() {
                time_t t = Datapoint::get_current_time();
                if (t < m_update_time + m_update_period) return;
                m_update_time = t;

                if (m_wattsup == 0) {
                        find_wattsups();
                        if (m_wattsup == 0) return;
                }

                if (m_wattsup->m_datapoints.size() > 0) {
                        m_find_wattsups_attempts = 0;
                }
                else if (!m_wattsup->is_open()) {
                        delete m_wattsup;
                        m_wattsup = 0;
                        find_wattsups();
                        if (m_wattsup == 0) return;
                }

                m_wattsup->update();
        }
};


#else // !_WIN32


#define PORT_PREFIX "/dev/ttyUSB"

#include <cmath>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

struct WattsupSensor : Sensor {

        string m_device;
        int m_fd;
        char m_buf[Packet::buf_len];
        int m_buf_len;
        int m_buf_pos0;
        int m_buf_pos1;
        time_t m_last_read_time;
        CommandQ m_commands;
        bool m_closing;
        double m_last_watts;

        WattsupSensor(const string device) {
                m_name = "wattsup";
                m_description = device;
                m_device = device;
                m_fd = -1;
                m_buf_len = 0;
                m_buf_pos0 = 0;
                m_buf_pos1 = 0;
                m_last_read_time = 0;
                m_closing = false;
                m_last_watts = -1;

                open();
                if (!is_open()) return;

                if (debug) {
                        // Version request
                        WattsupCommand c;
                        c.m_p.m_cmd = 'V';
                        c.m_p.m_sub_cmd = 'R';
                        c.m_p.m_count = 0;
                        c.m_wait_reply = 'v';
                        m_commands.push_back(c);
                }

                {
                        // Request fields
                        WattsupCommand c;
                        c.m_p.m_cmd = 'C';
                        c.m_p.m_sub_cmd = 'W';
                        c.m_p.m_count = 18;
                        c.m_p.m_fields[0] = (char*) "1"; // watts
                        c.m_p.m_fields[1] = (char*) "0"; // volts
                        c.m_p.m_fields[2] = (char*) "0"; // amps
                        c.m_p.m_fields[3] = (char*) "0"; // watt hours
                        c.m_p.m_fields[4] = (char*) "0"; // cost
                        c.m_p.m_fields[5] = (char*) "0"; // mo. kWh
                        c.m_p.m_fields[6] = (char*) "0"; // mo. cost
                        c.m_p.m_fields[7] = (char*) "0"; // max watts
                        c.m_p.m_fields[8] = (char*) "0"; // max volts
                        c.m_p.m_fields[9] = (char*) "0"; // max amps
                        c.m_p.m_fields[10] = (char*) "0"; // min watts
                        c.m_p.m_fields[11] = (char*) "0"; // min volts
                        c.m_p.m_fields[12] = (char*) "0"; // min amps
                        c.m_p.m_fields[13] = (char*) "0"; // power factor
                        c.m_p.m_fields[14] = (char*) "0"; // duty cycle
                        c.m_p.m_fields[15] = (char*) "0"; // power cycle
                        c.m_p.m_fields[16] = (char*) "0"; // line frequency Hz
                        c.m_p.m_fields[17] = (char*) "0"; // volt-amps
                        c.m_wait_reply = 'n';
                        m_commands.push_back(c);
                }

                {
                        // Memory full handling
                        WattsupCommand c;
                        c.m_p.m_cmd = 'O';
                        c.m_p.m_sub_cmd = 'W';
                        c.m_p.m_count = 1;
                        c.m_p.m_fields[0] = (char*) "1";
                        m_commands.push_back(c);
                }

                {
                        // External logging 1s
                        WattsupCommand c;
                        c.m_p.m_cmd = 'L';
                        c.m_p.m_sub_cmd = 'W';
                        c.m_p.m_count = 3;
                        c.m_p.m_fields[0] = (char*) "E";
                        c.m_p.m_fields[1] = (char*) "";
                        c.m_p.m_fields[2] = (char*) "1";
                        m_commands.push_back(c);
                }
        }

        ~WattsupSensor() {
                close();
        }

        void open() {
                if (is_open()) return;

                m_buf_len = 0;
                m_closing = false;
                m_fd = ::open(m_device.data(), O_RDWR | O_NONBLOCK);

                if (m_fd < 0) {
                        close();
                        return;
                }

                tcflush(m_fd, TCIOFLUSH);

                struct termios m_termios;
                int err = tcgetattr(m_fd, &m_termios);
                if (err) {
                        close();
                        return;
                }

                cfmakeraw(&m_termios);
                cfsetispeed(&m_termios, B115200);
                cfsetospeed(&m_termios, B115200);
                m_termios.c_iflag |= IGNPAR;
                m_termios.c_cflag &= ~CSTOPB;

                err = tcsetattr(m_fd, TCSANOW, &m_termios);
                if (err) {
                        close();
                        return;
                }
        }

        bool is_open() {
                return m_fd >= 0;
        }

        void close() {
                if (m_closing) return;
                m_closing = true;

                if (m_fd >= 0) {
                        // Internal logging 600s
                        Packet p;
                        p.m_cmd = 'L';
                        p.m_sub_cmd = 'W';
                        p.m_count = 3;
                        p.m_fields[0] = (char*) "I";
                        p.m_fields[1] = (char*) "";
                        p.m_fields[2] = (char*) "600";
                        write(p);
                        ::close(m_fd);
                        m_fd = -1;
                }

                m_buf_len = 0;
                m_commands.clear();
                m_closing = false;
        }

        bool write(Packet& p) {
                memset(p.m_buf, 0, sizeof(p.m_buf));
                int n = sprintf(p.m_buf, "#%c,%c,%d", p.m_cmd, p.m_sub_cmd, p.m_count);
                char* s = p.m_buf + n;
                p.m_len = n;

                for (int i = 0; i < p.m_count; i++) {
                        if ((p.m_len + strlen(p.m_fields[i]) + 4) >= sizeof(p.m_buf)) {
                                return false;
                        }

                        n = sprintf(s, ",%s", p.m_fields[i]);
                        s += n;
                        p.m_len += n;
                }

                p.m_buf[p.m_len++] = ';';

                if (debug) {
                        cout << m_device << ": "
                             << p.m_buf << endl;
                }

                int pos = 0;
                while (pos < p.m_len) {
                        int res = ::write(m_fd, p.m_buf + pos, p.m_len - pos);
                        if (res < 0) {
                                return false;
                        }

                        pos += res;
                }

                return true;
        }

        bool read(Packet& p) {
                while (true) {
                        // Read as many new bytes as possible.
                        fd_set read_set;
                        FD_ZERO(&read_set);
                        FD_SET(m_fd, &read_set);

                        struct timeval tv;
                        tv.tv_sec = 0;
                        tv.tv_usec = 0;

                        int res = select(m_fd + 1, &read_set, 0, 0, &tv);
                        if (res < 0) {
                                break;

                        } else if (res == 0) {
                                break;
                        }

                        res = ::read(m_fd, m_buf + m_buf_len, sizeof(m_buf) - 1 - m_buf_len);
                        if (res < 0) {
                                break;
                        }

                        if (res == 0) {
                                break;
                        }

                        m_buf_len += res;
                        m_last_read_time = Datapoint::get_current_time();
                }

                if (m_buf_pos1 <= m_buf_pos0) {
                        // Look for '#'.
                        bool found_sharp = false;

                        while (m_buf_pos0 < m_buf_len) {
                                if (m_buf[m_buf_pos0] == '#') {
                                        found_sharp = true;
                                        break;
                                }

                                m_buf_pos0++;
                        }

                        if (!found_sharp) {
                                // Don't mind the useless characters.
                                m_buf_len = m_buf_pos0 = m_buf_pos1 = 0;
                                return false;
                        }

                        m_buf_pos1 = m_buf_pos0 + 1;
                }

                {
                        // Look for ';'.
                        bool found_semi_colon = false;

                        while (m_buf_pos1 < m_buf_len) {
                                if (m_buf[m_buf_pos1] == ';') {
                                        found_semi_colon = true;
                                        break;
                                }

                                m_buf_pos1++;
                        }

                        if (!found_semi_colon) {
                                return false;
                        }
                }

                {
                        // Copy message to packet.
                        p.m_len = m_buf_pos1 + 1 - m_buf_pos0;
                        memcpy(p.m_buf, m_buf + m_buf_pos0, p.m_len);
                        p.m_buf[p.m_len] = '\0';

                        // Shift m_buf.
                        m_buf_len -= m_buf_pos1 + 1;
                        memmove(m_buf, m_buf + m_buf_pos1 + 1, m_buf_len);
                        m_buf_pos0 = m_buf_pos1 = 0;
                }

                {
                        // Parse packet.
                        char* s = p.m_buf + 1;
                        char* next = strchr(s, ',');
                        if (next) {
                                p.m_cmd = *s;
                                s = ++next;
                        } else {
                                // Invalid command field
                                return false;
                        }

                        /*
                         * Next character is the subcommand, and should be '-'
                         * Though, it doesn't matter, because we just
                         * discard it anyway.
                         */
                        next = strchr(s, ',');
                        if (next) {
                                p.m_sub_cmd = *s;
                                s = ++next;
                        } else {
                                // Invalid 2nd field
                                return false;
                        }

                        /*
                         * Next is the number of parameters,
                         * which should always be > 0.
                         */
                        next = strchr(s, ',');
                        if (next) {
                                *next++ = '\0';
                                p.m_count = atoi(s);
                                s = next;
                        } else {
                                // Couldn't determine number of parameters
                                return false;
                        }

                        if (p.m_count <= 0) return false;

                        /*
                         * Now, we loop over the rest of the string,
                         * storing a pointer to each in p.m_fields[].
                         *
                         * The last character was originally a ';', but may have been
                         * overwritten with a '\0', so we make sure to catch
                         * that when converting the last parameter.
                         */
                        for (int i = 0; i < p.m_count; i++) {
                                next = strpbrk(s, ",;");
                                if (next) {
                                        *next++ = '\0';
                                } else {
                                        if (i < (p.m_count - 1)) {
                                                // Malformed parameter string
                                                return false;
                                        }
                                }

                                /*
                                 * Skip leading white space in fields
                                 */
                                while (isspace(*s)) s++;
                                p.m_fields[i] = s;
                                if (next) s = next;
                        }

                        if (debug) {
                                cout << m_device << ": "
                                     << '#'
                                     << p.m_cmd << ','
                                     << p.m_sub_cmd << ','
                                     << p.m_count;

                                for (int i = 0; i < p.m_count; i++) {
                                        cout << ',' << p.m_fields[i];
                                }

                                cout << ';' << endl;
                        }

                        return true;
                }
        }

        void update() {
                if (!is_open()) return;

                Packet p;

                while (!m_commands.empty()) {
                        WattsupCommand& command = m_commands.front();
                        time_t t = Datapoint::get_current_time();

                        if (command.m_write_time == 0) {
                                // Send command.
                                write(command.m_p);
                                command.m_write_time = t;
                        }

                        if (command.m_wait_reply == '\0') {
                                // No need to wait for a reply.
                                m_commands.pop_front();
                                continue;
                        }

                        if (t > command.m_write_time + 5) {
                                // Reply timed out.
                                m_commands.pop_front();
                                close();
                                return;
                        }

                        if (read(p) && p.m_cmd == command.m_wait_reply) {
                                // Reply received. Proceed forward.
                                m_commands.pop_front();
                                continue;
                        }

                        return;
                }

                double watts = -1;

                while (read(p)) {
                        if (p.m_cmd == 'd' && p.m_sub_cmd == '-') {
                                watts = strtod(p.m_fields[0], NULL) / 10.0;
                        }
                }

                time_t t = Datapoint::get_current_time();

                if (watts != -1) {
                        m_datapoints.push_back(Datapoint(t, watts));
                        m_last_watts = watts;
                }

                if (t > m_last_read_time + 5) {
                        //m_datapoints.push_back(Datapoint(t, NAN));
                        close();
                }
        }

};


struct WattsupManager : SensorManager {

        int m_error;
        bool m_error_reported;
        WattsupSensor* m_wattsup;
        int m_find_wattsups_period;
        time_t m_find_wattsups_time;
        int m_find_wattsups_attempts;
        int m_find_wattsups_max_attempts;
        int m_update_period;
        time_t m_update_time;

        WattsupManager() {
                m_name = "WattsupManager";
                m_error = 0;
                m_error_reported = false;
                m_find_wattsups_period = 10;
                m_find_wattsups_time = 0;
                m_find_wattsups_attempts = 0;
                m_find_wattsups_max_attempts = 3;
                m_update_period = 1;
                m_update_time = 0;
        }

        ~WattsupManager() {
                close_wattsups();
        }

        void find_wattsups() {
                if (m_error) return;

                time_t t = Datapoint::get_current_time();
                if (t < m_find_wattsups_time + m_find_wattsups_period) return;
                m_find_wattsups_time = t;

                if (m_wattsup != 0) {
                        delete m_wattsup;
                        m_wattsup = 0;
                }

                for (int i = 0; i < 10; i++) {
                        ostringstream ss;
                        ss << PORT_PREFIX << i;

                        WattsupSensor* wattsup = new WattsupSensor(ss.str());
                        if (!wattsup->is_open()) {
                                delete wattsup;
                                continue;
                        }

                        m_wattsup = wattsup;
                        break;
                }

                // An opened ttyUSB might not be a valid Wattsup.
                // Don't reset m_find_wattsups_attempts here.

                m_find_wattsups_attempts++;
                if (false && m_find_wattsups_attempts >= m_find_wattsups_max_attempts) {
                        if (debug) cout << "ERROR_ABORTED_DEVICE_SEARCH" << endl;
                        m_error = ERROR_DEVICE_SEARCH_ABORTED;
                }
        }

        void close_wattsups() {
                if (m_wattsup == 0) return;
                delete m_wattsup;
                m_wattsup = 0;
        }

        void add_sensors(SensorV& sensors, ErrorV& errors) {
                if (m_error) {
                        if (!m_error_reported) {
                                errors.push_back(Error(__FILE__, m_error, ERRORS[m_error]));
                                m_error_reported = true;
                        }
                        return;
                }

                if (m_wattsup == 0) return;
                sensors.push_back(m_wattsup);
        }

        void update_sensors() {
                time_t t = Datapoint::get_current_time();
                if (t < m_update_time + m_update_period) return;
                m_update_time = t;

                if (m_wattsup == 0) {
                        find_wattsups();
                        if (m_wattsup == 0) return;
                }

                if (m_wattsup->m_datapoints.size() > 0) {
                        m_find_wattsups_attempts = 0;
                }
                else if (!m_wattsup->is_open()) {
                        delete m_wattsup;
                        m_wattsup = 0;
                        find_wattsups();
                        if (m_wattsup == 0) return;
                }

                m_wattsup->update();
        }
};

#endif // _WIN32

static WattsupManager manager;

SensorManager* getWattsupManager() {
        return &manager;
}

