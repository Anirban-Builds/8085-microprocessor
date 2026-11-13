#ifndef _UTILS_8085_
#define _UTILS_8085_
#include "common_8085.h"

#define INIT_CAP 16

#define FNV_OFFSET 14695981039346656037UL
#define FNV_PRIME 1099511628211UL

typedef struct{
 char * key;
 void* val;
}hme;

typedef struct{
 hme* elem;
 size_t cap;
 size_t num;
}Hashmap;

int is_allowed_origin(char*);
void load_env(char *);
char* strcasestr_custom(char*, char*);
Hashmap* map_init();
void map_kill(Hashmap*);
bool map_set(Hashmap*, const char*, void*);
void* map_get(Hashmap*, const char*);
bool map_remove(Hashmap*, const char*);
void gen_id(char*);

#endif
