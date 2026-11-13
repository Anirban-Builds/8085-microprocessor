#include "handler_8085.h"

#define HANDLER_ARGS SOCKET fd, char* body, char* buffer

char res[2048];
char json_body[2048];

void handle_preflight(SOCKET fd){
    char res[256];
    int res_len = snprintf(res, sizeof(res),
        "HTTP/1.1 204 No Content\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, DELETE, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n\r\n"
    );
    if (res_len < 0) return;
    if ((size_t)res_len >= sizeof(res)) {
        res_len = sizeof(res) - 1;
    }
    send(fd, res, res_len, 0);
}

void handle_response(SOCKET fd, char *status, int code, char *message, char* data){
    if(!data || !strlen(data)) data = "null";
    snprintf(json_body, sizeof(json_body),
    "{\"status\": \"%s\", \"message\": \"%s\", \"code\": %d, \"data\": %s}",
    status, message, code, data);

   int res_len = snprintf(res, sizeof(res),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: application/json; charset=utf-8\r\n"
        "Content-Length: %d\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Connection: close\r\n\r\n%s",
        code, message, strlen(json_body), json_body
    );
    if (res_len < 0) return;
    if ((size_t)res_len >= sizeof(res)) {
        res_len = sizeof(res) - 1;
    }
    send(fd, res, res_len, 0);
}

void handle_index(HANDLER_ARGS){
    handle_response(fd, "success", 200, "Server is Live ✅", "");
}

void handle_exec(HANDLER_ARGS){
    char *q = strstr(body, "\"code\"");
    if (!q){
        handle_response(fd, "ERROR", 400, "Code not found", "");
        return;
    }
    q+=8;
    char temp[BUFFER_SIZE];
    int i = 0;
    while(*q != '"'){
        if(*q == '\\' && *(q+1)== 'n'){
            temp[i++] = '\n';
            q+=2;
        }
        else if(*q == '\0' || i >= BUFFER_SIZE-1) break;
        else{
        temp[i++] = *q; q++;
        }
    }
    temp[i] = '\0';
    if(!strlen(temp)){
        handle_response(fd, "ERROR", 400, "No instruction provided", "");
        return;
    }
    // extract session id
    char session_id[33] ="";
    char *sid = strstr(body, "\"session_id\"");
    if(sid){
        sid += 14; int i=0;
        while(i<32 && *sid != '\0' && *sid != '"') session_id[i++] = *sid++;
        session_id[i] = '\0';
    }
    // extract start addr
    char *addr = strstr(body, "\"start_addr\"");
    // assembler
    // build opcode hashmap
    extern Hashmap *opcode_mp;
    if(!opcode_mp){
        handle_response(fd, "ERROR", 500, "Failed to build map", "");
        return;
    }
    Asmres res;
    memset(res.bytes, 0, sizeof(res.bytes)); // init
    res.len = 0;
    res.ss = 1;
    res.num_seg = 1;
    res.seg[0] = (Segment){.start_addr = START_ADDR, .offset = 0, .len = 0};
    res.err[0] = '\0'; // init
    res.cur_addr = addr ? (uint16_t)strtol(addr, NULL, 16) : START_ADDR;
    assemble(temp, &res, opcode_mp);
    if(!res.ss){
        handle_response(fd, "ERROR", 400, "Invalid insruction", res.err);
        return;
    }
    CPU* cpu = get_cpu(session_id);
    if(!cpu){
        handle_response(fd, "ERROR", 400, "CPU search or creation failed", "");
        return;
    }
    cpu_load(cpu, res.bytes, res.num_seg, res.seg, res.cur_addr);
    if(cpu->err){
        handle_response(fd, "ERROR", 400, "Load failed", cpu->err_msg);
        return;
    }
    cpu_run(cpu, 10000); // 10000 max steps
    if(cpu->err || !cpu->halted){
        handle_response(fd, "ERROR", 400, cpu->err ?
        "Execution failed": "Halting failed", cpu->err_msg);
        return;
    }
    char res_json[2048];
    char stack_json[1024];
    int pos = snprintf(stack_json, sizeof(stack_json), "[");
    uint16_t stack_addr = 0xFFFF;
    while(stack_addr >=cpu->SP){
        pos += snprintf(stack_json+pos, sizeof(stack_json)-pos,
        "%s\"%02X\"", stack_addr == 0xFFFF ? "": ",", cpu->mem[stack_addr]);
        if(pos >= (int)sizeof(stack_json)-8) break; // safety check
        addr--;
    }
    snprintf(stack_json + pos, sizeof(stack_json) - pos, "]");
    snprintf(res_json, sizeof(res_json),
    "{\"session_id\" : \"%s\", "
    "\"cpu\": {\"A\":%d, \"B\":%d, \"C\":%d, \"D\":%d, \"E\":%d, \"F\":%d, \"H\":%d, \"L\":%d, "
    "\"PC\":%d, \"SP\":%d, \"halted\":%d}, "
    "\"flags\": {\"S\":%d, \"Z\":%d, \"AC\":%d, \"P\":%d, \"CY\":%d}, "
    "\"stack \" : %s }",
    session_id,
    cpu->A, cpu->B, cpu->C, cpu->D, cpu->E, cpu->F, cpu->H, cpu->L, cpu->PC, cpu->SP,
    cpu->halted ? 1 : 0,
    (cpu->F>>7)&1, (cpu->F>>6)&1, (cpu->F>>4)&1, (cpu->F>>2)&1, (cpu->F>>0)&1,
    stack_json);
    handle_response(fd, "OK", 200, "Assembled and executed", res_json);
}

void handle_cpu_reset(HANDLER_ARGS){
    char *q = strstr(body, "\"session_id\"");
    if(!q){
        handle_response(fd, "ERROR", 400, "session_id not found", "");
        return;
    }
    q += 14;
    char session_id[33] = "";
    int i = 0;
    while(*q != '"' && *q != '\0' && i < 32) session_id[i++] = *q++;
    session_id[i] = '\0';

    extern Hashmap* sessions_mp;
    extern mutex_t session_lock;

    MUTEX_LOCK(session_lock);
    CPU *cpu = (CPU*)map_get(sessions_mp, session_id);
    if(!cpu){
        MUTEX_UNLOCK(session_lock);
        handle_response(fd, "ERROR", 404, "Session not found", "");
        return;
    }
    cpu_init(cpu); // wipe state back to fresh, same CPU*, same session_id
    MUTEX_UNLOCK(session_lock);

    handle_response(fd, "OK", 200, "CPU reset", "");
}