/*
json.c
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "json.h"

#define HASHTABLE_MIN_SIZE 7
#define HASHTABLE_MAX_SIZE 13845163

char* json_strdup(const char* s);

#define CHECK_MEMORY 0

#if CHECK_MEMORY
#include <sgc.h>
#define JSON_NEW(_type)            (_type*) sgc_new_object(sizeof(_type), SGC_ZERO, 0)
#define JSON_NEW_ARRAY(_type, _n)  (_type*) sgc_new_object(_n * sizeof(_type), SGC_ZERO, 0)
#define JSON_FREE(_p)              sgc_free_object((void*)_p)

#else
#define JSON_NEW(_type)            (_type*) calloc(1, sizeof(_type))
#define JSON_NEW_ARRAY(_type, _n)  (_type*) calloc(_n, sizeof(_type))
#define JSON_FREE(_p)              free(_p)
#endif


#if CHECK_MEMORY
static int memory_leak_callback(int op, void* ptr, int type, int counter, int size)
{
        if (op == 3) {
                printf("Memory leak: counter=%d, size=%d\n", counter, size);
        }
        return 1;
}

void __attribute__ ((constructor))  json_init()
{
        int dummy;
	if (sgc_init(0, &dummy, 0, 0) != 0) {
                fprintf(stderr, "Failed the initialise the SGC memory heap\n");
                exit(1);
        }
}

void __attribute__ ((destructor)) json_cleanup()
{
        sgc_search_memory_leaks(memory_leak_callback);
        sgc_cleanup();
}
#endif

/******************************************************************************/

typedef struct _string_t {
	uint32 refcount;
	int length;
	char* s;
} string_t;

static string_t* new_string(const char* s)
{
	string_t *string;

        //printf("new string %s\n", s);

	string = JSON_NEW(string_t);
	if (string == NULL) {
		return NULL;
	}

	string->refcount = 0;
	string->length = strlen(s);
	string->s = JSON_NEW_ARRAY(char, string->length+1);
	if (string->s == NULL) {
		JSON_FREE(string);
		return NULL;
	}
	memcpy(string->s, s, string->length);
	string->s[string->length] = 0;

	return string;
}

static void delete_string(string_t* string)
{
	JSON_FREE(string->s);
	JSON_FREE(string);
}

/******************************************************************************/

typedef struct _array_t {
	uint32 refcount;
	int length;
	int datalen;
	json_object_t* data;
} array_t;

static array_t* new_array()
{
	array_t *array;

	array = JSON_NEW(array_t);
	if (array == NULL) {
		return NULL;
	}

	array->refcount = 0;
	array->length = 0;
	array->datalen = 0;
	array->data = NULL;

	return array;
}

static void delete_array(array_t* array)
{
        for (int i = 0; i < array->datalen; i++)
                json_unref(array->data[i]);

	if (array->data) {
		JSON_FREE(array->data);
	}
	JSON_FREE(array);
}

static int array_set(array_t* array, json_object_t object, int index)
{
	if (index >= array->datalen) {
		int newlen = 8;
		while (newlen <= index) {
			newlen *= 2;
		}
		json_object_t* data = JSON_NEW_ARRAY(json_object_t, newlen);
		if (data == NULL) {
			return -1;
		}
		for (int i = 0; i < array->datalen; i++) {
			data[i] = array->data[i];
		}
		for (int i = array->datalen; i < newlen; i++) {
			data[i].type = k_json_null;
		}

		if (array->data) {
			JSON_FREE(array->data);
		}
		array->data = data;
		array->datalen = newlen;
	}

	array->data[index] = object;
        json_ref(array->data[index]);
	if (index >= array->length) {
		array->length = index+1;
	}

	return 0;
}

static json_object_t array_get(array_t* array, int index)
{
	if ((0 <= index) && (index < array->length)) {
		return array->data[index];
	}
	return json_null();
}

static int array_length(array_t* array)
{
	return array->length;
}

/******************************************************************************/

typedef struct _json_hashnode_t json_hashnode_t;

struct _json_hashnode_t {
	char* key;
	json_object_t value;
	json_hashnode_t *next;
};

static json_hashnode_t* new_json_hashnode(const char* key, json_object_t* value);
static void delete_json_hashnode(json_hashnode_t *hash_node);
static void delete_json_hashnodes(json_hashnode_t *hash_node);
unsigned int json_strhash(const char* v);


typedef struct _json_hashtable_t {
	uint32 refcount;
        int size;
        int num_nodes;
	json_hashnode_t **nodes;
} json_hashtable_t;


static json_hashtable_t* new_json_hashtable();
static void delete_json_hashtable(json_hashtable_t *hashtable);
static int json_hashtable_set(json_hashtable_t *hashtable, const char* key, json_object_t value);
static json_object_t json_hashtable_get(json_hashtable_t *hashtable, const char* key);
static int json_hashtable_unset(json_hashtable_t *hashtable, const char* key);
static void json_hashtable_foreach(json_hashtable_t *hashtable, json_iterator_t func, void* data);
static int json_hashtable_size(json_hashtable_t *hashtable);
static void json_hashtable_resize(json_hashtable_t *hashtable);
static json_hashnode_t** json_hashtable_lookup_node(json_hashtable_t *hashtable, const char* key);

static json_hashtable_t* new_json_hashtable()
{
	json_hashtable_t *hashtable;

	hashtable = JSON_NEW(json_hashtable_t);
	if (hashtable == NULL) {
		return NULL;
	}
	hashtable->refcount = 0;
	hashtable->num_nodes = 0;
	hashtable->size = 0;
	hashtable->nodes = NULL;

	return hashtable;
}


