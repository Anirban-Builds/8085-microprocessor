#ifndef _HANDLER_8085_
#define _HANDLER_8085_

#include "common_8085.h"
#include "cpu_8085.h"

void handle_preflight(SOCKET);
void handle_response(SOCKET, char *, int, char*, char*);
void handle_index(SOCKET, char*, char*);
void handle_exec(SOCKET, char*, char*);
void handle_cpu_reset(SOCKET, char*, char*);
#endif