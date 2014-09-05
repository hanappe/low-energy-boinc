#include "TEMPer.hpp"

using namespace std;

// Based on pcsensor.c by Juan Carlos Perez
// based on Temper.c by Robert Kavaler

#include <iostream>
#include <iomanip>
#include <vector>
#include <cstring>

#ifdef _WIN32
	//#include "libusb.h"
#else // Unix
	#include <usb.h>
#endif

static const bool debug = false;
static const bool close_each_time = true;

static const string ERRORS[] = {
#define ERROR_NO_ERROR 0
        "no error",
#define ERROR_DEVICE_SEARCH_ABORTED 1
        "device search aborted",
};

static char init0[] = { 0x01, 0x01 };
static char temperature[] = { 0x01, (char) 0x80, 0x33, 0x01, 0x00, 0x00, 0x00, 0x00 };
static char init1[] = { 0x01, (char) 0x82, 0x77, 0x01, 0x00, 0x00, 0x00, 0x00 };
static char init2[] = { 0x01, (char) 0x86, (char) 0xff, 0x01, 0x00, 0x00, 0x00, 0x00 };

#define VENDOR_ID  0x0c45
#define PRODUCT_ID 0x7401

#define CHECK_OR_CLOSE(x)                                               \
        if ((x) < 0) {                                                  \
                if (debug) cout << (void*) this << " " # x << endl;     \
                close(); return;                                        \
        }

#ifdef _WIN32


struct TEMPerManager : SensorManager {

        TEMPerManager() {
                m_name = "TEMPerManager";
        }

        ~TEMPerManager() {
           
        }

        void find_tempers() {
                
        }

        void close_tempers() {
        }

        void add_sensors(SensorV& sensors, ErrorV& errors) {
                
        }

        void update_sensors() {
				
        }
};


#else // Unix



struct TEMPerSensor : Sensor {

        struct usb_device* m_dev;
        struct usb_dev_handle* m_h;
        char m_read[8];

        TEMPerSensor(struct usb_device* dev) {
                cout << "TEMPerSensor()" << endl;
                m_name = "TEMPer";
                m_description = "Temperature sensor";
                m_dev = dev;
                m_h = 0;
                open();
                cout << "TEMPerSensor() End" << endl;
        }

        ~TEMPerSensor() {
                close();
        }

        void open() {
                if (is_open()) return;
                if (!check_vendor_and_product(m_dev)) return;

                m_h = usb_open(m_dev);
                if (m_h == 0) return;

#if LIBUSB_HAS_DETACH_KERNEL_DRIVER_NP
                usb_detach_kernel_driver_np(m_h, 0);
                usb_detach_kernel_driver_np(m_h, 1);
#endif // LIBUSB_HAS_DETACH_KERNEL_DRIVER_NP

                CHECK_OR_CLOSE(usb_set_configuration(m_h, 1));
                CHECK_OR_CLOSE(usb_claim_interface(m_h, 0));
                CHECK_OR_CLOSE(usb_claim_interface(m_h, 1));

                CHECK_OR_CLOSE(control_msg(0x0201, 0x00, init0, sizeof(init0)));
                CHECK_OR_CLOSE(control_msg(0x0200, 0x01, temperature, sizeof(temperature)));
                CHECK_OR_CLOSE(interrupt_read());

                CHECK_OR_CLOSE(control_msg(0x0200, 0x01, init1, sizeof(init1)));
                CHECK_OR_CLOSE(interrupt_read());

                CHECK_OR_CLOSE(control_msg(0x0200, 0x01, init2, sizeof(init2)));
                CHECK_OR_CLOSE(interrupt_read());
                CHECK_OR_CLOSE(interrupt_read());

                if (debug && m_h != 0) {
                        cout << "vendor id: 0x"
                             << setw(4) << setfill('0') << setbase(16)
                             << m_dev->descriptor.idVendor << ", "
                             << "product id: 0x"
                             << setw(4) << setfill('0') << setbase(16)
                             << m_dev->descriptor.idProduct
                             << setbase(10)
                             << endl;
                }
        }

        bool is_open() {
                return m_h != 0;
        }

