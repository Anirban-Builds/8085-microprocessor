#include "main_8085.h"

Hashmap *opcode_mp = NULL, *sessions_mp = NULL;
mutex_t session_lock;

int main(){
    srand(time(NULL));
    load_env(".env");
    opcode_mp = map_init();
    sessions_mp = map_init();
     MUTEX_INIT(session_lock);
    for(int i=0; i<om_size; i++) map_set(opcode_mp, om[i].name, (void*)(intptr_t)om[i].op);
    run_server();
    map_kill(opcode_mp);
    map_kill(sessions_mp);
    return 0;
}