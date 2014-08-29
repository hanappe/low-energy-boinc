#include <iostream>
#include <sstream>
#include <string>
#include <cstring>
#include <map>
#include <vector>
#include <cstdarg>

#include <curl/curl.h>

#include "error_numbers.h"
#include "str_replace.h"
#include "trickle_handler.h"

#include "json.h"
// Ugly but necessary since we need to compile into BOINC's source tree.
#include "json.c"

using namespace std;

template <class T> void append(vector<T>& dest, vector<T>& src) {
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

////////////////////////////////////////////////////////////////////////////////
// Trickle sensors
////////////////////////////////////////////////////////////////////////////////

struct Sensor {
        int index;
        int osd_id;
        string name;
        string description;

        Sensor() {
                index = -1;
                osd_id = unknown_osd_id;
        }

        void print_json(ostream& os) {
                os << "{";

                if (osd_id != unknown_osd_id) {
                        os << "\"id\":" << osd_id << ",";
                }

                os << "\"name\":\"" << name << "\","
                   << "\"description\":\"" << description << "\"}";
        }

        static int unknown_osd_id;
};

typedef vector<Sensor> Sensors;

int Sensor::unknown_osd_id = -1;

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

////////////////////////////////////////////////////////////////////////////////
// BOINC style database objects
////////////////////////////////////////////////////////////////////////////////

struct OSD_ACCOUNT {
        int id;
        int create_time;
        int userid;
        string osd_key;

        void clear() {
                id = create_time = userid = 0;
                osd_key = "";
        }

        static string unknown_osd_key;
};

string OSD_ACCOUNT::unknown_osd_key("unknown_osd_key");

struct DB_OSD_ACCOUNT : DB_BASE, OSD_ACCOUNT {

        DB_OSD_ACCOUNT(DB_CONN* dc = 0) : DB_BASE("osd_account", dc ? dc : &boinc_db) {
        }

        int get_id() {
                return id;
        }

        void db_print(char* buf) {
                string osd_key_ = my_escape_string(osd_key);
                sprintf(buf,
                        "create_time=%d, "
                        "userid=%d, "
                        "osd_key='%s' ",
                        create_time,
                        userid,
                        osd_key_.data()
                        );
        }

        void db_parse(MYSQL_ROW &r) {
                clear();
                int i = 0;
                id = atoi(r[i++]);
                create_time = atoi(r[i++]);
                userid = atoi(r[i++]);
                osd_key = r[i++];
        }

        void operator=(OSD_ACCOUNT& w) {
                OSD_ACCOUNT::operator=(w);
        }

        static string create_table;
};

string DB_OSD_ACCOUNT::create_table("\
create table if not exists osd_account (                             \
    id                      integer         not null auto_increment, \
    create_time             integer         not null,                \
    userid                  integer         not null,                \
    osd_key                 blob,                                    \
    primary key (id)                                                 \
) engine=InnoDB;                                                     \
");

struct OSD_GROUP {
        int id;
        int create_time;
        int userid;
        int hostid;
        string gname;
        string json;

        void clear() {
                id = create_time = userid = hostid = 0;
                gname = "";
                json = "";
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
                        Sensor sensor;
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

                        json_object_t osd_id = json_object_get(datastream, "id");

                        // FIXME: check OSD server so that ids are
                        // always integers or always strings, not
                        // sometimes one, sometimes the other.
                        if (json_isnumber(osd_id)) {
                                sensor.osd_id = json_number_value(osd_id);
                        } else if (json_isstring(osd_id)) {
                                sensor.osd_id = atoi(json_string_value(osd_id));
                        } else {
                                sensor.osd_id = Sensor::unknown_osd_id;
                        }

                        sensors.push_back(sensor);
                }

                json_unref(group);
                return sensors;
        }

        virtual void update_json(Sensors& sensors) {
                ostringstream oss;

                oss << "{\"name\":\"" << gname << "\","
                    << "\"description\":\"lowenergyboinc\","
                    << "\"datastreams\":[";

                for (size_t i = 0; i < sensors.size(); i++) {
                        if (i > 0) oss << ',';
                        sensors[i].print_json(oss);
                }

                oss << "]}";
                json = oss.str();
        }

};

struct DB_OSD_GROUP : DB_BASE, OSD_GROUP {

        DB_OSD_GROUP(DB_CONN* dc = 0) : DB_BASE("osd_group", dc ? dc : &boinc_db) {
        }

        int get_id() {
                return id;
        }

        void db_print(char* buf) {
                string gname_ = my_escape_string(gname);
                string json_ = my_escape_string(json);
                sprintf(buf,
                        "create_time=%d, "
                        "userid=%d, "
                        "hostid=%d, "
                        "name='%s', "
                        "json='%s' ",
                        create_time,
                        userid,
                        hostid,
                        gname_.data(),
                        json_.data()
                        );
        }