static void delete_json_hashtable(json_hashtable_t *hashtable)
{
	if (hashtable == NULL) {
		return;
	}

	if (hashtable->nodes) {
		for (int i = 0; i < hashtable->size; i++) {
			delete_json_hashnodes(hashtable->nodes[i]);
		}
		JSON_FREE(hashtable->nodes);
	}

	JSON_FREE(hashtable);
}

static json_hashnode_t** json_hashtable_lookup_node(json_hashtable_t* hashtable, const char* key)
{
	json_hashnode_t **node;

	if (hashtable->nodes == NULL) {
		hashtable->size = HASHTABLE_MIN_SIZE;
		hashtable->nodes = JSON_NEW_ARRAY(json_hashnode_t*, hashtable->size);
		for (int i = 0; i < hashtable->size; i++) {
			hashtable->nodes[i] = NULL;
		}
	}

	node = &hashtable->nodes[json_strhash(key) % hashtable->size];

	while (*node && (strcmp((*node)->key, key) != 0)) {
		node = &(*node)->next;
	}

	return node;
}


static int json_hashtable_set(json_hashtable_t *hashtable, const char* key, json_object_t value)
{

	if ((key == NULL) || (strlen(key) == 0)) {
		return -1;
	}

	json_hashnode_t **node = json_hashtable_lookup_node(hashtable, key);

	if (*node) {
		json_object_t oldvalue = (*node)->value;
		(*node)->value = value;
		json_ref((*node)->value);
		json_unref(oldvalue);

	} else {
		*node = new_json_hashnode(key, &value);
		if (*node == NULL) {
			return -1;
		}
		hashtable->num_nodes++;

		if ((3 * hashtable->size <= hashtable->num_nodes)
		    && (hashtable->size < HASHTABLE_MAX_SIZE)) {
			json_hashtable_resize(hashtable);
		}
	}

        char buffer[256];
        json_tostring(value, buffer, sizeof(buffer));
        //printf("hash node %s - %s\n", key, buffer);

	return 0;
}

static json_object_t json_hashtable_get(json_hashtable_t *hashtable, const char* key)
{
	json_hashnode_t *node;
	node = *json_hashtable_lookup_node(hashtable, key);
       	return (node)? node->value : json_null();
}

static int json_hashtable_unset(json_hashtable_t *hashtable, const char* key)
{
	json_hashnode_t **node, *dest;

	node = json_hashtable_lookup_node(hashtable, key);
	if (*node) {
		dest = *node;
		(*node) = dest->next;
		delete_json_hashnode(dest);
		hashtable->num_nodes--;
		return 0;
	}

	return -1;
}

static void json_hashtable_foreach(json_hashtable_t *hashtable, json_iterator_t func, void* data)
{
	json_hashnode_t *node = NULL;

	for (int i = 0; i < hashtable->size; i++) {
		for (node = hashtable->nodes[i]; node != NULL; node = node->next) {
			(*func)(node->key, &node->value, data);
		}
	}
}

static int json_hashtable_size(json_hashtable_t *hashtable)
{
	return hashtable->num_nodes;
}


static void json_hashtable_resize(json_hashtable_t *hashtable)
{
	json_hashnode_t **new_nodes;
	json_hashnode_t *node;
	json_hashnode_t *next;
	unsigned int hash_val;
	int new_size;

	new_size = 3 * hashtable->size + 1;
	new_size = (new_size > HASHTABLE_MAX_SIZE)? HASHTABLE_MAX_SIZE : new_size;

	new_nodes = JSON_NEW_ARRAY(json_hashnode_t*, new_size);

	for (int i = 0; i < hashtable->size; i++) {
		for (node = hashtable->nodes[i]; node; node = next) {
			next = node->next;
			hash_val = json_strhash(node->key) % new_size;
			node->next = new_nodes[hash_val];
			new_nodes[hash_val] = node;
		}
	}

	JSON_FREE(hashtable->nodes);
	hashtable->nodes = new_nodes;
	hashtable->size = new_size;
}

/******************************************************************************/


static json_hashnode_t* new_json_hashnode(const char* key, json_object_t* value)
{
	json_hashnode_t *hash_node = JSON_NEW(json_hashnode_t);
	if (hash_node == NULL) {
		return NULL;
	}

	hash_node->key = json_strdup(key);
	if (hash_node->key == NULL) {
		JSON_FREE(hash_node);
		return NULL;
	}
	hash_node->value = *value;
	json_ref(hash_node->value);
	hash_node->next = NULL;

	return hash_node;
}


static void delete_json_hashnode(json_hashnode_t *hash_node)
{
	json_unref(hash_node->value);
	JSON_FREE(hash_node->key);
	JSON_FREE(hash_node);
}


static void delete_json_hashnodes(json_hashnode_t *hash_node)
{
	while (hash_node) {
		json_hashnode_t *next = hash_node->next;
		delete_json_hashnode(hash_node);
		hash_node = next;
	}
}

/******************************************************************************/

/* 31 bit hash function */
unsigned int json_strhash(const char* key)
{
  const char *p = key;
  unsigned int h = *p;

  if (h) {
    for (p += 1; *p != '\0'; p++) {
      h = (h << 5) - h + *p;
    }
  }

  return h;
}


char* json_strdup(const char* s)
{
	int len = strlen(s) + 1;
	char* t = JSON_NEW_ARRAY(char, len);
	if (t == NULL) {
		return NULL;
	}
	strcpy(t, s);
	return t;
}

/******************************************************************************/

json_object_t json_object_create()
{
	json_object_t object;
	json_hashtable_t* hashtable;

	object.type = k_json_object;
	hashtable = new_json_hashtable();
	if (hashtable == NULL) {
		object.type = k_json_null;
		return object;
	}

	object.value.data = hashtable;
	json_ref(object);
	return object;
}

