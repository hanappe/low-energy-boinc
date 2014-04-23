#include <iostream>
#include <cstring>
#include "LibSensors.hpp"

#include <sensors/sensors.h>

using namespace std;

struct LibSensor : Sensor {

        const sensors_chip_name* m_chip;
        const sensors_feature* m_feature;
        const sensors_subfeature* m_subfeature;

        LibSensor(const sensors_chip_name* chip,
                  const sensors_feature* feature,
                  const sensors_subfeature* subfeature) {
                m_chip = chip;
                m_feature = feature;
                m_subfeature = subfeature;

                m_name = string(chip->prefix) + "-" + string(feature->name);
                m_description = (m_subfeature->type == SENSORS_SUBFEATURE_FAN_INPUT) ? "Fan" : "Temperature";

                char* label = sensors_get_label(chip, feature);
                if (strcmp(label, feature->name) != 0) {
                        m_description = label;
                }
                free(label);
        }

        void update(time_t t) {
                double value = 0;
                int err = sensors_get_value(m_chip, m_subfeature->number, &value);
                if (err != 0) return;
                if (m_subfeature->type == SENSORS_SUBFEATURE_FAN_INPUT && value == 0) return;
                m_datapoints.push_back(Datapoint(t, value));
        }

};

static const string ERRORS[]= {
#define ERROR_NO_ERROR 0
        "no error",
#define ERROR_SENSORS_INIT_FAILED 1
        "sensors_init failed",
};

struct LibSensorsManager : SensorManager {

        int m_error;
        bool m_error_reported;
        int m_update_period;
        time_t m_time;
        vector<LibSensor*> m_sensors;

        LibSensorsManager() {
                m_name = "LibSensorsManager";
                m_error = 0;
                m_error_reported = false;
                m_update_period = 5;
                m_time = 0;

                if (::sensors_init(NULL) != 0) {
                        m_error = ERROR_SENSORS_INIT_FAILED;
                }

                int chip_nr = 0;
                const sensors_chip_name* chip = 0;
                while ((chip = sensors_get_detected_chips(0, &chip_nr)) != 0) {
                        int feature_nr = 0;
                        const sensors_feature* feature = 0;
                        while ((feature = sensors_get_features(chip, &feature_nr)) != 0) {
                                int subfeature_nr = 0;
                                const sensors_subfeature* subfeature = 0;
                                while ((subfeature = sensors_get_all_subfeatures(chip, feature, &subfeature_nr)) != 0) {
                                        if (subfeature->type != SENSORS_SUBFEATURE_FAN_INPUT
                                            && subfeature->type != SENSORS_SUBFEATURE_TEMP_INPUT)
                                                continue;

                                        LibSensor* sensor = new LibSensor(chip, feature, subfeature);
                                        m_sensors.push_back(sensor);
                                }
                        }
                }
        }

        ~LibSensorsManager() {
                if (m_error) return;

                size_t nsensors = m_sensors.size();
                for (size_t i = 0; i < nsensors; i++) {
                        delete m_sensors[i];
                }

                ::sensors_cleanup();
        }

        void add_sensors(SensorV& sensors, ErrorV& errors) {
                if (m_error) {
                        if (!m_error_reported) {
                                errors.push_back(Error(__FILE__, m_error, ERRORS[m_error]));
                                m_error_reported = true;
                        }
                        return;
                }

                size_t nsensors = m_sensors.size();
                for (size_t i = 0; i < nsensors; i++) {
                        sensors.push_back(m_sensors[i]);
                }
        }

        void update_sensors() {
                if (m_error) return;

                time_t t = Datapoint::get_current_time();
                if (t < m_time + m_update_period) return;
                m_time = t;

                long rounded_t = (t / m_update_period) * m_update_period;

                size_t nsensors = m_sensors.size();
                for (size_t i = 0; i < nsensors; i++) {
                        m_sensors[i]->update(rounded_t);
                }
        }
};

static LibSensorsManager manager;

SensorManager* getLibSensorsManager() {
        return &manager;
}



