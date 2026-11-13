#ifndef _COMMON_8085_
#define _COMMON_8085_

#ifdef _WIN32
    #include <winsock2.h>
    #include <windows.h>
    #include <ws2tcpip.h>
    #include <direct.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef int socklen_t;
    typedef CRITICAL_SECTION mutex_t;
    #define close_socket closesocket
    #define MUTEX_INIT(m) InitializeCriticalSection(&(m))
    #define MUTEX_LOCK(m) EnterCriticalSection(&(m))
    #define MUTEX_UNLOCK(m) LeaveCriticalSection(&(m))
    #define SHUTDOWN_BOTH(fd) shutdown((fd), SD_BOTH)
    #define SET_ENV(key, value) _putenv_s(key, value)
    char* strcasestr_custom(char*, char*);
    #define STRCASESTR strcasestr_custom
#else
    #include <pthread.h>
    #include <unistd.h>
    #include <arpa/inet.h>
    #include <sys/socket.h>
    #include <sys/stat.h>
    #include <netinet/in.h>
    #define close_socket close
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    typedef int SOCKET;
    typedef pthread_mutex_t mutex_t;
    #define MUTEX_INIT(m) pthread_mutex_init(&(m), NULL)
    #define MUTEX_LOCK(m) pthread_mutex_lock(&(m))
    #define MUTEX_UNLOCK(m) pthread_mutex_unlock(&(m))
    #define SHUTDOWN_BOTH(fd) shutdown((fd), SHUT_RDWR)
    #define SET_ENV(key, value) setenv(key, value, 1)
    #define STRCASESTR strcasestr
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#define PORT 7860
#define MAX_CLIENTS 10
#define BUFFER_SIZE (2*1024)
#endif