int json_object_set(json_object_t object, const char* key, json_object_t value)
{
	if (object.type != k_json_object) {
		return -1;
	}
	json_hashtable_t* hashtable = (json_hashtable_t*) object.value.data;
	return json_hashtable_set(hashtable, key, value);
}

json_object_t json_object_get(json_object_t object, const char* key)
{
	if (object.type != k_json_object) {
		return json_null();
	}
	json_hashtable_t* hashtable = (json_hashtable_t*) object.value.data;
	return json_hashtable_get(hashtable, key);
}

const char* json_object_getstr(json_object_t object, const char* key)
{
	if (object.type != k_json_object) {
		return NULL;
	}
	json_hashtable_t* hashtable = (json_hashtable_t*) object.value.data;
	json_object_t val = json_hashtable_get(hashtable, key);
        if (val.type == k_json_string) {
                return json_string_value(val);
        } else {
                return NULL;
        }
}

int json_object_unset(json_object_t object, const char* key)
{
	if (object.type != k_json_object) {
		return -1;
	}
	json_hashtable_t* hashtable = (json_hashtable_t*) object.value.data;
	return json_hashtable_unset(hashtable, key);
}


int json_object_setnum(json_object_t object, const char* key, double value)
{
	return json_object_set(object, key, json_number_create(value));
}

int json_object_setstr(json_object_t object, const char* key, const char* value)
{
	return json_object_set(object, key, json_string_create(value));
}

void json_object_foreach(json_object_t object, json_iterator_t func, void* data)
{
	if (object.type != k_json_object) {
		return;
	}
	json_hashtable_t* hashtable = (json_hashtable_t*) object.value.data;
	json_hashtable_foreach(hashtable, func, data);
}

int json_object_length(json_object_t object)
{
	if (object.type != k_json_object) {
		return 0;
	}
	return json_hashtable_size((json_hashtable_t*) object.value.data);
}

/******************************************************************************/

json_object_t json_number_create(double value)
{
	json_object_t number;
	number.type = k_json_number;
	number.value.number = value;
	json_ref(number);
	return number;
}

float64 json_number_value(json_object_t obj)
{
	return obj.value.number;
}

/******************************************************************************/

json_object_t json_string_create(const char* s)
{
	json_object_t string;
	string.type = k_json_string;
	string.value.data = new_string(s);
	if (string.value.data == NULL) {
		string.type = k_json_null;
	}
	json_ref(string);
	return string;
}

const char* json_string_value(json_object_t string)
{
	if (string.type != k_json_string) {
		return NULL;
	}
	string_t* str = (string_t*) string.value.data;
	return str->s;
}

int json_string_length(json_object_t string)
{
	if (string.type != k_json_string) {
		return 0;
	}
	string_t* str = (string_t*) string.value.data;
	return str->length;
}

int json_string_equals(json_object_t string, const char* s)
{
	if (string.type != k_json_string) {
		return 0;
	}
	string_t* str = (string_t*) string.value.data;
	return (strcmp(str->s, s) == 0);
}

/******************************************************************************/

void json_refcount(json_object_t *object, int val)
{
        switch (object->type) {

        case k_json_object: {
                json_hashtable_t* hashtable = (json_hashtable_t*) object->value.data;
                //printf("delete object\n");
                hashtable->refcount += val;
                if (hashtable->refcount <= 0) {
                        delete_json_hashtable(hashtable);
                        object->value.data = NULL;
                        object->type = k_json_null; }}
                break;
        case k_json_string: {
                string_t* string = (string_t*) object->value.data;
                //printf("delete string %s\n", ((string_t*)object->value.data)->s);
                string->refcount += val;
                if (string->refcount <= 0) {
                        delete_string(string);
                        object->value.data = NULL;
                        object->type = k_json_null; }}
                break;

        case k_json_array: {
                array_t* array = (array_t*) object->value.data;
                array->refcount += val;
                if (array->refcount <= 0) {
                        delete_array(array);
                        object->value.data = NULL;
                        object->type = k_json_null; }}
                break;
        default:
                break;
        }
}

/******************************************************************************/

json_object_t json_null()
{
	json_object_t object;
	object.type = k_json_null;
	json_ref(object);
	return object;
}

json_object_t json_true()
{
	json_object_t object;
	object.type = k_json_boolean;
	object.value.number = 1.0;
	json_ref(object);
	return object;
}

json_object_t json_false()
{
	json_object_t object;
	object.type = k_json_boolean;
	object.value.number = 0.0;
	json_ref(object);
	return object;
}

/******************************************************************************/

json_object_t json_array_create()
{
	json_object_t object;
	array_t* array;
	object.type = k_json_array;
	array = new_array();
	if (array == NULL) {
		return json_null();
	}

	object.value.data = array;
	json_ref(object);
	return object;
}

int json_array_length(json_object_t array)
{
	if (array.type != k_json_array) {
		return 0;
	}
	array_t* a = (array_t*) array.value.data;
	return array_length(a);
}

json_object_t json_array_get(json_object_t array, int index)
{
	if (array.type != k_json_array) {
		return json_null();
	}
	array_t* a = (array_t*) array.value.data;
	return array_get(a, index);
}

int json_array_set(json_object_t array, json_object_t value, int index)
{
	if (array.type != k_json_array) {
		return 0;
	}
	array_t* a = (array_t*) array.value.data;
	return array_set(a, value, index);
}

int json_array_push(json_object_t array, json_object_t value)
{
	if (array.type != k_json_array) {
		return 0;
	}
	array_t* a = (array_t*) array.value.data;
	int index = a->length;
	return array_set(a, value, index);
}

int json_array_setnum(json_object_t array, double value, int index)
{
	return json_array_set(array, json_number_create(value), index);
}

int json_array_setstr(json_object_t array, char* value, int index)
{
	return json_array_set(array, json_string_create(value), index);
}