        void db_parse(MYSQL_ROW &r) {
                clear();
                int i = 0;
                id = atoi(r[i++]);
                create_time = atoi(r[i++]);
                userid = atoi(r[i++]);
                hostid = atoi(r[i++]);
                gname = r[i++];
                json = r[i++];
        }

        void operator=(OSD_GROUP& w) {
                OSD_GROUP::operator=(w);
        }


        virtual void update_json(Sensors& sensors) {
                OSD_GROUP::update_json(sensors);
                update();
        }

        virtual void build_name(int userid_, int hostid_) {
                ostringstream ss;
                ss << 'u' << userid_ << 'h' << hostid_;
                gname = ss.str();
        }

        void lookup_or_insert(int userid_, int hostid_) {
                build_name(userid_, hostid_);

                // Get OSD group info.
                ostringstream lookupstring;
                lookupstring << " where userid = " << userid_
                             << " and hostid = " << hostid_
                             << " and name = '" << gname << "'";

                if (lookup(lookupstring.str().data()) != 0) {
                        // First trickle message from this host. Start with empty OSD group.
                        clear();
                        create_time = time(0);
                        userid = userid_;
                        hostid = hostid_;
                        build_name(userid_, hostid_);
                        json = "";
                        insert();
                        clear();
                        lookup(lookupstring.str().data());

                        // Empty sensors.
                        Sensors sensors;
                        update_json(sensors);
                }
        }

        virtual bool manage_sensor(Sensor& sensor) {
                // Anything but BOINC workunit sensors.
                return sensor.description.find("workunit") != 0;
        }

