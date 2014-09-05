#include "BoincSensors.hpp"


#include <vector>
#include <unordered_map>
#include "boinc_api.h"
#include "gui_rpc_client.h"

#ifdef _WIN32
#include "win_util.h"
#endif



using namespace std;
// anonymous namespace
namespace
{
        // return true if string 's' contains a substring 'substr'
        bool contains(const std::string& s, std::string const & substr)
        {
                std::string::size_type n = s.find(substr);
                return (n != std::string::npos);
        }
}

struct ResultSensor : Sensor {
        string m_project_url;
        string m_result_name;
        bool m_updated;

        ResultSensor(RESULT* result) {
                // NOTE: do not keep the result pointer. It will
                // become obsolete after a while.
                m_project_url = result->project_url;
                m_result_name = result->name;
                m_updated = false;

                m_name = toOSDName(m_project_url + m_result_name);
                m_description = "workunit";

                //std::cout << "m_result_name: " <<  m_result_name << std::endl;
                //fileLog("m_result_name: " + m_result_name);
        }

        void update(time_t t, RESULT* result) {


                if (m_updated) return;                
                if (result->fraction_done > 0) {
                        //std::ostringstream oss;
                        //oss << "result->fraction_done: " << result->fraction_done;
                        //fileLog(oss.str());
                        m_datapoints.push_back(Datapoint(t, result->fraction_done));
                }


                /*for (int i=0; i<result->output_files.size(); i++) {
                  FILE_REF& fref = result->output_files[i];
                  fip = fref.file_info;
                  if (fip->uploaded) continue;
                  get_pathname(fip, path, sizeof(path));
                  retval = file_size(path, size);
                  if (retval) {
                  if (fref.optional) {
                  fip->upload_urls.clear();
                  continue;
                  }
                  
                  // an output file is unexpectedly absent.
                  //
                  fip->status = retval;
                  had_error = true;
                  msg_printf(
                  rp->project, MSG_INFO,
                  "Output file %s for task %s absent",
                  fip->name, rp->name
                  );
                  }
                  }*/

                m_updated = true;
        }

        static string toOSDName(const string& s) {
                string result;
                size_t start_i = s.find("http://") != 0 ? 0 : 7;
                for (size_t i = start_i; i < s.length(); i++) {
                        result += isalnum(s[i]) ? s[i] : '_';
                }
                return result;
        }

};

typedef unordered_map<string, ResultSensor*> StringToSensor;

static const string ERRORS[] = {
#define ERROR_NO_ERROR
        "no error",
#define ERROR_BOINC_NOT_RUNNING 1
        "BOINC not running",
#define ERROR_NO_BOINC_STATE 2
        "no BOINC state",
};



#ifdef _WIN32
namespace {
        static std::string get_current_dir() {
                
                std::string res;
                
                TCHAR Buffer[512] = {0};
                DWORD dwRet;
                
                dwRet = GetCurrentDirectory(512, Buffer);
                
                if( dwRet != 0 ) {
                        res = std::string(Buffer);
                } else {
                        std::cerr << "get_current_dir failed." <<  GetLastError() << endl;
                }
                
                return res;
		
	}

	static void set_current_dir(const std::string & dirpath) {
                //std::cerr << "set_current_dir:before: " << get_current_dir() << endl;
                if( !SetCurrentDirectory(dirpath.data()))
                        {
                                std::cerr << "set_current_dir failed." <<  GetLastError() << endl;
                                return;
				}
                
                //std::cerr << "set_current_dir:after: " << get_current_dir() << endl;
	}

}
#endif

struct BoincSensorsManager : SensorManager {

        int m_error;
        bool m_error_reported;

		int m_update_period;
        time_t m_update_time;

        RPC_CLIENT m_boinc_rpc;

        vector<ResultSensor*> m_sensors;
        StringToSensor m_stringToSensor;

		#ifdef _WIN32
			std::string m_app_path;
			std::string m_boinc_path;
		#endif

        BoincSensorsManager() {
                m_name = "BoincSensorsManager";
                m_error = 0;
                m_error_reported = false;
                m_update_period = 60;
                m_update_time = 0;

                
		#ifdef _WIN32
                //Save the current/initial directory (subdirectory in "slot")
                m_app_path = get_current_dir();
		
                //Change directory to AppData/BOINC (useful for RPC things) and save it
                chdir_to_data_dir();
                m_boinc_path = get_current_dir();
                #endif

				#if defined(_WIN32) && defined(USE_WINSOCK)

					WSADATA wsdata;
					int retval = WSAStartup( MAKEWORD( 1, 1 ), &wsdata);
					if (retval) {
						fprintf(stderr, "WinsockInitialize: %d\n", retval);
						exit(1);
					}
				#endif

                if (m_boinc_rpc.init("localhost") != 0) {
                        m_error = ERROR_BOINC_NOT_RUNNING;
                        //fileLog("ERROR_BOINC_NOT_RUNNING");
                        return;
                }
                #ifdef _WIN32
                
                setSlotDirectory();
                
                #endif
        }

