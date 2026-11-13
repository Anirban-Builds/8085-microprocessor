#include "server_8085.h"

#ifdef _WIN32
DWORD WINAPI client_thread(LPVOID arg) // this is the function name
#else
void* client_thread(void* arg)
#endif
{
    SOCKET client_fd = *(SOCKET*)arg;
    free(arg);
    char *buffer = malloc(BUFFER_SIZE);
    int total = 0, bytes = 0;
    while((bytes = recv(client_fd, buffer + total, BUFFER_SIZE - total - 1, 0)) > 0){
        total += bytes;
        // buffer[total] = '\0';
        char *header_end = strstr(buffer, "\r\n\r\n");
        if(!header_end) continue;
        char *cl = STRCASESTR(buffer, "Content-Length: ");
         if(cl){
            int content_length = atoi(cl + 16);
            int header_len = (header_end + 4) - buffer;
            if (header_len + content_length >= BUFFER_SIZE) {
                handle_response(client_fd, "failure", 413, "Payload Too Large", "");
                free(buffer);
                close_socket(client_fd);
                return 0;
            }
            if(total >= header_len + content_length) break;
            } else break;
    }
     if(total > 0){
        buffer[total] = '\0';
        route_http_request(client_fd, buffer);
        }
    free(buffer);
    close_socket(client_fd);
    return 0;
}

int run_server(){

    setbuf(stdout, NULL);

    #ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) !=0) return 1;
    #endif

    SOCKET server_fd, client_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    if((server_fd = (socket(AF_INET, SOCK_STREAM, 0)))== INVALID_SOCKET){
        perror("Socket Creation Failure !");
        return 1;
    }

    int opt=1;
    #ifdef _WIN32
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
    #else
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    #endif

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Port Binding Failure !");
        return 1;
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("Listening  Failure !");
        return 1;
    }

    printf("8085 processor server running on port %d...\n", PORT);

    while(1){
        client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if(client_fd == INVALID_SOCKET) continue;

        SOCKET* p = malloc(sizeof(SOCKET));
        *p = client_fd;

        #ifdef _WIN32
        CreateThread(NULL, 0, client_thread, p, 0, NULL);
        #else
        pthread_t tid;
        pthread_create(&tid, NULL, client_thread, p);
        pthread_detach(tid);
        #endif
    }
    // close_socket(server_fd);
    #ifdef _WIN32
    WSACleanup();
    #endif

    return 0;
}