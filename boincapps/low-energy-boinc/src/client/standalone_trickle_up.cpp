#include "standalone_trickle_up.h"

#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <map>
#include <iomanip>

// Boinc headers

#include "boinc_api.h"
#include "parse.h"
#include "miofile.h"
#include "error_numbers.h"
#include "str_replace.h"

// Curl

#include <curl/curl.h>

// Own headers

#include "json/json.h"

// KRC key
// TODO read the key from a text file (example : "standalone.key")

using namespace std;


static const bool Debug = true;

// TEMP variable for testing
// TODO load this data from file

unsigned int AccountId = 46;
std::string OsdKey = std::string("af538a92-ed4a-4675-9dc7-0a77c7e88b61");
std::string GroupName = "groupname";


namespace {

        template <class T>
        void append(vector<T>& dest, vector<T>& src) {
                for (size_t i = 0; i < src.size(); i++)
                        dest.push_back(src[i]);
        }
        
        string my_escape_string(const string& str) {
                ostringstream ss;
                
                size_t len = str.length();
                for (size_t i = 0; i < len; i++) {
                        char c = str[i];
                        if (c == '\'') ss << '\\' << '\'';
                        else if (c == '\\') ss << '\\' << '\\';
                        else ss << c;
                }
                
                return ss.str();
        }


        #ifdef _WIN32

        #define isnan _isnan

        static struct tm *
        localtime_r (const time_t *timer, struct tm *result) {
                localtime_s(result, timer);
                return result;
        }

        #endif // _WIN32


	void print_format(std::ostream& os, const time_t& t) {
		struct tm tm;
                localtime_r(&t, &tm);
                
		os << std::setw(4) << std::setfill('0') << (1900 + tm.tm_year)
                   << '-' << std::setw(2) << std::setfill('0') << tm.tm_mon + 1
                   << '-' << std::setw(2) << std::setfill('0') << tm.tm_mday
                   << 'T' << std::setw(2) << std::setfill('0') << tm.tm_hour
                   << ':' << std::setw(2) << std::setfill('0') << tm.tm_min
                   << ':' << std::setw(2) << std::setfill('0') << tm.tm_sec;
	}

	time_t get_current_time() {
		return time(NULL);
	}

        std::string to_string_with_underscore(const time_t& t) {
                struct tm tm;
                localtime_r(&t, &tm);
                
                ostringstream os;
		os << std::setw(4) << std::setfill('0') << (1900 + tm.tm_year)
                   << '-' << std::setw(2) << std::setfill('0') << tm.tm_mon + 1
                   << '-' << std::setw(2) << std::setfill('0') << tm.tm_mday
                   << '-' << std::setw(2) << std::setfill('0') << tm.tm_hour
                   << '-' << std::setw(2) << std::setfill('0') << tm.tm_min
                   << '-' << std::setw(2) << std::setfill('0') << tm.tm_sec;
                return os.str();
        }


bool LoadFromFile(const string& filename, ostringstream& oss) {
        ifstream file;
        file.open(filename.c_str(), ifstream::in);
        
        
        if(!file.good()) {
                std::cerr << "file open error: " << filename << " file error.\n";
                file.close();
                return false;
        }

        
        //move at end if good, move at begin, if it is good, file is good.
        std::streamsize size;
        if (file.seekg(0, std::ios::end).good()) size = file.tellg();
        if (file.seekg(0, std::ios::beg).good()) size -= file.tellg();
        
        if (!file.good()) {
                if (file.fail()) {
                        std::cerr << filename << " file.fail().\n";
                } else if (file.eof()) {
                        std::cerr << filename << " file.eof().\n";
                } else if (file.bad()) {
                        std::cerr << filename << " file.bad().\n";
                }
                
                file.close();
                return false;
        }
        
        std::string line;
        while(std::getline(file, line, '\n')) {
                oss << line;
        }

        return true;
}

bool SaveToFile(const std::string& filename, const std::string& data) {
        std::ofstream file(filename.c_str(), std::ios::out | std::ios::binary);
        
        if (!file.good()) {
                if (file.fail()) {
                        std::cerr << filename << " file.fail().\n";
                } else if (file.eof()) {
                        std::cerr << filename << " file.eof().\n";
                } else if (file.bad()) {
                        std::cerr << filename << " file.bad().\n";
                }
                
                file.close();
                return false;
        }
 
        file << data; // "Writing this to a file.\n";

        file.close();
        return true;
}


} // end anonymous namespace


