#include "ArduinoTemp.hpp"

#include <iostream>
#include <cstring>
#include <sstream>

#include "serial.h"

static const bool debug = true;

using namespace std;

#ifdef _WIN32
#include <windows.h>
	#define PORT_PREFIX_A "COM"
	#define PORT_PREFIX_B "\\\\.\\COM"
#else //Unix
	#define PORT_PREFIX_A "/dev/ttyUSB"
	#define PORT_PREFIX_B "/dev/ttyUSB"
#endif


// An arduino packet in an array of unsigned char, the last char have to be '\0'
class ArduinoPacket : public std::vector<unsigned char> {

public:
	ArduinoPacket& operator<<(unsigned char c) {
		push_back(c);
		return *this;
	}
	ArduinoPacket& operator<<(const std::string& str) {
		size_t s = str.size(); 
		size_t i = 0;
		while (i <= s ) { // Yep "<=". we need the '/0' char
			//if (debug)
				//std::cout << "ArduinoPacket& operator: " << static_cast<unsigned char>(str[i]) << " : " << (int)static_cast<unsigned char>(str[i]) << std::endl;
			
			push_back(static_cast<unsigned char>(str[i]));
			++i;
		}
		return *this;
	}

	static const ArduinoPacket sm_id_question; // '?'
	static const ArduinoPacket sm_id_answer; // 'A'

	static const ArduinoPacket sm_temp_question; // T
private:

	static ArduinoPacket CreateCommad(std::string& s) {
		ArduinoPacket p;
		p << s;
		return p;
	}
};

const ArduinoPacket ArduinoPacket::sm_id_question = ArduinoPacket::CreateCommad(std::string("?")); // Complete Command is "?\0"
const ArduinoPacket ArduinoPacket::sm_id_answer = ArduinoPacket::CreateCommad(std::string("A"));; // Complete Command is "A\0"

const ArduinoPacket ArduinoPacket::sm_temp_question = ArduinoPacket::CreateCommad(std::string("T"));; // Complete Command is "T\0"
//ArduinoPacket ArduinoPacket::sm_temp_answer = ArduinoPacket::CreateCommad(std::string("?"));;

struct ArduinoTempSensor : Sensor {

		enum Flag {
			Undefine,
			Arduino,
			NoArduino
		};

		std::string m_port;
		serial::Serial m_serial;

		bool m_closing;
		Flag m_flag;

		 ArduinoTempSensor(std::string& port) {
				//cout <<  "ArduinoTempSensor" << endl;
				m_description = "Get (external) temperature from Arduino device";
			
				m_port = port;	
				m_closing = false;
				m_flag = Undefine;

				open();
				if (is_open()) {
					identify();
				}
        }

        ~ArduinoTempSensor(){
			if (m_serial.isOpen()) {
				m_serial.close();
			}
		}

		void identify() {
				if (m_flag == Undefine) {
						// Questions device
						std::cout << "Send Question" << std::endl;
						sendPacket( ArduinoPacket::sm_id_question );

						std::cout << "Get Answer" << std::endl;
						// Get the answer
						ArduinoPacket p;
						readPacket(p);
				
						m_flag = (p == ArduinoPacket:: sm_id_answer) ? Arduino : NoArduino;
				}
		}

		bool is_arduino() const {
			return m_flag == Arduino;
		}

		void open() {

				//std::cout << "Arduino open ?" << std::endl;
			if (is_open()) {
				return;
			}              
				
				m_serial.setPort(m_port);
				m_serial.setBaudrate(9600);
				serial::Timeout t(1000, 1000);
				m_serial.setTimeout(t);
				//m_serial.setTimeout(serial::Timeout(11111));
				m_serial.setBytesize(serial::eightbits);
				m_serial.setParity(serial::parity_none);
				m_serial.setStopbits(serial::stopbits_one);
				
				m_serial.open();

				if (!m_serial.isOpen()) {
                        m_serial.close();
						close();
                        return;
                }
		}

        bool is_open() {
				return m_serial.isOpen();
        }

