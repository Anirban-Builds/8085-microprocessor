#include "utils_8085.h"

void load_env(char *filename){
    FILE *f = fopen(filename, "r");
    if (!f) {
        // printf("No .env file found, skipping (%s)\n", filename);
        return;
    }
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';

        if (line[0] == '\0' || line[0] == '#') continue;

        char *eq = strchr(line, '=');
        if (!eq) continue;

        *eq = '\0';
        char *key = line;
        char *value = eq + 1;

        SET_ENV(key, value);
    }
}

int is_allowed_origin(char* request)
{
    char* origin_start = STRCASESTR(request, "Origin: ");
    if (!origin_start) {
        return 0;
    }
    origin_start += 8;

    char origin[256] = {0};
    int i = 0;
    while (origin_start[i] != '\r' && origin_start[i] != '\n' && i < 255) {
        origin[i] = origin_start[i];
        i++;
    }
    origin[i] = '\0';

    if (strcmp(origin, getenv("CORS")) == 0){
        return 1;   // Allowed
    }

    return 0;   // Blocked
}

int is_edit_msg(char* buffer){
    if(strstr(buffer, "\"type\":\"edit\"")){
    return 1;
    }
return 0;
}

char* strcasestr_custom(char* str, char* substr) {
    if (!*substr) return (char*)str;
    for (; *str; str++) {
        const char *h = str, *n = substr;
        while (*h && *n && tolower((unsigned char)*h) == tolower((unsigned char)*n)) {
            h++; n++;
        }
        if (!*n) return (char*)str;
    }
    return NULL;
}

Hashmap* map_init(){
    Hashmap* mp = malloc(sizeof(Hashmap));
    if(!mp) return NULL; 
    mp->cap = INIT_CAP;
    mp->num = 0;
    mp->elem = calloc(mp->cap, sizeof(hme));
    if(!mp->elem){
        free(mp); 
        return NULL;     
    }
    return mp;
}

static uint64_t hashfunc(const char* key) {
    uint64_t hash = FNV_OFFSET;
    while (*key) {
        hash ^= (uint8_t)(*key);
        hash *= FNV_PRIME;
        key++;
    }
    return hash;
}

void gen_id(char *out){
    const char hex[] = "0123456789abcdef";
    for(int i = 0; i < 32; i++) out[i] = hex[rand() % 16];
    out[32] = '\0';
}

static bool map_resize(Hashmap* map) {
    size_t old_cap = map->cap;
    hme* old_entries = map->elem;
    size_t new_cap = old_cap * 2;
    hme* new_entries = calloc(new_cap, sizeof(hme));
    if (new_entries == NULL) return false;
    map->cap = new_cap;
    map->elem = new_entries;
    for (size_t i = 0; i < old_cap; i++) {
        if (old_entries[i].key != NULL) {
            size_t index = hashfunc(old_entries[i].key) & (new_cap - 1);
            while (map->elem[index].key != NULL) index = (index + 1) & (new_cap - 1);
            map->elem[index].key = old_entries[i].key;
            map->elem[index].val = old_entries[i].val;
        }
    }
    free(old_entries);
    return true;
}

void map_kill(Hashmap* mp) {
    if (!mp) return; 
    for (size_t i = 0; i < mp->cap; i++) {
        if (mp->elem[i].key) free(mp->elem[i].key);  
    }
    free(mp->elem); free(mp);         
}

bool map_remove(Hashmap* mp, const char* key){
    if(!mp->cap) return false;
    size_t idx = (size_t)(hashfunc(key) & (uint64_t)(mp->cap-1));
    while(mp->elem[idx].key){
        if(!strcmp(key, mp->elem[idx].key)){
            free(mp->elem[idx].key);
            free(mp->elem[idx].val);      
            // frees the CPU* — only correct for the sessions map, NOT the opcode map
            mp->elem[idx].key = NULL;
            mp->elem[idx].val = NULL;
            mp->num--;
            return true;
        }
        idx = (idx+1) & (mp->cap-1);
    }
    return false; 
}

bool map_set(Hashmap* map, const char* key, void* value) {
    if (!value) {
        fprintf(stderr, "Error"); return false; 
    }

    if (map->num + 1 > (map->cap * 3) / 4) {
        if (!map_resize(map)) return false;
    }

    size_t index = hashfunc(key) & (map->cap - 1);
    
    while (map->elem[index].key != NULL) {
        if (strcmp(map->elem[index].key, key) == 0) {
            map->elem[index].val = value; 
            return true;
        }
        index = (index + 1) & (map->cap - 1);
    }

    map->elem[index].key = strdup(key);
    if (!map->elem[index].key) return false;
    map->elem[index].val = value;
    map->num++;
    return true;
}

void* map_get(Hashmap* map, const char* key) {
    if (map->num == 0) return NULL; 
    size_t index = hashfunc(key) & (map->cap - 1);
    size_t start_index = index;
    while (map->elem[index].key != NULL) {
        if (strcmp(map->elem[index].key, key) == 0) return map->elem[index].val; 
        index = (index + 1) & (map->cap - 1);
        if (index == start_index) break;
    }
    return NULL; 
}