struct Point {
        std::string date;
        std::string value;
};


struct Datapoints : public std::vector<Point> {
};

// SensorData = Datastream info
struct SensorData {

        static const int unknown_index = -1;
        static const int unknown_osd_id = -1;

        int index;
        int datastream_id;
        string name;
        string description;

        Datapoints datapoints;

        SensorData() {
                index = unknown_index;
                datastream_id = unknown_osd_id;
        }

        ~SensorData() {
        }

        void print_json(ostream& os) const {
                os << "{";

                if (idIsValid()) {
                        os << "\"id\":" << datastream_id << ",";
                }

                os << "\"name\":\"" << name << "\","
                   << "\"description\":\"" << description << "\"}";
        }

        void print_full_json(ostream& os) const {
                os << "{";
                os << "\"index\":" << index << ",";
                os << "\"id\":" << datastream_id << ",";
                os << "\"name\":\"" << name << "\",";
                os << "\"description\":\"" << description << "\",";

                os << "\"values\":";

                os << "[" << std::endl;;
                
                size_t size = datapoints.size();
                size_t last = size - 1;
                for (size_t i = 0; i < size; ++i) {
                        const Point& p = datapoints[i];
                        

                        os << "\t"<< "{";
                        os << "\"date\":\"" << p.date << "\",";
                        os << "\"value\":\"" << p.value << "\"";
                        os << "}";

                        if (i < last) {
                                os << "," << std::endl;                                
                        }
                }
                os << std::endl << "]";

                os << "}";
                
        }

        void print(ostream& os) const {
                os << "[SensorData:";
                os << " index: " << index;
                os << " datastream_id: " << datastream_id;;
                os << " name: " << name;
                os << " description: " << description << "]";
        }

        bool idIsValid() const {
                return datastream_id > SensorData::unknown_osd_id;
        }

        bool indexIsValid() const {
                return index > SensorData::unknown_index;;
        }
};

typedef vector<SensorData> Sensors;

////////////////////////////////////////////////////////////////////////////////
// Curl
////////////////////////////////////////////////////////////////////////////////

struct CurlEasy {

        CURL* curl;
        curl_slist* header_lines;
        stringstream outgoing_data;
        stringstream ingoing_data;
        long response_code;
        long sent;
        long received;

        CurlEasy() {
                curl = curl_easy_init();
                if (curl == 0) cout << " * ERROR: curl_easy_init" << endl;
                header_lines = 0;
        }

        ~CurlEasy() {
                if (header_lines != 0) {
                        curl_slist_free_all(header_lines);
                        header_lines = 0;
                }

                if (curl != 0) {
                        curl_easy_cleanup(curl);
                        curl = 0;
                }
        }

        void append_header_line(string line) {
                header_lines = curl_slist_append(header_lines, line.data());
        }

        CURLcode upload_to(string url) {
                if (header_lines != 0) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_lines);
                curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
                curl_easy_setopt(curl, CURLOPT_PUT, 1L);
                curl_easy_setopt(curl, CURLOPT_URL, url.data());
                curl_easy_setopt(curl, CURLOPT_READFUNCTION, stringstream_read_function);
                curl_easy_setopt(curl, CURLOPT_READDATA, &outgoing_data);
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, stringstream_write_function);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ingoing_data);
                curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t) outgoing_data.tellp());
		//curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

                CURLcode res = curl_easy_perform(curl);
                if (res != CURLE_OK) return res;

                response_code = -1;
                res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
                if (res != CURLE_OK) return res;

                sent = -1;
                res = curl_easy_getinfo(curl, CURLINFO_SIZE_UPLOAD, &sent);
                if (res != CURLE_OK) return res;

                received = -1;
                res = curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD, &received);
                if (res != CURLE_OK) return res;

                return res;
        }

        static size_t stringstream_read_function(void *ptr, size_t size, size_t nmemb, void *stream) {
                return ((stringstream*) stream)->readsome((char*) ptr, size * nmemb);
        }

        static size_t stringstream_write_function(void *ptr, size_t size, size_t nmemb, void *stream) {
                ((stringstream*) stream)->write((char*) ptr, size * nmemb);
                return size * nmemb;
        }

};


