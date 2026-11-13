#ifndef _ROUTER_8085_
#define _ROUTTER_8085_
#include "handler_8085.h"
typedef struct {
    const char* method;
    const char* path;
    void (*handler_func)(SOCKET, char*, char*);
} Route;
void route_http_request(SOCKET, char*);
#endif