float64 json_array_getnum(json_object_t array, int index)
{
        json_object_t val = json_array_get(array, index);
        if (val.type == k_json_number) {
                return json_number_value(val);
        } else {
                return 0.0;
        }
}

int json_array_gettype(json_object_t array, int index)
{
        json_object_t val = json_array_get(array, index);
        return val.type;
}

/******************************************************************************/


int json_serialise_text(json_object_t object, json_writer_t fun, void* userdata);

typedef struct _json_strbuf_t {
        char* s;
        int len;
        int index;
} json_strbuf_t;

static int json_strwriter(void* userdata, const char* s, int len)
{
        json_strbuf_t* strbuf = (json_strbuf_t*) userdata;
        while (len-- > 0) {
                if (strbuf->index >= strbuf->len - 1) {
                        return -1;
                }
                strbuf->s[strbuf->index++] = *s++;
        }
        return 0;
}

int json_tostring(json_object_t object, char* buffer, int buflen)
{
        json_strbuf_t strbuf = { buffer, buflen, 0 };

        int r = json_serialise(object, 0, json_strwriter, (void*) &strbuf);
        strbuf.s[strbuf.index] = 0;
        return r;
}

/* typedef struct _json_filebuf_t { */
/*         FILE* fp; */
/*         int linelen; */
/* } json_strbuf_t; */

static int json_file_writer(void* userdata, const char* s, int len)
{
        FILE* fp = (FILE*) userdata;
	int n = fwrite(s, len, 1, fp);
	return (n != 1);
}

int json_tofilep(json_object_t object, int binary, FILE* fp)
{
        int res = json_serialise(object, binary, json_file_writer, (void*) fp);
        fprintf(fp, "\n");
        return res;
}

int json_tofile(json_object_t object, int binary, const char* path)
{
        FILE* fp = fopen(path, "w");
        if (fp == NULL)
                return -1;

        int r = json_serialise(object, binary, json_file_writer, (void*) fp);
        fclose(fp);

        return r;
}

int json_serialise(json_object_t object, int binary, json_writer_t fun, void* userdata)
{
	if (binary) {
		return -1;
	} else {
		return json_serialise_text(object, fun, userdata);
	}
}

int json_print(json_writer_t fun, void* userdata, const char* s)
{
	return (*fun)(userdata, s, strlen(s));
}

int json_serialise_text(json_object_t object, json_writer_t fun, void* userdata)
{
	int r;

	switch (object.type) {

	case k_json_number: {
		char buf[128];
		if (floor(object.value.number) == object.value.number) {
			snprintf(buf, 128, "%g", object.value.number);
		} else {
			snprintf(buf, 128, "%e", object.value.number);
		}
		buf[127] = 0;
		r = json_print(fun, userdata, buf);
		if (r != 0) return r;
	} break;

	case k_json_string: {
		string_t* string = (string_t*) object.value.data;
		r = json_print(fun, userdata, "\"");
		if (r != 0) return r;
		r = json_print(fun, userdata, string->s);
		if (r != 0) return r;
		r = json_print(fun, userdata, "\"");
		if (r != 0) return r;
	} break;

	case k_json_boolean: {
		if (object.value.number) {
			r = json_print(fun, userdata, "true");
		} else {
			r = json_print(fun, userdata, "false");
		}
		if (r != 0) return r;
	} break;

	case k_json_null: {
		r = json_print(fun, userdata, "null");
		if (r != 0) return r;
	} break;

	case k_json_array: {
		r = json_print(fun, userdata, "[");
		if (r != 0) return r;
		array_t* array = (array_t*) object.value.data;
		for (int i = 0; i < array->length; i++) {
			r = json_serialise_text(array->data[i], fun, userdata);
			if (r != 0) return r;
			if (i < array->length - 1) {
				r = json_print(fun, userdata, ", ");
				if (r != 0) return r;
                                if ((i+1) % 10 == 0) // FIXME
                                        r = json_print(fun, userdata, "\n"); // FIXME
				if (r != 0) return r;
			}
		}
		r = json_print(fun, userdata, "]");
		if (r != 0) return r;

	} break;

	case k_json_object: {
		r = json_print(fun, userdata, "{");
		if (r != 0) return r;

		json_hashtable_t* hashtable = (json_hashtable_t*) object.value.data;
		json_hashnode_t *node = NULL;
		int count = 0;
		for (int i = 0; i < hashtable->size; i++) {
			for (node = hashtable->nodes[i]; node != NULL; node = node->next) {
				r = json_print(fun, userdata, "\"");
				if (r != 0) return r;
				r = json_print(fun, userdata, node->key);
				if (r != 0) return r;
				r = json_print(fun, userdata, "\": ");
				if (r != 0) return r;
				r = json_serialise_text(node->value, fun, userdata);
				if (r != 0) return r;
				if (++count < hashtable->num_nodes) {
					r = json_print(fun, userdata, ", ");
				}
			}
		}

		r = json_print(fun, userdata, "}");
		if (r != 0) return r;
	} break; }

	return 0;
}

typedef enum {
	k_end_of_string = -5,
	k_stack_overflow = -4,
	k_out_of_memory = -3,
	k_token_error = -2,
	k_parsing_error = -1,
	k_continue = 0
} json_error_t;

typedef enum {
	k_object_start,
	k_object_end,
	k_array_start,
	k_array_end,
	k_string,
	k_number,
	k_true,
	k_false,
	k_null,
	k_colon,
	k_comma
} json_token_t;

typedef enum {
	k_do,
	k_parsing_string,
	k_parsing_number,
	k_parsing_true,
	k_parsing_false,
	k_parsing_null
} json_token_state_t;