namespace Config {

        bool IsInitialized = false;
        std::string Key; // OSD Key
        std::map<std::string, int> DatastreamIndexMap;

        void Print() {
                std::cout << "[Config: " << std::endl;
                std::cout << "osd key: " << Key << std::endl;
                std::map<std::string, int>::const_iterator it = DatastreamIndexMap.begin();
                std::map<std::string, int>::const_iterator end = DatastreamIndexMap.end();
                for (; it != end; ++it) {
                        std::cout << it->first << ": " << it->second << '\n';
                }
                std::cout << "]" << std::endl;
        }

        bool Load() {
                std::string json;
                // Load from file
                {                
                        static std::string config_filename("config.json");
                        std::ostringstream oss;

                        if (!LoadFromFile(config_filename, oss)) {
                                std::cerr << " * Error: LoadFromFile" << std::endl;
                                return false;
                        }

                        json = oss.str();
                }
                
                json_parser_t* json_parser = json_parser_create();
                if (!json_parser) {
                        cout << " * ERROR: json_parser_create" << endl;
                        return false;
                }
                
                json_object_t group = json_parser_eval(json_parser, json.data());
                if (!json_parser_done(json_parser)) {
                        cout << " * ERROR: json_parser error: " << json_parser_errstr(json_parser) << endl;
			cout << " == BEGIN JSON ==" << endl;
			cout << json << endl;
			cout << " == END JSON ==" << endl;
                        return false;
                }
                
                json_parser_destroy(json_parser);

                
                { // get key block
                        json_object_t account_key = json_object_get(group, "account_key");
                        if (!json_isstring(account_key)) {
                                json_unref(group);
                                cout << " * ERROR: no key string in JSON object" << endl;
                                return false;
                        }

                        Key = json_string_value(account_key);
                } // end get key block

                { // get datastreams block
                        json_object_t datastreams = json_object_get(group, "datastreams");
                        if (!json_isarray(datastreams)) {
                                json_unref(group);
                                cout << " * ERROR: no datastreams array in JSON object" << endl;
                                return false;
                        }
                        
                        const size_t array_len = json_array_length(datastreams);
                
                        for (size_t i = 0; i < array_len; i++) {
                                json_object_t datastream = json_array_get(datastreams, i);
                                if (!json_isobject(datastream)) {
                                        cout << " * ERROR: datastream " << i << " is not a JSON object" << endl;
                                        json_unref(group);
                                        return false;
                                }
                                
                                json_object_t datastream_id = json_object_get(datastream, "id");                                
                                
                                int id = -1;
                                // FIXME: check OSD server so that ids are
                                // always integers or always strings, not
                                // sometimes one, sometimes the other.
                                if (json_isnumber(datastream_id)) {
                                        id = json_number_value(datastream_id);
                                } else if (json_isstring(datastream_id)) {
                                        id = atoi(json_string_value(datastream_id));
                                } else {
                                        std::cerr << " * ERROR: datastream " << i << " 'id' can't be parsed" << std::endl;
                                        json_unref(group);
                                        return false;
                                }
                                
                                json_object_t datastream_name = json_object_get(datastream, "name");
                                if (!json_isstring(datastream_name)) {
                                        cout << " * ERROR: datastream " << i << " 'name' string" << endl;
                                json_unref(group);
                                return false;
                                }
                                
                                std::string name = json_string_value(datastream_name);
                                
                                DatastreamIndexMap[name] = id;

                        } // end get datastreams block
 
                } // end for

                //Print();

                return true;
        
        }

}; // end namespace Config



// TODO turn it to singleton (?)
struct SensorHandler {

        static const std::string JsonFilename;
        std::string json;



        //Config config;

        SensorHandler() {
        }

        void load_json() {
                
                std::cout << "load_json begin" << std::endl;

                ostringstream oss;
                if (LoadFromFile(JsonFilename, oss)) {
                        json = oss.str();                
                }

                std::cout << "load_json end" << std::endl;
        }