        int manage_trickle_sensors(Sensors& trickle_sensors, map<string, int>& name_to_index, DB_OSD_ACCOUNT& osd_account) {
                Sensors parsed_sensors = parse_sensors();
                Sensors old_sensors;
                Sensors pending_sensors;
                Sensors new_sensors;
                bool need_update = false;

                for (size_t i = 0; i < parsed_sensors.size(); i++) {
                        Sensor& sensor = parsed_sensors[i];
                        if (!manage_sensor(sensor)) {
                                // Some old sensor is not managed anymore by this group.
                                need_update = true;
                                continue;
                        }

                        map<string, int>::iterator it = name_to_index.find(sensor.name);
                        if (it == name_to_index.end()) {
                                // Known sensor does not appear in the trickle message.
                                if (sensor.osd_id == Sensor::unknown_osd_id) {
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
                                Sensor& sensor_ = trickle_sensors[it->second];
                                sensor_.osd_id = sensor.osd_id;

                                if (sensor_.osd_id == Sensor::unknown_osd_id) {
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
                }

                for (size_t i = 0; i < trickle_sensors.size(); i++) {
                        Sensor& sensor = trickle_sensors[i];
                        if (!manage_sensor(sensor)) continue;
                        if (sensor.osd_id == Sensor::unknown_osd_id) {
                                need_update = true;
                                new_sensors.push_back(sensor);
                        }
                }

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
                        curl.append_header_line(string("X-OpenSensorData-Key: ") + osd_account.osd_key);
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

                        json = curl.ingoing_data.str();
                        update();

                        // Get ids of sensors in the current trickle message.
                        sensors = parse_sensors();
                        for (size_t i = 0; i < sensors.size(); i++) {
                                Sensor& sensor = sensors[i];
                                map<string, int>::iterator it = name_to_index.find(sensor.name);
                                if (it != name_to_index.end()) {
                                        Sensor& sensor_ = trickle_sensors[it->second];
                                        sensor_.osd_id = sensor.osd_id;
                                }
                        }
                }

                return 0;
        }

        static string create_table;
};

string DB_OSD_GROUP::create_table("\
create table if not exists osd_group (                               \
    id                      integer         not null auto_increment, \
    create_time             integer         not null,                \
    userid                  integer         not null,                \
    hostid                  integer         not null,                \
    name                    blob,                                    \
    json                    blob,                                    \
    primary key (id)                                                 \
) engine=InnoDB;                                                     \
");

// OSD group dedicated to BOINC workunit sensors. They are filtered
// based on their description.
struct DB_OSD_GROUP_WUS : DB_OSD_GROUP {

        virtual bool manage_sensor(Sensor& sensor) {
                return sensor.description.find("workunit") == 0;
        }

        virtual void build_name(int userid_, int hostid_) {
                ostringstream ss;
                ss << 'u' << userid_ << 'h' << hostid_ << "_wus";
                gname = ss.str();
        }

};


////////////////////////////////////////////////////////////////////////////////
// Implementation of trickle handler
////////////////////////////////////////////////////////////////////////////////

int handle_trickle_init(int, char**) {
        curl_global_init(CURL_GLOBAL_ALL);
        boinc_db.do_query(DB_OSD_ACCOUNT::create_table.data());
        boinc_db.do_query(DB_OSD_GROUP::create_table.data());
        return 0;
}

int handle_trickle(MSG_FROM_HOST& msg) {

        {
                // Delete old trickle messages which were successfully handled.
                DB_MSG_FROM_HOST msg_;
                msg_.delete_from_db_multi("variety = 'low-energy-boinc' and handled = 1");
        }

        cout << "msg id: " << msg.id << ", "
             << "hostid: " << msg.hostid << ", "
             << "create_time: " << msg.create_time << ", "
             << "xml size: " << strlen(msg.xml)
             << endl;

	/*
	cout << " == MESSAGE BEGIN ==" << endl
	     << msg.xml << endl
	     << " == MESSAGE END ==" << endl;
	*/

        DB_HOST host;
        if (host.lookup_id(msg.hostid) != 0) {
                cout << " * ERROR: unknown hostid: " << msg.hostid << endl;
                return 0;
        }

	//cout << msg.xml << endl;

        MIOFILE mf;
        mf.init_buf_read(msg.xml);
        XML_PARSER xp(&mf);

        xp.get_tag();
        string result_name;
        if (!xp.parse_string("result_name", result_name)) {
                cout << " * ERROR: missing tag: result_name" << endl;
                return 0;
        }

        xp.get_tag();
        string time_s;
        if (!xp.parse_string("time", time_s)) {
                cout << " * ERROR: missing tag: time" << endl;
                return 0;
        }

        map<string, int> name_to_index;
        Sensors trickle_sensors;
        DB_OSD_ACCOUNT osd_account;
        DB_OSD_GROUP osd_group;
        DB_OSD_GROUP_WUS osd_group_wus;

        while (!xp.get_tag()) {
                string str;

                if (xp.parse_string("sensors", str)) {
                        {
                                // Parse sensors in the trickle message
                                istringstream iss(str);

                                while (true) {
                                        string line;
                                        getline(iss, line);
                                        if (line.size() == 0) break;

                                        istringstream issl(line);
                                        Sensor sensor;

                                        string index;
                                        getline(issl, index, ',');
                                        istringstream issi(index);

                                        sensor.index = -1;
                                        issi >> sensor.index;
                                        if (sensor.index != (int) trickle_sensors.size()) {
                                                cout << " * ERROR: unexpected sensor index: "
                                                     << sensor.index << " != " << trickle_sensors.size() << endl;
                                                return 0;
                                        }

                                        sensor.name = "";
                                        getline(issl, sensor.name, ',');
                                        if (sensor.name.size() == 0) {
                                                cout << " * ERROR: sensor with no name: " << sensor.index << endl;
                                                return 0;
                                        }

                                        sensor.description = "";
                                        getline(issl, sensor.description);

                                        map<string, int>::iterator it = name_to_index.find(sensor.name);
                                        if (it != name_to_index.end()) {
                                                cout << " * ERROR: duplicate sensor name: " << sensor.name << endl;
                                                return 0;
                                        }

                                        sensor.osd_id = Sensor::unknown_osd_id;
                                        name_to_index[sensor.name] = trickle_sensors.size();
                                        trickle_sensors.push_back(sensor);
                                }
                        }

                        {
                                // Get OSD account info.
                                ostringstream lookup;
                                lookup << " where userid = " << host.userid;
                                if (osd_account.lookup(lookup.str().data()) != 0) {
                                        DB_USER db_user;
                                        if (db_user.lookup_id(host.userid) != 0) {
                                                cout << " * ERROR: could not find user with id " << host.userid << endl;
                                                return 0;
                                        }

                                        // Request a new account on the OSD server. Get the key.
                                        string user_key;
                                        string json;

                                        {
                                                CurlEasy curl;
                                                curl.outgoing_data
                                                        << "{\"username\":\"lowenergyboinc_u" << host.userid << "\","
                                                        << "\"email\":\"" << db_user.email_addr << "\"}";

                                                curl.append_header_line("Content-Type: application/json");
                                                // Special OSD key to create accounts without email authentication. Keep it secret.
                                                curl.append_header_line("X-OpenSensorData-AppKey: f0945f4e-1bc5-bed0-ddec-f8ddc8a48b76");
                                                curl.upload_to("http://opensensordata.net/accounts/");

                                                if (curl.response_code != 200L) {
                                                        cout << " * ERROR: OSD new account request failed: " << curl.response_code << endl;
                                                        cout << " == REQUEST BEGIN ==" << endl;
                                                        cout << curl.outgoing_data.str();
                                                        cout << " == REQUEST END ====" << endl;
                                                        cout << " == RESPONSE BEGIN ==" << endl;
                                                        cout << curl.ingoing_data.str();
                                                        cout << " == RESPONSE END ====" << endl;
                                                        return 0;
                                                }

                                                json = curl.ingoing_data.str();
                                        }

                                        {
                                                json_parser_t* json_parser = json_parser_create();
                                                if (!json_parser) {
                                                        cout << " * ERROR: json_parser_create" << endl;
                                                        return 0;
                                                }

                                                json_object_t response = json_parser_eval(json_parser, json.data());
                                                if (!json_parser_done(json_parser)) {
                                                        cout << " * ERROR: json_parser error: " << json_parser_errstr(json_parser) << endl;
                                                        return 0;
                                                }

                                                json_parser_destroy(json_parser);

                                                json_object_t key = json_object_get(response, "key");
                                                if (!json_isstring(key)) {
                                                        cout << " * ERROR: json_object_get: no 'key' string" << endl;
                                                        return 0;
                                                }

                                                user_key = json_string_value(key);
                                                json_unref(response);
                                        }

                                        osd_account.clear();
                                        osd_account.create_time = time(0);
                                        osd_account.userid = host.userid;
                                        osd_account.osd_key = user_key;
                                        osd_account.insert();
                                }

                                osd_account.clear();
                                if (osd_account.lookup(lookup.str().data()) != 0) {
                                        cout << " * ERROR: could not find OSD account for user " << host.userid << endl;
                                        return 0;
                                }

                                if (OSD_ACCOUNT::unknown_osd_key.compare(osd_account.osd_key) == 0) {
                                        cout << " * ERROR: unknown OSD key for user " << host.userid << endl;
                                        return 0;
                                }
                        }

                        osd_group.lookup_or_insert(osd_account.userid, host.id);
                        osd_group.manage_trickle_sensors(trickle_sensors, name_to_index, osd_account);

                        osd_group_wus.lookup_or_insert(osd_account.userid, host.id);
                        osd_group_wus.manage_trickle_sensors(trickle_sensors, name_to_index, osd_account);
                }

                else if (xp.parse_string("datapoints", str)) {
                        // Parse and upload datapoints.

                        if (trickle_sensors.size() == 0) {
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
                                        cout << " * ERROR: bad sensor index in datapoints: " << idx << endl;
                                        return 0;
                                }

                                Sensor& sensor = trickle_sensors[idx];
                                if (sensor.osd_id == Sensor::unknown_osd_id) {
                                        cout << " * ERROR: sensor #" << idx
                                             << " (" << sensor.name << ")"
                                             << " has incorrect OSD id" << endl;
                                        return 0;
                                }

                                string date = "";
                                getline(issl, date, ',');
                                if (date.size() == 0) {
                                        cout << " * ERROR: datapoint is missing date" << endl;
                                        return 0;
                                }

                                string value = "";
                                getline(issl, value);
                                if (value.size() == 0) {
                                        cout << " * ERROR: datapoint is missing value" << endl;
                                        return 0;
                                }

                                curl.outgoing_data << sensor.osd_id << "," << date << "," << value << endl;
                        }

                        if (curl.outgoing_data.tellp() <= 0) {
                                cout << " * ERROR: no datapoints in the trickle message" << endl;
                                return 0;
                        }

                        curl.append_header_line("Content-Type: text/csv");
                        curl.append_header_line(string("X-OpenSensorData-Key: ") + osd_account.osd_key);
                        curl.upload_to("http://opensensordata.net/upload/");

                        if (curl.response_code != 200L) {
                                cout << " * ERROR: OSD datapoints upload failed: " << curl.response_code << endl;
                                cout << " == RESPONSE BEGIN ==" << endl;
                                cout << curl.ingoing_data.str();
                                cout << " == RESPONSE END ====" << endl;
                                return 0;
                        }
                }

                else if (xp.parse_string("errors", str)) {
                        // Parse errors
                        istringstream iss(str);

                        while (true) {
                                string line;
                                getline(iss, line);
                                if (line.size() == 0) break;

                                istringstream issl(line);

                                string module;
                                getline(issl, module, ',');

                                string code;
                                getline(issl, code, ',');

                                string text;
                                getline(issl, text);

                                cout << " * ERROR: "
                                     << module << ", code = "
                                     << code << ", "
                                     << text << endl;
                        }
                }

                else {
                        cout << " * ERROR: unexpected tag " << xp.parsed_tag << endl;
                        return 0;
                }
        }

        osd_group.clear();
        osd_group_wus.clear();
        osd_account.clear();
        return 0;
}