struct _json_parser_t {
	char* buffer;
	int buflen;
	int bufindex;
	json_token_state_t state;
	char backslash;
	char unicode;
	int unihex;
	int numstate;
	int error_code;
	char* error_message;

	int stack_top;
	int stack_depth;
	json_object_t value_stack[256];
	int state_stack[256];
};

static inline int whitespace(int c)
{
	return ((c == ' ') || (c == '\r') || (c == '\n') || (c == '\t'));
}

json_parser_t* json_parser_create()
{
	json_parser_t* parser = (json_parser_t*) malloc(sizeof(json_parser_t));
	if (parser == NULL) {
		return NULL;
	}

	memset(parser, 0, sizeof(*parser));

        json_parser_reset(parser);

	return parser;
}

void json_parser_destroy(json_parser_t* parser)
{
	if (parser != NULL) {
		if (parser->buffer != NULL) {
			free(parser->buffer);
		}
		if (parser->error_message != NULL) {
			free(parser->error_message);
		}
		free(parser);
	}
}

int json_parser_errno(json_parser_t* parser)
{
	return parser->error_code;
}

char* json_parser_errstr(json_parser_t* parser)
{
	return parser->error_message;
}

static void json_parser_set_error(json_parser_t* parser, int error, const char* message)
{
	parser->error_code = error;
	parser->error_message = json_strdup(message);
}

static int json_parser_append(json_parser_t* parser, char c)
{
	if (parser->bufindex >= parser->buflen) {
		int newlen = 2 * parser->buflen;
		if (newlen == 0) {
			newlen = 128;
		}
		char* newbuf = (char*) malloc(newlen);
		if (newbuf == NULL) {
			return k_out_of_memory;
		}
		if (parser->buffer != NULL) {
			memcpy(newbuf, parser->buffer, parser->buflen);
			free(parser->buffer);
		}
		parser->buffer = newbuf;
		parser->buflen = newlen;
	}

	parser->buffer[parser->bufindex++] = (char) (c & 0xff);

	return k_continue;
}


enum { k_parse_done = -1, k_parse_value = 0, k_object_1, k_object_2, k_object_3, k_object_4, k_array_1, k_array_2 };

void json_parser_reset(json_parser_t* parser)
{
	parser->stack_top = 0;
	parser->stack_depth = 256;
	parser->state_stack[0] = k_parse_value;
	parser->state = k_do;
	parser->bufindex = 0;
	parser->unicode = 0;
	parser->unihex = 0;
	parser->backslash = 0;
	parser->error_code = 0;
	if (parser->error_message != NULL) {
		free(parser->error_message);
		parser->error_message = NULL;
	}
}

void json_parser_reset_buffer(json_parser_t* parser)
{
	parser->bufindex = 0;
}

//
// value = object | array | number | string | true | false | null
// object = object_start key_value object_rest object_end
// key_value = string : value
// object_rest = , key_value object_rest
// array = array_start value array_rest array_end
// array_rest = , array_rest

// parse_value + object_start -> object_1  & top_value = new object()
// object_1    + string       -> object_2
// object_2    + colon        -> object_3  & top_state = parse_value & parse
// object_3    + (value)      -> object_4  & object.set(key,top_value)
// object_4    + object_end   -> (top_state)
// object_4    + comma        -> object_1
//
// parse_value + array_start  -> array_1  & top_value = new array()
// array_1     + (value)      -> array_2  & array.push(top_value)
// array_2     + colon        -> array_1  & top_state = parse_value & parse
// array_2     + array_end    -> (top_state)


