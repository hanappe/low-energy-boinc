#include "ArduinoTemp.hpp"

#include <iostream>
#include <cstring>
#include <sstream>

#ifdef _WIN32
	#include "serial.h"
	#include <windows.h>
	#define PORT_PREFIX_A "COM"
	#define PORT_PREFIX_B "\\\\.\\COM"
#else //Unix
	#define PORT_PREFIX_A "/dev/ttyUSB"
	#define PORT_PREFIX_B "/dev/ttyUSB"
#endif

static const bool debug = false;

static const std::string ERRORS[] = {
#define ERROR_NO_ERROR 0
        "no error",
#define ERROR_DEVICE_SEARCH_ABORTED 1
        "device search aborted",
};

using namespace std;

// An arduino packet in an array of unsigned char, the last char have to be '\0'
class ArduinoPacket : public std::vector<unsigned char> {

public:

	ArduinoPacket(const char * str) {
		*this << std::string(str);
	}
	ArduinoPacket(std::string& str) {
		*this << str;
	}

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

	friend ostream& operator<<(ostream& os , const ArduinoPacket& p) {
		
		for (ArduinoPacket::const_iterator it = p.begin(); it != p.end(); ++it) {
			os << (*it);
		}
		return  os;
	}

	static const ArduinoPacket sm_id_question; // "{{?}}"
	static const ArduinoPacket sm_temp_question; // "{{T}}"
};

const ArduinoPacket ArduinoPacket::sm_id_question = ArduinoPacket("{{?}}");
const ArduinoPacket ArduinoPacket::sm_temp_question = ArduinoPacket("{{T}}");

struct PacketParser : std::vector<char> {
	
	static const size_t MaxSize = 256;

	// Assuming Arduino answers is like {{data}}
	// With 'data' equals anything (256 byts max)
	enum State {
		BeginToken1, //Find the first {
		BeginToken2, //Find the second {
		EndToken1, //Find the first }
		EndToken2, //Find the second }
		ValueFound // Arduino data/value found
	};

	PacketParser() :
		s(BeginToken1)
	{}

	void processChar(const char c) {
		switch (s) {
		case BeginToken1:
			if (c == '{') {
				s = BeginToken2;
			}
			break;
		case BeginToken2:
			if (c == '{') {
				s = EndToken1;
			} else {
				init();
			}
			break;
		case EndToken1:
			if (c == '}') {
				s = EndToken2;
			} else {
				if (this->size() < MaxSize) {
					this->push_back(c);
				} else {
					init();
				}
			}
			break;
		case EndToken2:
			if (c == '}') {
				s = ValueFound;
			} else {
				init();
			}
			break;
		default:
			break;
		}
	}

	void init() {
		this->clear();
		s = BeginToken1;
	}

	bool hasValue() const {
		return s == ValueFound;
	}

	// Template. Specialization needed
	template <typename T> T getValue();
	
	template <>
	char getValue<char>() {
		return *(reinterpret_cast<const char*>(data()));
	}

	template <>
	float getValue<float>() {
		return *(reinterpret_cast<const float*>(data()));
	}

	template <>
	std::string getValue<std::string>() {
		return std::string(this->data(), this->size());
	}

	State s;
};


struct ArduinoTempSensor : Sensor {

		std::string m_device_port;
		bool m_closing;
		
		serial::Serial m_serial;

		ArduinoTempSensor(std::string& port)
		{
				m_name = "arduinotemp";
				m_description = "Get (external) temperature from Arduino device";
			
				m_device_port = port;	
				m_closing = false;

				if (!open())
					return;

				//this->fileLog("Device found ! [" + m_device_port + "]");
        }

        ~ArduinoTempSensor(){
			close();
			
		}

		bool open() {
			bool device_opened = false;

			if (isOpen()) {
				if (debug)
					std::cout << "serial already opened" << std::endl;
			} else {
				m_serial.setPort(m_device_port);
				m_serial.setBaudrate(9600);
				m_serial.setTimeout(serial::Timeout::simpleTimeout(2000));
				
				m_serial.open();

				// Open port
				if (m_serial.isOpen()) {
					// Check if the device is well an Arduino
					if (identify()) {
						m_serial.flush();
						device_opened = true;
					} else {
						m_serial.close();
					}
				} 
			}

			return device_opened;

				
		}

		bool identify() {
				bool identified = false;

				// Questions device
				if (debug)
					std::cout << "Send Id Question: " << ArduinoPacket::sm_id_question << std::endl;

				sendPacket( ArduinoPacket::sm_id_question );


				// Get an answer
				PacketParser m;
				if (readAnswer(m)) {
					if (debug)
						std::cout << "[ArduinoSensor] Identification answer: " <<  m.getValue<char>() << std::endl;
							
					char c = m.getValue<char>();
					identified = (c == 'A');
				}

				return identified;
				
		}

        bool isOpen() {
                return m_serial.isOpen();
        }