        void save_json() {
                std::cout << "save_json begin" << std::endl;

                SaveToFile(JsonFilename, json);

                std::cout << "save_json end" << std::endl;
        }

public:

        // Change sensor id to id defined in the config file
        void change_datastream_id(Sensors& sensors) {

                //const std::map<std::string, int>& map = config.datastream_index_map;
                const std::map<std::string, int>& map = Config::DatastreamIndexMap;


                Sensors::iterator it = sensors.begin();
                Sensors::iterator end = sensors.end();
                for (; it != end ; ++it) {
                        SensorData& sensor = (*it);
                        std::map<std::string, int>::const_iterator map_it = map.find(sensor.name);
                        if (map_it != map.end()) {
                                sensor.datastream_id = map_it->second;    
                        }
                }
        }

        Sensors parse_sensors() {
                Sensors no_sensors;
                Sensors sensors;
                
                json_parser_t* json_parser = json_parser_create();
                if (!json_parser) {
                        cout << " * ERROR: json_parser_create" << endl;
                        return no_sensors;
                }
                
                json_object_t group = json_parser_eval(json_parser, json.data());
                if (!json_parser_done(json_parser)) {
                        cout << " * ERROR: json_parser error: " << json_parser_errstr(json_parser) << endl;
			cout << " == BEGIN JSON ==" << endl;
			cout << json << endl;
			cout << " == END JSON ==" << endl;
                        return no_sensors;
                }
                
                json_parser_destroy(json_parser);

                json_object_t datastreams = json_object_get(group, "datastreams");
                if (!json_isarray(datastreams)) {
                        json_unref(group);
                        cout << " * ERROR: no datastreams array in JSON object" << endl;
                        return no_sensors;
                }
                
                for (int i = 0; i < json_array_length(datastreams); i++) {
                        SensorData sensor;
                        sensor.index = -1;
                        
                        json_object_t datastream = json_array_get(datastreams, i);
                        if (!json_isobject(datastream)) {
                                cout << " * ERROR: datastream " << i << " is not a JSON object" << endl;
                                json_unref(group);
                                return no_sensors;
                        }

                        json_object_t name = json_object_get(datastream, "name");
                        if (!json_isstring(name)) {
                                cout << " * ERROR: datastream " << i << " has no 'name' string" << endl;
                                json_unref(group);
                                return no_sensors;
                        }
                        sensor.name = json_string_value(name);


                        json_object_t desc = json_object_get(datastream, "description");
                        if (!json_isstring(desc)) {
                                cout << " * ERROR: datastream " << i << " has no 'description' string" << endl;
                                json_unref(group);
                                return no_sensors;
                        }
                        sensor.description = json_string_value(desc);

                        json_object_t datastream_id = json_object_get(datastream, "id");

                        // FIXME: check OSD server so that ids are
                        // always integers or always strings, not
                        // sometimes one, sometimes the other.
                        if (json_isnumber(datastream_id)) {
                                sensor.datastream_id = json_number_value(datastream_id);
                        } else if (json_isstring(datastream_id)) {
                                sensor.datastream_id = atoi(json_string_value(datastream_id));
                        } else {
                                sensor.datastream_id = SensorData::unknown_osd_id;
                        }

                        sensors.push_back(sensor);
                }

                json_unref(group);
                return sensors;
        }
                
        void update_json(Sensors& sensors) {
                ostringstream oss;

                oss << "{\"name\":\"" << GroupName << "\","
                    << "\"description\":\"lowenergyboinc\","
                    << "\"datastreams\":[";

                for (size_t i = 0; i < sensors.size(); i++) {
                        if (i > 0) {
                                oss << ',';
                        }
                        sensors[i].print_json(oss);
                }

                oss << "]}";

                json = oss.str();
                
                // TODO save json in file
        }

        // Test
        // TODO: Remove this