static int json_parser_token(json_parser_t* parser, int token)
{
	int state = parser->state_stack[parser->stack_top];
	double d;
	json_object_t value;
	json_object_t key;
	json_object_t object;
	int r = k_continue;

	switch (state) {

	case k_parse_done:
		r = k_parsing_error;
		break;

	case k_parse_value:

		switch (token) {

		case k_object_start:
			parser->value_stack[parser->stack_top] = json_object_create();
			parser->state_stack[parser->stack_top] = k_object_1;
			parser->state_stack[++parser->stack_top] = k_parse_value;
			if (parser->stack_top >= parser->stack_depth) {
				r = k_stack_overflow;
			}
			break;

		case k_object_end:
			if (parser->stack_top > 0 && parser->state_stack[parser->stack_top-1] == k_object_1) {
                                parser->stack_top--;
                                parser->state_stack[parser->stack_top] = k_object_4;
				r = json_parser_token(parser, token);
			} else {
				r = k_parsing_error;
			}
			break;

		case k_array_start:
			parser->value_stack[parser->stack_top] = json_array_create();
			parser->state_stack[parser->stack_top] = k_array_1;
			parser->state_stack[++parser->stack_top] = k_parse_value;
			if (parser->stack_top >= parser->stack_depth) {
				r = k_stack_overflow;
			}
			break;

		case k_array_end:
			if (parser->stack_top > 0 && parser->state_stack[parser->stack_top-1] == k_array_1) {
                                parser->stack_top--;
                                parser->state_stack[parser->stack_top] = k_array_2;
				r = json_parser_token(parser, token);
			} else {
				r = k_parsing_error;
			}
			break;

		case k_string:
			parser->value_stack[parser->stack_top] = json_string_create(parser->buffer);
			json_parser_reset_buffer(parser);
			if (parser->stack_top > 0) {
				parser->stack_top--;
				r = json_parser_token(parser, token);
			} else {
				parser->state_stack[parser->stack_top] = k_parse_done;
			}
			break;

		case k_number:
			d = atof(parser->buffer); // FIXME: use strtod
			parser->value_stack[parser->stack_top] = json_number_create(d);
			json_parser_reset_buffer(parser);
			if (parser->stack_top > 0) {
				parser->stack_top--;
				r = json_parser_token(parser, token);
			} else {
				parser->state_stack[parser->stack_top] = k_parse_done;
			}

			break;

		case k_true:
			parser->value_stack[parser->stack_top] = json_true();
			json_parser_reset_buffer(parser);
			if (parser->stack_top > 0) {
				parser->stack_top--;
				r = json_parser_token(parser, token);
			} else {
				parser->state_stack[parser->stack_top] = k_parse_done;
			}
			break;

		case k_false:
			parser->value_stack[parser->stack_top] = json_false();
			json_parser_reset_buffer(parser);
			if (parser->stack_top > 0) {
				parser->stack_top--;
				r = json_parser_token(parser, token);
			} else {
				parser->state_stack[parser->stack_top] = k_parse_done;
			}
			break;

		case k_null:
			parser->value_stack[parser->stack_top] = json_null();
			json_parser_reset_buffer(parser);
			if (parser->stack_top > 0) {
				parser->stack_top--;
				r = json_parser_token(parser, token);
			} else {
				parser->state_stack[parser->stack_top] = k_parse_done;
			}
			break;

		default:
			r = k_parsing_error;
			break;
		}
		break;

	case k_object_1:
		if (token == k_string) {
			// keep the key on the stack by increase the stack top.
			parser->stack_top++;
			if (parser->stack_top >= parser->stack_depth) {
				r = k_stack_overflow;
			}
			parser->state_stack[parser->stack_top] = k_object_2;
		} else {
			r = k_parsing_error;
		}
		break;

	case k_object_2:
		if (token == k_colon) {
			parser->state_stack[parser->stack_top] = k_object_3;
			parser->state_stack[++parser->stack_top] = k_parse_value;
			if (parser->stack_top >= parser->stack_depth) {
				r = k_stack_overflow;
			}
		} else {
			r = k_parsing_error;
		}
		break;

	case k_object_3:
		switch (token) {
		case k_object_end:
		case k_array_end:
		case k_string:
		case k_number:
		case k_true:
		case k_false:
		case k_null:
			value = parser->value_stack[parser->stack_top+1];
			key = parser->value_stack[parser->stack_top--];
			object = parser->value_stack[parser->stack_top];
			json_object_set(object, json_string_value(key), value);
			json_unref(key);
			json_unref(value);
			parser->state_stack[parser->stack_top] = k_object_4;
			break;

		default:
			r = k_parsing_error;
			break;
		}
		break;

	case k_object_4:
		switch (token) {
		case k_object_end:
			if (parser->stack_top > 0) {
				parser->stack_top--;
				r = json_parser_token(parser, token);
			} else {
				parser->state_stack[parser->stack_top] = k_parse_done;
			}
			break;

		case k_comma:
			parser->state_stack[parser->stack_top] = k_object_1;
			parser->state_stack[++parser->stack_top] = k_parse_value;
			if (parser->stack_top >= parser->stack_depth) {
				r = k_stack_overflow;
			}
			break;

		default:
			r = k_parsing_error;
			break;
		}
		break;

	case k_array_1:
		switch (token) {
		case k_object_end:
		case k_array_end:
		case k_string:
		case k_number:
		case k_true:
		case k_false:
		case k_null:
			value = parser->value_stack[parser->stack_top+1];
			object = parser->value_stack[parser->stack_top];
			json_array_push(object, value);
                        json_unref(value);
			parser->state_stack[parser->stack_top] = k_array_2;
			break;

		default:
			r = k_parsing_error;
			break;
		}
		break;

	case k_array_2:
		switch (token) {
		case k_comma:
			parser->state_stack[parser->stack_top] = k_array_1;
			parser->state_stack[++parser->stack_top] = k_parse_value;
			if (parser->stack_top >= parser->stack_depth) {
				r = k_stack_overflow;
			}
			break;

		case k_array_end:
			if (parser->stack_top > 0) {
				parser->stack_top--;
				r = json_parser_token(parser, token);
			} else {
				parser->state_stack[parser->stack_top] = k_parse_done;
			}
			break;

		default:
			r = k_parsing_error;
			break;
		}
		break;
	};

	return r;
}

static int json_parser_unicode(json_parser_t* parser, char c)
{
	int r = k_continue;
	char v = 0;
	if (('0' <= c) && (c <= '9')) {
		v = c - '0';
	} else if (('a' <= c) && (c <= 'f')) {
		v = 10 + c - 'a';
	} else if (('A' <= c) && (c <= 'F')) {
		v = 10 + c - 'A';
	}
	switch (parser->unicode) {
	case 1: parser->unihex = (v & 0x0f);
		break;

	case 2: parser->unihex = (parser->unihex << 4) | (v & 0x0f);
		break;

	case 3: parser->unihex = (parser->unihex << 4) | (v & 0x0f);
		break;

	case 4: parser->unihex = (parser->unihex << 4) | (v & 0x0f);
		if ((0 <= parser->unihex) && (parser->unihex <= 0x007f)) {
			r = json_parser_append(parser, (char) (parser->unihex & 0x007f));
		} else if (parser->unihex <= 0x07ff) {
			unsigned char b1 = 0xc0 | (parser->unihex & 0x07c0) >> 6;
			unsigned char b2 = 0x80 | (parser->unihex & 0x003f);
			r = json_parser_append(parser, (char) b1);
			if (r != k_continue) return r;
			r = json_parser_append(parser, (char) b2);
		} else if (parser->unihex <= 0xffff) {
			unsigned char b1 = 0xe0 | (parser->unihex & 0xf000) >> 12;
			unsigned char b2 = 0x80 | (parser->unihex & 0x0fc0) >> 6;
			unsigned char b3 = 0x80 | (parser->unihex & 0x003f);
			r = json_parser_append(parser, (char) b1);
			if (r != k_continue) return r;
			r = json_parser_append(parser, (char) b2);
			if (r != k_continue) return r;
			r = json_parser_append(parser, (char) b3);
		}
		break;
	}
	return r;
}