        void close() {
				
                if (m_closing) return;
                m_closing = true;

				if (m_serial.isOpen()) {
					m_serial.close();
                }	 

                m_closing = false;
        }

		void sendPacket(const ArduinoPacket& p) {
			m_serial.write(p.data(), p.size());
		}

		//Read string answer
		void readPacket(ArduinoPacket& p) {

			//ostringstream ss;
			unsigned char c = 1;
			int i = 0;

			p.clear();

			while(m_serial.read(&c, 1) > 0) {
				if (debug)
					std::cout << "c: " << (int)c << " : " << c <<  std::endl;
				p << c;

				if (c == '\0') {
					break;
				}
			}
		}

		void readFloat(float & f) {
			unsigned char data[5] = {0};
			m_serial.read(&data[0], 5);

			if (debug) {
				std::cout << "readFloat: " << (int)data[0] << ":" << (int)data[1] << ":" << (int)data[2] << ":" << (int)data[3] << std::endl;
			}

			if (data[4] == '\0') {
				f = *(reinterpret_cast<const float*>(&data[0]));
			}
		}

        virtual void update(time_t t) {
			if (is_arduino()) {

				// Questions device
				sendPacket( ArduinoPacket::sm_temp_question );

				// Get the answer
				float f;
				readFloat(f);
				std::cout << "External temperature: " << f << std::endl;

			}
		}

		
};

static const string ERRORS[] = {
#define ERROR_NO_ERROR 0
        "no error",
#define ERROR_NO_ACPI_SUPPORT 1
        "no ACPI support",
};

struct ArduinoTempManager : SensorManager {

        int m_error;
        bool m_error_reported;
        int m_update_period; // Second
        time_t m_time;
        ArduinoTempSensor* m_arduino_temp_sensor;

        ArduinoTempManager() {
                m_name = "ArduinoTempManager";
                m_error = 0;
                m_error_reported = false;
                m_update_period = 5;
                m_time = 0;
				m_arduino_temp_sensor = 0;	
        }

        ~ArduinoTempManager() {
                if (m_error) return;

				if (m_arduino_temp_sensor) {
					delete m_arduino_temp_sensor;
					m_arduino_temp_sensor = 0;
				}
        }

		ArduinoTempSensor* get_arduino_sensor() {

			ArduinoTempSensor* result = 0;
			for (int i = 0; i <= 32; ++i)

			{
                    ostringstream ss;
					ss << ((i < 10) ? PORT_PREFIX_A : PORT_PREFIX_B) << i;

                    ArduinoTempSensor* device_sensor = new ArduinoTempSensor(ss.str());
                       //std::cout << "Port test: " << ss.str() << std::endl;
					if (device_sensor->is_open() && device_sensor->is_arduino()) {
						std::cout << "Arduino FOUND A!: " << ss.str()<< std::endl;
                        result =  device_sensor;
                    } else {
						delete device_sensor;
					}
            }

			if (debug && !result) {
				std::cout << "Arduino Not found.... " << result <<std::endl;
			}

			return result;
		}

        void add_sensors(SensorV& sensors, ErrorV& errors) {
                if (m_error) {
                        if (!m_error_reported) {
                                errors.push_back(Error(__FILE__, m_error, ERRORS[m_error]));
                                m_error_reported = true;
                        }
                        return;
                }

				if (m_arduino_temp_sensor) {
					sensors.push_back(m_arduino_temp_sensor);
				}

        }

        void update_sensors() {
                if (m_error) return;

                time_t t = Datapoint::get_current_time();
                if (t < m_time + m_update_period) return;
                m_time = t;

                long rounded_t = (t / m_update_period) * m_update_period;
				
				
				if (!m_arduino_temp_sensor) {
					m_arduino_temp_sensor = get_arduino_sensor();
				}
				
				if (m_arduino_temp_sensor) {
					m_arduino_temp_sensor->update(rounded_t);
				}
        }
};

static ArduinoTempManager manager;

SensorManager* getArduinoTempManager() {
        return &manager;
}