        void close() {
                if (!is_open()) return;
                usb_release_interface(m_h, 0);
                usb_release_interface(m_h, 1);
                usb_close(m_h);
                m_h = 0;
        }

        void update(time_t t) {
                if (!is_open()) return;

                CHECK_OR_CLOSE(control_msg(0x0200, 0x01, temperature, sizeof(temperature)));
                CHECK_OR_CLOSE(interrupt_read());

                int value = (m_read[3] & 0xFF) + (m_read[2] << 8);
                double tempC = value * (125.0 / 32000.0);

                m_datapoints.push_back(Datapoint(t, tempC));

                if (close_each_time) close();
        }

        int control_msg(int value, int index, char *bytes, int size) {
                int res = usb_control_msg(m_h, 0x21, 0x09, value, index, bytes, size, 100);
                return (res != size) ? -1 : 0;
        }

        int interrupt_read() {
                memset(m_read, 0, sizeof(m_read));
                int res = usb_interrupt_read(m_h, 0x82, m_read, sizeof(m_read), 100);
                return (res != sizeof(m_read)) ? -1 : 0;
        }

        static bool check_vendor_and_product(struct usb_device* dev) {
                return (dev->descriptor.idVendor == VENDOR_ID &&
                        dev->descriptor.idProduct == PRODUCT_ID);
        }
};

struct TEMPerManager : SensorManager {

        int m_error;
        bool m_error_reported;
        int m_find_temper_attempts;
        int m_find_temper_max_attempts;
        TEMPerSensor* m_temper;
        int m_update_period;
        time_t m_update_time;

        TEMPerManager() {
                m_name = "TEMPerManager";
                m_error = 0;
                m_error_reported = false;
                m_find_temper_attempts = 0;
                m_find_temper_max_attempts = 3;
                m_update_period = 900; // 15 minutes
                m_update_time = 0;
                usb_init();
        }

        ~TEMPerManager() {
                close_tempers();
        }

        void find_tempers() {
                if (m_error) return;

                if (m_temper != 0) {
                        delete m_temper;
                        m_temper = 0;
                }

                m_find_temper_attempts++;
                if (!close_each_time && m_find_temper_attempts > m_find_temper_max_attempts) {
                        if (debug) cout << "ERROR_ABORTED_DEVICE_SEARCH" << endl;
                        m_error = ERROR_DEVICE_SEARCH_ABORTED;
                        return;
                }

                usb_find_busses();
                usb_find_devices();

                for (struct usb_bus* bus = usb_get_busses(); bus != 0; bus = bus->next) {  
                        for (struct usb_device* dev = bus->devices; dev != 0; dev = dev->next) {
                                if (!TEMPerSensor::check_vendor_and_product(dev)) continue;
                                
                                TEMPerSensor* temper = new TEMPerSensor(dev);
                                if (!temper->is_open()) {
                                        delete temper;
                                        continue;
                                }

                                m_temper = temper;
                                break;
                        }
                }
        }

        void close_tempers() {
                if (m_temper == 0) return;
                delete m_temper;
                m_temper = 0;
        }

        void add_sensors(SensorV& sensors, ErrorV& errors) {
                if (m_error) {
                        if (!m_error_reported) {
                                errors.push_back(Error(__FILE__, m_error, ERRORS[m_error]));
                                m_error_reported = true;
                        }
                        return;
                }

                if (m_temper == 0) return;
                sensors.push_back(m_temper);
        }

        void update_sensors() {
                //cout << "TEMPerManager::() update_sensors" << endl;
                time_t t = Datapoint::get_current_time();
                if (t < m_update_time + m_update_period) return;
                m_update_time = t;
                long rounded_t = (t / m_update_period) * m_update_period;
                
                
                if (m_temper == 0) {
                        find_tempers();
                        if (m_temper == 0) return;
                }

                if (!m_temper->is_open() && m_temper->m_datapoints.size() == 0) {
                        delete m_temper;
                        m_temper = 0;
                        find_tempers();
                        if (m_temper == 0) return;
                }

                m_temper->update(rounded_t);
        }
};

#endif

static TEMPerManager * manager = 0;

SensorManager* getTEMPerManager() {
        if (!manager) {
                manager = new TEMPerManager;
        }
        return manager;
}