static int json_parser_feed_string(json_parser_t* parser, char c)
{
	int r = k_continue;

	if (parser->unicode > 0) {
		r = json_parser_unicode(parser, c);
		if (parser->unicode++ == 4) {
			parser->unicode = 0;
		}

	} else if (parser->backslash) {
		parser->backslash = 0;
		switch (c) {
		case 'n': r = json_parser_append(parser, '\n');
			break;
		case 'r': r = json_parser_append(parser, '\r');
			break;
		case 't': r = json_parser_append(parser, '\t');
			break;
		case 'b': r = json_parser_append(parser, '\b');
			break;
		case 'f': r = json_parser_append(parser, '\f');
			break;
		case 'u': parser->unicode = 1;
			break;
		default: r = json_parser_append(parser, c);
			break;
		}

	} else if (c == '\\') {
		parser->backslash = 1;

	} else if (c == '"') {
		r = json_parser_append(parser, 0);
		if (r != k_continue) {
			return r;
		}
		r = json_parser_token(parser, k_string);
		parser->state = k_do;

	} else {
		r = json_parser_append(parser, c);
	}

	return r;
}

///////////////////////////////////////////////////////////////////
//
//                  +------------+
//     0     1     2|    4     5 |6               10
//  ()-+---+-+-[0]-++-[.]+[0-9]+-+----------------+--()
//     |   | |     |     +-----+ |                |
//     |   | |     |            [e]               |
//     +[-]+ +[1-9]+[0-9]+       |                |
//                 |     |       |  +[+]+         |
//                 +-----+       +--+---+---+[0-9]+
//                 3             7  +[-]+ 8 +-----+
//                                                9
//
//-----------------------------------------------------------------
//
//   0 + ''    -> 1
//   0 + '-'   -> 1
//   1 + '0'   -> 2
//   1 + [1-9] -> 3
//   3 + [0-9] -> 3
//   3 + ''    -> 2
//   2 + ''    -> 6
//   2 + '.'   -> 4
//   4 + [0-9] -> 5
//   5 + ''    -> 4
//   5 + ''    -> 6
//   6 + ''    -> 10
//   6 + 'e'   -> 7
//   6 + 'E'   -> 7
//   7 + ''    -> 8
//   7 + '-'   -> 8
//   7 + '+'   -> 8
//   8 + [0-9] -> 9
//   9 + ''    -> 10
//   9 + [0-9] -> 9
//
//-----------------------------------------------------------------
//
//   0_1      + '-'   -> 1
//   1        + '0'   -> 2_6_10
//   1        + [1-9] -> 2_3_6_10
//   0_1      + '0'   -> 2_6_10
//   0_1      + [1-9] -> 2_3_6_10
//   2_6_10   + '.'   -> 4
//   2_6_10   + 'e'   -> 7_8
//   2_3_6_10 + [0-9] -> 2_3_6_10
//   2_3_6_10 + '.'   -> 4
//   2_3_6_10 + 'e'   -> 7_8
//   7_8      + '+'   -> 7
//   7_8      + '-'   -> 7
//   7_8      + [0-9] -> 9_10
//   7        + [0-9] -> 9_10
//   9_10     + [0-9] -> 9_10
//   4        + [0-9] -> 5_6_10
//   5_6_10   + [0-9] -> 5_6_10
//   5_6_10   + 'e'   -> 7_8
//
//-----------------------------------------------------------------

enum { _error = 0, _0_1, _1, _2_6_10, _2_3_6_10, _7_8, _7, _9_10, _4, _5_6_10, _state_last };

enum { _other = 0, _min, _plus, _zero, _d19, _dot, _e, _input_last };

char _numtrans[_state_last][_input_last] = {
//        _other, _min,   _plus,  _zero,     _d19,      _dot,   _e,
	{ _error, _error, _error, _error,    _error,    _error, _error },   // _error
	{ _error, _1,     _error, _2_6_10,   _2_3_6_10, _error, _error },   // _0_1
	{ _error, _error, _error, _2_6_10,   _2_3_6_10, _error, _error },   // _1
	{ _error, _error, _error, _error,    _error,    _4,     _7_8,  },   // _2_6_10
	{ _error, _error, _error, _2_3_6_10, _2_3_6_10, _4,     _7_8,  },   // _2_3_6_10
	{ _error, _7,     _7,     _9_10,     _9_10,     _error, _error },   // _7_8
	{ _error, _error, _error, _9_10,     _9_10,     _error, _error },   // _7
	{ _error, _error, _error, _9_10,     _9_10,     _error, _error },   // _9_10
	{ _error, _error, _error, _5_6_10,   _5_6_10,   _error, _error },   // _4
	{ _error, _error, _error, _5_6_10,   _5_6_10,   _error, _7_8,  }};  // _5_6_10

char _endstate[_state_last] = { 0, 0, 0, 1, 1, 0, 0, 1, 0, 1 };

static int json_numinput(json_parser_t* parser, char c)
{
        (void) parser;
	if (('1' <= c) && (c <= '9')) return _d19;
	if (c == '0') return _zero;
	if (c == '.') return _dot;
	if (c == '-') return _min;
	if (c == '+') return _plus;
	if (c == 'e') return _e;
	if (c == 'E') return _e;
	return _other;
}

