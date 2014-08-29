// g++ -g -Wall test.cpp -o test
#include <iostream>
#include <string>
#include "json.h"
#include "json.c"

using namespace std;

string json = "{ \"name\": \"u1h3\", \"description\": \"low-energy-boinc\", \"datastreams\": [{ \"name\": \"cpu0pstate0\", \"description\": \"frequency 2801000\"}, { \"name\": \"cpu0pstateN\", \"description\": \"frequency 1200000\"}, { \"name\": \"cpu1pstate0\", \"description\": \"frequency 2801000\"}, { \"name\": \"cpu1pstateN\", \"description\": \"frequency 1200000\"}, { \"name\": \"cpu2pstate0\", \"description\": \"frequency 2801000\"}, { \"name\": \"cpu2pstateN\", \"description\": \"frequency 1200000\"}, { \"name\": \"cpu3pstate0\", \"description\": \"frequency 2801000\"}, { \"name\": \"cpu3pstateN\", \"description\": \"frequency 1200000\"}] }";

int main(void) {
        json_parser_t* json_parser = json_parser_create();

        json_parser_eval(json_parser, json.data());
        if (!json_parser_done(json_parser)) {
                cerr << "json_parser_done" << endl;
                return -1;
        }

        json_parser_destroy(json_parser);
        return 0;
}