        ~BoincSensorsManager() {
                for (size_t i = 0; i < m_sensors.size(); i++)
                        delete m_sensors[i];
                
                
                m_boinc_rpc.quit();
                
		#if defined(_WIN32) && defined(USE_WINSOCK)
                WSACleanup();
		#endif
        }

        void add_sensors(SensorV& sensors_out, ErrorV& errors) {
                if (m_error) {
                        if (!m_error_reported) {
                                errors.push_back(Error(__FILE__, m_error, ERRORS[m_error]));
                                m_error_reported = true;
                        }
                        m_error = 0;
                } else {
                        for (size_t i = 0; i < m_sensors.size(); i++)
                                sensors_out.push_back(m_sensors[i]);
                }
        }


        #ifdef _WIN32

        void setSlotDirectory() {
                set_current_dir(m_app_path);
        };
        
        void setBoincDirectory() {
                set_current_dir(m_boinc_path);
        };

        #endif

        void _update_sensors() {

                if (m_error) return;
                
                time_t t = Datapoint::get_current_time();
                if (t < m_update_time + m_update_period) return;
                m_update_time = t;
                time_t rounded_t = (t / m_update_period) * m_update_period;

                CC_STATE cc_state;
                if (m_boinc_rpc.get_state(cc_state) != 0) {
                        m_error = ERROR_NO_BOINC_STATE;
                        return;
                }

                // Manage known sensors.

                for (size_t i = 0; i < m_sensors.size(); i++) {
                        ResultSensor* sensor = m_sensors[i];
                        sensor->m_updated = false; // clear updated state
                        RESULT* result = cc_state.lookup_result(sensor->m_project_url.data(), sensor->m_result_name.data());
                        
						if (result != 0 && ResultIsRunning(result)) {
							sensor->update(rounded_t, result);
						}
                }

                // Check for new results.
                for (size_t i = 0; i < cc_state.results.size(); i++) {
                        RESULT* result = cc_state.results[i];
                        
						if (result == 0) {
							continue;
						}

                        ResultSensor key(result);
                        StringToSensor::const_iterator it = m_stringToSensor.find(key.m_name);
                        ResultSensor* sensor = 0;

                        if (it == m_stringToSensor.end()) {
                                sensor = new ResultSensor(result);
								if (ResultIsLowEnergyBoinc(sensor->m_project_url)) {
									delete sensor;
									
								} else {
									m_stringToSensor[sensor->m_name] = sensor;
									m_sensors.push_back(sensor);
								}
                        }
                        else {
                                sensor = it->second;
                        }

						if (ResultIsRunning(result)) {
							sensor->update(rounded_t, result);
						}
                }

                // TODO: check status of the result. Register it only if it is *running*.

                // Update sensor vector.
                vector<ResultSensor*> newSensors;
                for (size_t i = 0; i < m_sensors.size(); i++) {
                        ResultSensor* sensor = m_sensors[i];
                        if (!sensor->m_updated) {
                                // Result has disappeared. Delete sensor.
                                m_stringToSensor.erase(sensor->m_name);
                                delete sensor;
                                continue;
                        }

                        newSensors.push_back(sensor);
                }

                m_sensors = newSensors;
		}

        void update_sensors() {

				#ifdef _WIN32
						setBoincDirectory();
				#endif

				_update_sensors();

				#ifdef _WIN32
						setSlotDirectory();
				#endif
        }
		
		

		static bool ResultIsActive(RESULT* r) {
			return r->active_task;
		}

		static bool ResultIsRunning(RESULT* r) {
			return r->scheduler_state == CPU_SCHED_SCHEDULED;
		}

		static bool ResultIsLowEnergyBoinc(const std::string& project_url) {
			return  contains(project_url, "low-energy-boinc");
		}
};

/*
#endif // _WIN32
*/
static BoincSensorsManager * manager = 0;

SensorManager* getBoincSensorsManager() {

		if (!manager) {
				manager = new BoincSensorsManager;
		}

        return manager;
}