static int json_parser_feed_number(json_parser_t* parser, char c)
{
	int r = k_token_error;

	int input = json_numinput(parser, c);

	int prevstate = parser->numstate;

	parser->numstate = _numtrans[prevstate][input];

	if (parser->numstate != _error) {
		r = json_parser_append(parser, c);

	} else {
		if (_endstate[prevstate]) {
			r = json_parser_append(parser, 0);
			if (r != k_continue) {
				return r;
			}
			r = json_parser_token(parser, k_number);
			if (r != k_continue) {
				return r;
			}
			//
			parser->state = k_do;
			r = json_parser_feed_one(parser, c);
		} else {
			r = k_token_error;
		}
	}

	return r;
}

static int json_parser_feed_true(json_parser_t* parser, char c)
{
	int r = json_parser_append(parser, c);
	if (r != k_continue) {
		return r;
	}
	if (parser->bufindex < 4) {
		if (memcmp(parser->buffer, "true", parser->bufindex) != 0) {
			return k_token_error;
		}
	} else if (parser->bufindex == 4) {
		if (memcmp(parser->buffer, "true", 4) == 0) {
			parser->state = k_do;
			return json_parser_token(parser, k_true);
		} else {
			return k_token_error;
		}
	}
	return k_continue;
}

static int json_parser_feed_false(json_parser_t* parser, char c)
{
	int r = json_parser_append(parser, c);
	if (r != k_continue) {
		return r;
	}
	if (parser->bufindex < 5) {
		if (memcmp(parser->buffer, "false", parser->bufindex) != 0) {
			return k_token_error;
		}
	} else if (parser->bufindex == 5) {
		if (memcmp(parser->buffer, "false", 5) == 0) {
			parser->state = k_do;
			return json_parser_token(parser, k_false);
		} else {
			return k_token_error;
		}
	}
	return k_continue;
}

static int json_parser_feed_null(json_parser_t* parser, char c)
{
	int r = json_parser_append(parser, c);
	if (r != k_continue) {
		return r;
	}
	if (parser->bufindex < 4) {
		if (memcmp(parser->buffer, "null", parser->bufindex) != 0) {
			return k_token_error;
		}
	} else if (parser->bufindex == 4) {
		if (memcmp(parser->buffer, "null", 4) == 0) {
			parser->state = k_do;
			return json_parser_token(parser, k_null);
		} else {
			return k_token_error;
		}
	}
	return k_continue;
}

static int json_parser_do(json_parser_t* parser, char c)
{
	int r = k_continue;

	if (whitespace(c)) {
		return k_continue;
	}

	switch (c) {
	case '{':
		parser->state = k_do;
		r = json_parser_token(parser, k_object_start);
		break;

	case '}':
		parser->state = k_do;
		r = json_parser_token(parser, k_object_end);
		break;

	case '[':
		parser->state = k_do;
		r = json_parser_token(parser, k_array_start);
		break;

	case ']':
		parser->state = k_do;
		r = json_parser_token(parser, k_array_end);
		break;

	case '"':
		parser->state = k_parsing_string;
		break;

	case 't':
		parser->state = k_parsing_true;
		r = json_parser_feed_true(parser, c);
		break;

	case 'f':
		parser->state = k_parsing_false;
		r = json_parser_feed_false(parser, c);
		return k_continue;

	case 'n':
		parser->state = k_parsing_null;
		r = json_parser_feed_null(parser, c);
		return k_continue;

	case '-': case 'O': case '0': case '1':
	case '2': case '3': case '4': case '5':
	case '6': case '7': case '8': case '9':
		parser->state = k_parsing_number;
		parser->numstate = _0_1;
		r = json_parser_feed_number(parser, c);
		break;

	case ',':
		parser->state = k_do;
		r = json_parser_token(parser, k_comma);
		break;

	case ':':
		parser->state = k_do;
		r = json_parser_token(parser, k_colon);
		break;

	case '\0':
		r = k_end_of_string;
		break;

	default:
		r = k_token_error;
		break;
	}

        if (r != k_continue) {
                char message[256];
                snprintf(message, 255, "invalid char: %c", c);
                message[255] = 0;
                json_parser_set_error(parser, 1, message);
        }

	return r;
}

int json_parser_feed_one(json_parser_t* parser, char c)
{
        int ret = k_token_error;

	switch (parser->state) {
	case k_do:
		ret = json_parser_do(parser, c);
                break;

	case k_parsing_string:
		ret = json_parser_feed_string(parser, c);
                break;

	case k_parsing_number:
		ret = json_parser_feed_number(parser, c);
                break;

	case k_parsing_false:
		ret = json_parser_feed_false(parser, c);
                break;

	case k_parsing_true:
		ret = json_parser_feed_true(parser, c);
                break;

	case k_parsing_null:
		ret = json_parser_feed_null(parser, c);
                break;
	}

	return ret;
}

int json_parser_feed(json_parser_t* parser, const char* buffer, int len)
{
	int r = 0;
	while (len-- > 0) {
		r = json_parser_feed_one(parser, *buffer++);
		if (r != 0) {
			break;
		}
	}
	return r;
}

int json_parser_done(json_parser_t* parser)
{
	return parser->state_stack[parser->stack_top] == k_parse_done;
}

json_object_t json_parser_eval(json_parser_t* parser, const char* s)
{
	int r = 0;

	json_parser_reset(parser);

	do {
		r = json_parser_feed_one(parser, *s);
		if (r != k_continue) {
			switch (r) {
			case k_out_of_memory:
				json_parser_set_error(parser, -1, "Out of memory");
				return json_null();

			case k_token_error:
				json_parser_set_error(parser, -2, "Unexpected character");
				return json_null();

			case k_parsing_error:
				json_parser_set_error(parser, -2, "Parse error");
				return json_null();
			}
		}

	} while (*s++);

	if (!json_parser_done(parser)) {
		json_parser_set_error(parser, -1, "Unexpected end while parsing");
		return json_null();
	}

	return parser->value_stack[0];
}

json_object_t json_parser_result(json_parser_t* parser)
{
	return parser->value_stack[0];
}