        void close() {
				
                if (m_closing) return;
                m_closing = true;

                if (m_serial.isOpen()) {
						m_serial.flush();
                        m_serial.close();
                }	 
                
                m_closing = false;

				//this->fileLog("Device closed ! [" + m_device_port + "]");
        }

        void sendPacket(const ArduinoPacket& p) {

				if (!isOpen()) {
					return;
				}

				try {
						m_serial.write(p.data(), p.size());
				} catch (...) {
					
						close();
						
						if (debug) {
								std::cout << "[device] writing error, device closed" << std::endl;
						}
				}
        }

		bool readAnswer(PacketParser & m) {

			if (!isOpen()) {
				return false;
			}

			m.init();

			uint8_t c;

			try {
				
				while (m_serial.read(&c, 1)) {
				
					if (debug) {
						std::cout << "read: " << c << " : " << (int)c << std::endl;
					}

					m.processChar(c);

					if (m.hasValue()) {
						break;
					}

				}

			} catch (...) {
					
					m_serial.flush();
					m_serial.close();
					if (debug) {
							std::cout << "[device] reading error, device closed" << std::endl;
					}
			}

			return m.hasValue();

		}
        
        virtual void update(time_t t) {
        
			if (debug)
				std::cout << "Send Temp Question: " << ArduinoPacket::sm_temp_question << std::endl;

            // Temp auestions
            sendPacket( ArduinoPacket::sm_temp_question );

            // Get an answer
			PacketParser m;
			if (readAnswer(m)) {
				if (debug)
					std::cout << "[ArduinoSensor] External temperature: " << m.getValue<float>() << std::endl;
			}
                
        }	
};



struct ArduinoTempManager : SensorManager {
        
        int m_error;
        bool m_error_reported;

		ArduinoTempSensor* m_arduino_temp;

		int m_update_period;
        time_t m_update_time;

		int m_find_device_period;
        time_t m_find_device_time;

        int m_find_device_attempts;
        int m_find_device_max_attempts;

        ArduinoTempManager() {
                m_name = "ArduinoTempManager";
                m_error = 0;
                m_error_reported = false;

				m_arduino_temp = 0;	

                m_update_period = 5;
                m_update_time = 0;

				m_find_device_period = debug ? 10 : 60; // Try to find a device each x second
                m_find_device_time = 0;
        }

        ~ArduinoTempManager() {
                if (m_error) return;
                
                if (m_arduino_temp) {
                        delete m_arduino_temp;
                        m_arduino_temp = 0;
                }
        }

		
        static ArduinoTempSensor * FindArduinoTemp() {

				if (debug) {
					std::cout << "ArduinoTempSensor()"<< std::endl;
				}


				ArduinoTempSensor * sensor = 0;

				for (int i = 0; i < 16; i++)
				{
                        ostringstream ss;
						ss << ((i < 10) ? PORT_PREFIX_A : PORT_PREFIX_B) << i;

                        ArduinoTempSensor* arduino = new ArduinoTempSensor(ss.str());
                        if (!arduino->isOpen()) {
                                delete arduino;
    
                        } else {
							if (debug) {
								std::cout << "Arduino found !"<< std::endl;
							}
							sensor = arduino;
							break;
						}
                       
                }
				return sensor;
        }
        
        
        void add_sensors(SensorV& sensors_out, ErrorV& errors) {
                if (m_error) {
                        if (!m_error_reported) {
                                errors.push_back(Error(__FILE__, m_error, ERRORS[m_error]));
                                m_error_reported = true;
                        }
                        return;
                } else {
					if (m_arduino_temp) {
							sensors_out.push_back(m_arduino_temp);
					}
				}
                
        }
        
        void update_sensors() {
                if (m_error) return;
               
				time_t t = Datapoint::get_current_time();
                if (t < m_update_time + m_update_period) return;
                m_update_time = t;


                if (!m_arduino_temp) {
						
						// If Wattsup time is elapsed, try to find Wattsup device
						time_t t2 = Datapoint::get_current_time();
						if (t2 > m_find_device_time + m_find_device_period) {
							// Update the reference time
							m_find_device_time = t2;
							m_arduino_temp = FindArduinoTemp();
						}

						if (!m_arduino_temp) {
							return;
						}
                }

				if (m_arduino_temp) {
					if (m_arduino_temp->m_datapoints.size() > 0) {
                        m_find_device_attempts = 0;
					}
					else if (!m_arduino_temp->isOpen()) {
						delete m_arduino_temp;
						m_arduino_temp = 0;
						//find_wattsups();
						m_arduino_temp = FindArduinoTemp();
						if (!m_arduino_temp) {
						return;
						}
					}
                }

                m_arduino_temp->update(m_update_time);
        }
};


static ArduinoTempManager * manager;

SensorManager* getArduinoTempManager() {

		if (!manager) {
				manager = new ArduinoTempManager;
		}

        return manager;
}
