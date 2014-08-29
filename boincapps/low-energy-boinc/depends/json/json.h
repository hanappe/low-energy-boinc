/*
json.h
Copyright (C) 2010 SONY Computer Science Laboratory Paris

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License, version 2.1, as published by the Free Software Foundation.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
02110-1301 USA
*/


#ifndef _JSON_H
#define _JSON_H

#include <stdio.h>

typedef char  int8;
typedef unsigned char uint8;
typedef short int16;
typedef int int32;
typedef unsigned int uint32;
typedef double float64;


#ifdef __cplusplus
extern "C" {
#endif

//

enum {
	k_json_null = 0,
	k_json_object = 1,
	k_json_array = 2,
	k_json_string = 3,
	k_json_number = 4,
	k_json_boolean = 5,
};

typedef struct _json_object_t {
	uint8 type;
	union {
		void* data;
		float64 number;
	} value;
} json_object_t;


#define json_ref(_obj)   json_refcount(&(_obj), 1)
#define json_unref(_obj) json_refcount(&(_obj), -1)
void json_refcount(json_object_t *object, int val);

typedef int (*json_iterator_t)(char* key, json_object_t* value, void* data);
typedef int (*json_writer_t)(void* userdata, const char* s, int len);

//

#define json_type(__o) __o.type

// object

json_object_t json_object_create();
int json_object_length(json_object_t object);
void json_object_foreach(json_object_t object, json_iterator_t func, void* data);
json_object_t json_object_get(json_object_t object, const char* key);
int json_object_set(json_object_t object, const char* key, json_object_t value);
int json_object_unset(json_object_t object, const char* key);

double json_object_getnum(json_object_t object, char* key);
const char* json_object_getstr(json_object_t object, const char* key);

int json_object_setnum(json_object_t object, const char* key, double value);
int json_object_setstr(json_object_t object, const char* key, const char* value);

#define json_isobject(__obj) (__obj.type == k_json_object)

// serialisation

int json_serialise(json_object_t object, int binary, json_writer_t fun, void* userdata);
int json_tostring(json_object_t object, char* buffer, int buflen);
int json_tofile(json_object_t object, int binary, const char* path);
int json_tofilep(json_object_t object, int binary, FILE* fp);

// parsing
typedef struct _json_parser_t json_parser_t;

json_parser_t* json_parser_create();
void json_parser_destroy(json_parser_t* parser);

void json_parser_reset(json_parser_t* parser);

int json_parser_errno(json_parser_t* parser);
char* json_parser_errstr(json_parser_t* parser);

int json_parser_feed(json_parser_t* parser, const char* buffer, int len);
int json_parser_feed_one(json_parser_t* parser, char c);
int json_parser_done(json_parser_t* parser);

json_object_t json_parser_eval(json_parser_t* parser, const char* buffer);

json_object_t json_parser_result(json_parser_t* parser);



// null

json_object_t json_null();

#define json_isnull(__obj) (__obj.type == k_json_null)

// boolean

json_object_t json_true();
json_object_t json_false();

// number

json_object_t json_number_create(double value);
float64 json_number_value(json_object_t);

#define json_isnumber(__obj) (__obj.type == k_json_number)

// string

json_object_t json_string_create(const char* s);
const char* json_string_value(json_object_t string);
int json_string_length(json_object_t string);
int json_string_equals(json_object_t string, const char* s);

#define json_isstring(__obj) (__obj.type == k_json_string)

// array

json_object_t json_array_create();
int json_array_length(json_object_t array);
json_object_t json_array_get(json_object_t array, int index);
int json_array_set(json_object_t array, json_object_t value, int index);
int json_array_push(json_object_t array, json_object_t value);

int json_array_gettype(json_object_t object, int index);

double json_array_getnum(json_object_t object, int index);
const char* json_array_getstr(json_object_t object, int index);

int json_array_setnum(json_object_t object, double value, int index);
int json_array_setstr(json_object_t object, char* value, int index);

#define json_isarray(__obj) (__obj.type == k_json_array)

#ifdef __cplusplus
}
#endif

#endif // _JSON_H