        static void UploadDatapoint(std::string& key) {
                CurlEasy curl;        
                //curl.outgoing_data << sensor.osd_id << "," << date << "," << value << endl;
                curl.outgoing_data << 8874 << "," << "2014-08-22 17:48:20"  << "," << 666 << endl;
                if (curl.outgoing_data.tellp() <= 0) {
                        cout << " * ERROR: no datapoints in the trickle message" << endl;
                        return;
                }
                curl.append_header_line("Content-Type: text/csv");
                curl.append_header_line(string("X-OpenSensorData-Key: ") + key);
                curl.upload_to("http://opensensordata.net/upload/");
                
                if (curl.response_code != 200L) {
                        cout << " * ERROR: OSD datapoints upload failed: " << curl.response_code << endl;
                        cout << " == RESPONSE BEGIN ==" << endl;
                        cout << curl.ingoing_data.str();
                        cout << " == RESPONSE END ====" << endl;
                        return;
                }
        }

        static bool manage_sensor(SensorData& sensor) {
                // Anything but BOINC workunit sensors.
                return sensor.description.find("workunit") != 0;
        }
        
        int manage_trickle_sensors(Sensors& trickle_sensors, map<string, int>& name_to_index, const string& osd_key) {

                // Parse "loaded" (from data base, on group object creation) sensors
                Sensors parsed_sensors = parse_sensors();
                Sensors old_sensors;
                Sensors pending_sensors;
                Sensors new_sensors;
                bool need_update = false;

                for (size_t i = 0; i < parsed_sensors.size(); i++) {
                        cout << "manage_trickle_sensors: C0 " << endl;
                        SensorData& sensor = parsed_sensors[i];
                        if (!manage_sensor(sensor)) {
                                // Some old sensor is not managed anymore by this group.
                                need_update = true;
                                continue;
                        }
                        
                        map<string, int>::iterator it = name_to_index.find(sensor.name);
                        if (it == name_to_index.end()) {
                                // Known sensor does not appear in the trickle message.
                                if (!sensor.idIsValid()) {
                                        // Still need to get an id.
                                        need_update = true;
                                        pending_sensors.push_back(sensor);
                                } else {
                                        // Old sensor with an id.
                                        old_sensors.push_back(sensor);
                                }
                                
                        } else {
                                // Known sensor appears in the trickle message.
                                sensor.index = it->second;
                                SensorData& sensor_ = trickle_sensors[it->second];
                                sensor_.datastream_id = sensor.datastream_id;
                                
                                if (!sensor_.idIsValid()) {
                                        // Managed as new sensor below.
                                        continue;
                                }
                        
                                if (sensor_.description.compare(sensor.description) == 0) {
                                        // Description was not updated.
                                        old_sensors.push_back(sensor);
                                } else {
                                        // Description needs an update.
                                        need_update = true;
                                        pending_sensors.push_back(sensor_);
                                }
                        }
                        cout << "manage_trickle_sensors: C100 " << endl;
                }
                
                cout << "manage_trickle_sensors: D" << endl;
                for (size_t i = 0; i < trickle_sensors.size(); i++) {
                        SensorData& sensor = trickle_sensors[i];
                        if (!manage_sensor(sensor)) continue;
                        if (!sensor.idIsValid()) {
                                need_update = true;
                                new_sensors.push_back(sensor);
                        }
                }

                cout << "manage_trickle_sensors: E" << endl;
                
                if (need_update) {
                        // We need to update the group definition.
                        
                        Sensors sensors;
                        append(sensors, old_sensors);
                        append(sensors, pending_sensors);
                        append(sensors, new_sensors);
                        
                        update_json(sensors);



                        CurlEasy curl;
                        curl.outgoing_data << json;
                        curl.append_header_line("Content-Type: application/json");
                        curl.append_header_line(string("X-OpenSensorData-Key: ") + osd_key);

                        if (!Debug) {
                                curl.upload_to("http://opensensordata.net/groups/");
                                
                                if (curl.response_code != 200L) {
                                        cout << " * ERROR: OSD group request failed: " << curl.response_code << endl;
                                        cout << " == RESPONSE BEGIN ==" << endl;
                                        cout << curl.ingoing_data.str();
                                        cout << " == RESPONSE END ====" << endl;
                                        return -1;
                                }

                        
                        /*
                          cout << " * TEST: OSD group request" << endl;
                          cout << " == REQUEST BEGIN ==" << endl;
                          cout << curl.outgoing_data.str();
                          cout << " == REQUEST END ====" << endl;
                          cout << " == RESPONSE BEGIN ==" << endl;
                          cout << curl.ingoing_data.str();
                          cout << " == RESPONSE END ====" << endl;
                        */
                        

                        cout << "curl.ingoing_data.str()" << curl.ingoing_data.str() << std::endl;
                        json = curl.ingoing_data.str();
                        
                        }
                        // Kev
                        //update();                        
                        
                        // Get ids of sensors in the current trickle message.
                        sensors = parse_sensors();
                        for (size_t i = 0; i < sensors.size(); i++) {
                                SensorData& sensor = sensors[i];
                                map<string, int>::iterator it = name_to_index.find(sensor.name);
                                if (it != name_to_index.end()) {
                                        SensorData& sensor_ = trickle_sensors[it->second];
                                        sensor_.datastream_id = sensor.datastream_id;
                                }
                        }

                        // Kev
                        update_json(sensors);
                        save_json();
                }
                
                return 0;
        }

}; // end class SensorHandler

const std::string SensorHandler::JsonFilename = std::string("sensors.json");


// TEMP function
// Todo remove it

void CreateNewGroup() {
        CurlEasy curl;        

        
        ostringstream oss;
        if (!LoadFromFile("group.json", oss)) {
                std::cout << "Json file error..." << std::endl;
        }

        curl.outgoing_data << oss.str();
        std::cout << oss.str();
        if (curl.outgoing_data.tellp() <= 0) {
                cout << " * ERROR: no datapoints in the trickle message" << endl;
                return;
        }
        curl.append_header_line("Content-Type: application/json");
        curl.append_header_line("Content-Lenght: 100000");
        curl.append_header_line(string("X-OpenSensorData-Key: ") + Config::Key);
        curl.upload_to("http://opensensordata.net/groups/");
        
        if (curl.response_code != 200L) {
                cout << " * ERROR: OSD datapoints upload failed: " << curl.response_code << endl;
                cout << " == RESPONSE BEGIN ==" << endl;
                cout << curl.ingoing_data.str();
                cout << " == RESPONSE END ====" << endl;
                return;
        }
        std::cout << "End CreateNewGroup" << std::endl;
}

int send_trickle_up(const char* appname, const char* xmldata) {


        Config::Load();
        
        //CreateNewGroup();
        //SensorHandler::UploadDatapoint(OsdKey);

        //return 1;

        MIOFILE mf;
        mf.init_buf_read(xmldata);
        XML_PARSER xp(&mf);

        Sensors trickle_sensors;
        Datapoints datapoints;
        map<string, int> name_to_index;

        SensorHandler sensor_handler = SensorHandler();

        //cout << "SensorHanderler created ! " << endl;

        while (true) {
                if (xp.get_tag()) {
                        break;
                }
                string str;

                if (xp.parse_string("sensors", str)) {
                        {
                                // Parse sensors in the trickle message
                                istringstream iss(str);
                                
                                while (true) {

                                        string line;
                                        getline(iss, line);

                                        if (line.empty()) {
                                                break;
                                        }
                                        
                                        istringstream issl(line);
                                        
                                        SensorData  sensor;

                                        string index;
                                        getline(issl, index, ',');
                                        // Convert string to int;
                                        istringstream(index) >> sensor.index;
                                        if (sensor.index != (int) trickle_sensors.size()) {
                                                cout << " * ERROR: unexpected sensor index: " << sensor.index << " != " << trickle_sensors.size() << endl;
                                                return 0;
                                        }


                                        getline(issl, sensor.name, ',');
                                        if (sensor.name.empty()) {
                                                cerr << " * ERROR: sensor with no name: " << sensor.index << endl;
                                                return 0;
                                        }


                                        getline(issl, sensor.description);

                                        if (name_to_index.find(sensor.name) != name_to_index.end()) {
                                                cerr << " * ERROR: duplicate sensor name: " << sensor.name << endl;
                                                return 0;
                                        }
                                        
                                        name_to_index[sensor.name] = trickle_sensors.size();
                                        trickle_sensors.push_back(sensor);
                                        
                                        // Debug print

                                        //sensor.print(std::cout);
                                        //std::cout << std::endl;
                                }

                                // Change ids
                                
                                sensor_handler.change_datastream_id(trickle_sensors);
                        }                        
                }

                else if (xp.parse_string("datapoints", str)) {

                        // Parse and upload datapoints.

                        if (trickle_sensors.empty()) {
                                cout << " * ERROR: <datapoints> before or without <sensors>" << endl;
                                return 0;
                        }

                        istringstream iss(str);
                        CurlEasy curl;

                        while (true) {
                                string line;
                                getline(iss, line);
                                if (line.size() == 0) break;

                                istringstream issl(line);
                                string index;
                                getline(issl, index, ',');
                                istringstream issi(index);
                                int idx = -1;
                                issi >> idx;
                                if (idx < 0 || idx >= (int) trickle_sensors.size()) {
                                        cerr << " * ERROR: bad sensor index in datapoints: " << idx << endl;
                                        return 0;
                                }

                                SensorData& sensor = trickle_sensors[idx];
                                /*if (!sensor.idIsValid()) {
                                        cerr << " * ERROR: sensor #" << idx
                                             << " (" << sensor.name << ")"
                                             << " has incorrect OSD id" << endl;
                                        return 0;
                                        }*/

                                Point p;
                                //string date = "";
                                getline(issl, p.date, ',');
                                if (p.date.empty()) {
                                        cout << " * ERROR: datapoint is missing date" << endl;
                                        return 0;
                                }

                                //string value = "";
                                getline(issl, p.value);
                                if (p.value.empty()) {
                                        cout << " * ERROR: datapoint is missing value" << endl;
                                        return 0;
                                }

                                // Store datapoint value, it may be usefull to write data localy
                                sensor.datapoints.push_back(p);

                                // Fill with datapoints content 
                                
                                if (sensor.idIsValid()) {
                                        curl.outgoing_data << sensor.datastream_id << "," << p.date << "," << p.value << endl;
                                } else {
                                
                                        //std::cout << "ELSE" << std::endl;
                                        //sensor.print(std::cout);
                                        //std::cout << std::endl;
                                }
                        }

                        if (curl.outgoing_data.tellp() <= 0) {
                                cerr << " * ERROR: no datapoints in the trickle message" << endl;
                                return 0;
                        }

                        // Send datapoints content to database

                        if (!Debug) {
                                curl.append_header_line("Content-Type: text/csv");
                                curl.append_header_line(string("X-OpenSensorData-Key: ") + OsdKey);
                                curl.upload_to("http://opensensordata.net/upload/");
                                
                                if (curl.response_code != 200L) {
                                        cout << " * ERROR: OSD datapoints upload failed: " << curl.response_code << endl;
                                        cout << " == RESPONSE BEGIN ==" << endl;
                                        cout << curl.ingoing_data.str();
                                        cout << " == RESPONSE END ====" << endl;
                                        return 0;
                                }
                        }

                        // Write datapoints in local file


                        std::string current_str_time = to_string_with_underscore(get_current_time());
                        // Write all XML data in a file
                        {
                                // TODO
                                SaveToFile( "datastream" + current_str_time + ".xml", ostringstream(xmldata).str());      
                        }

                        // Write sent datastream/datapoints in JSON file
                        {
                                ostringstream oss;
                                oss << "{[" << std::endl;
                                int size = trickle_sensors.size();
                                int last = size - 1;
                                for (int i = 0; i < size; ++i) {
                                        SensorData& sensor = trickle_sensors[i];
                                        sensor.print_full_json(oss);

                                        if (i < last) {
                                                oss << "," << std::endl;
                                        }
                                }
                                oss << std::endl <<"]};";
                                
                                SaveToFile( "datastream" + current_str_time + ".json", oss.str());
                                //std::cout << "futur json: " << oss.str() << std::endl;
                        }
                        
                }

                else if (xp.parse_string("errors", str)) {
                        // Do nothing
                }

                else {
                        cout << " * ERROR: unexpected tag " << xp.parsed_tag << endl;
                        return 0;
                }

        } // end while

        return 0;
}
