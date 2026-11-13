#define _POSIX_C_SOURCE 200809L
#include "assembler_8085.h"

static int reg_code(const char *tok) {
    if (!strcasecmp(tok, "A")) return 7;
    if (!strcasecmp(tok, "B")) return 0;
    if (!strcasecmp(tok, "C")) return 1;
    if (!strcasecmp(tok, "D")) return 2;
    if (!strcasecmp(tok, "E")) return 3;
    if (!strcasecmp(tok, "H")) return 4;
    if (!strcasecmp(tok, "L")) return 5;
    if (!strcasecmp(tok, "M")) return 6;
    return -1;
}

static void to_upper(char *s) {
    for (; *s; s++) *s = toupper((unsigned char)*s);
}

static int isvalid_hex(char c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F');
}

static size_t parse_opd(const char *tok, int* success, int num_bytes) {
    char buf[32];
    strncpy(buf, tok, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    size_t len = strlen(buf);
    if(num_bytes == 1){
        if (len == 3 && (buf[len - 1] == 'H') && (isvalid_hex(buf[0]) && isvalid_hex(buf[1]))){
        buf[len - 1] = '\0';
        return (uint8_t)strtol(buf, NULL, 16);
    }
    }
    if(num_bytes ==2){
        if(len == 5 && buf[4] == 'H' && isvalid_hex(buf[0]) &&
        isvalid_hex(buf[1]) && isvalid_hex(buf[2]) && isvalid_hex(buf[3])){
        buf[4] = '\0';
        return (uint16_t)strtoul(buf, NULL, 16);
    }
    }
    *success = 0;
    return 0;
}

static void single_src_opcode(Asmres* res, char** tok_ptr,
    int line_num, Hashmap* mp, const char* mnc){
    char *src = strtok_r(NULL, " \t,", tok_ptr);
    int s = src ? reg_code(src) : -1;
            if(s < 0){
            res->ss = 0;
            snprintf(res->err, sizeof(res->err),
            "Invalid register for %s at line %d", mnc, line_num);
            return;
        }
    res->bytes[res->len++] = (!strcmp(mnc, "INR") || !strcmp(mnc, "DCR")) ?
    ((int)(intptr_t)map_get(mp, mnc) | (s << 3)): ((int)(intptr_t)map_get(mp, mnc) | s);
}

void assemble(char* s, Asmres* res, Hashmap* mp){

    char temp[BUFFER_SIZE];
    strncpy(temp, s, BUFFER_SIZE-1);
    temp[BUFFER_SIZE-1] = '\0';
    char* line_ptr = NULL;
    char* line = strtok_r(temp, "\n", &line_ptr);
    int line_num = 0;
    while(line){
        line_num++;
        char* comment = strchr(line, ';');
        if(comment) *comment = '\0';

        char cur[256];
        strncpy(cur, line, sizeof(cur)-1);
        cur[sizeof(cur)-1] = '\0';
        to_upper(cur);

        char* tok_ptr = NULL;
        char *mnc = strtok_r(cur, " \t,", &tok_ptr);
        if(!mnc){
            line = strtok_r(NULL, "\n", &line_ptr);
            continue;
        }
        if(!strcmp(mnc , "NOP")){
            Opcode *elem = opcode_lookup("NOP");
            res->bytes[res->len++] = elem->op;
        }
        else if(!strcmp(mnc , "HLT")){
            Opcode *elem = opcode_lookup("HLT");
            res->bytes[res->len++] = elem->op;
        }
        else if(!strcmp(mnc, "MVI") || !strcmp(mnc, "PUSH") || !strcmp(mnc, "POP") ||
         !strcmp(mnc, "INX")  || !strcmp(mnc, "DCX") || !strcmp(mnc, "DAD")  ||
         !strcmp(mnc, "LDAX") || !strcmp(mnc, "STAX")){
            char *reg = strtok_r(NULL, " \t,", &tok_ptr);
            char *imm = strtok_r(NULL, " \t,", &tok_ptr);

            char lookup[16];
            snprintf(lookup, sizeof(lookup), "%s %s", mnc, reg ? reg: "");
            Opcode *elem = opcode_lookup(lookup);
            if(!elem || !reg || (!strcmp(mnc, "MVI") && !imm)){
                res->ss = 0;
                snprintf(res->err, sizeof(res->err),
                "Invalid register for %s at line %d", mnc, line_num);
                return;
            }
            res->bytes[res->len++] = elem->op;
            if(!strcmp(mnc, "MVI")){
                uint8_t opd = parse_opd(imm, &res->ss, 1);
                if(!res->ss){
                res->ss = 0;
                snprintf(res->err, sizeof(res->err),
                "Invalid register for %s at line %d", mnc, line_num);
                return;
            }
                res->bytes[res->len++] = opd;
            }
        }
        else if(!strcmp(mnc, "MOV")){
            char *dest = strtok_r(NULL, " \t,", &tok_ptr);
            char *src = strtok_r(NULL, " \t,", &tok_ptr);
            int d = dest ? reg_code(dest) : -1;
            int s = src ? reg_code(src): -1;
            if(d < 0 || s < 0){
                res->ss = 0;
                snprintf(res->err, sizeof(res->err),
                "Invalid register for %s at line %d", "MOV", line_num);
                return;
            }

            res->bytes[res->len++] = BASE_MOV | (d << 3) | s;
        }
        else if(!strcmp(mnc, "ADD") || !strcmp(mnc, "ADC") || !strcmp(mnc, "SUB") ||
                !strcmp(mnc, "SBB") || !strcmp(mnc, "ANA") || !strcmp(mnc, "XRA") ||
                !strcmp(mnc, "ORA") || !strcmp(mnc, "CMP") || !strcmp(mnc, "INR") ||
                !strcmp(mnc, "DCR")){
            single_src_opcode(res, &tok_ptr, line_num, mp, mnc);
        }
        else if(!strcmp(mnc, "RET") || !strcmp(mnc, "RZ") || !strcmp(mnc, "RNZ") ||
        !strcmp(mnc, "RC") || !strcmp(mnc, "RNC") || !strcmp(mnc, "RP") ||
        !strcmp(mnc, "RM") || !strcmp(mnc, "RPE") || !strcmp(mnc, "RPO") ||
        !strcmp(mnc, "PCHL") || !strcmp(mnc, "XTHL") || !strcmp(mnc, "SPHL")) {
        Opcode *e = opcode_lookup(mnc);
        res->bytes[res->len++] = e->op;
    }
        else if (!strcmp(mnc, "ADI") || !strcmp(mnc, "ACI") || !strcmp(mnc, "SUI") ||
         !strcmp(mnc, "SBI") || !strcmp(mnc, "ANI") || !strcmp(mnc, "XRI") ||
         !strcmp(mnc, "ORI") || !strcmp(mnc, "CPI") || !strcmp(mnc, "IN")  ||
         !strcmp(mnc, "OUT")) {

         char *imm = strtok_r(NULL, " \t,", &tok_ptr);
         Opcode *elem = opcode_lookup(mnc);

         if (!elem || !imm){
            res->ss = 0;
            snprintf(res->err, sizeof(res->err),
            "Missing operand for %s at line %d", mnc, line_num);
            return;
         }

         res->bytes[res->len++] = elem->op;
         uint8_t opd = parse_opd(imm, &res->ss, 1);
         if (!res->ss) {
            res->ss = 0;
            snprintf(res->err, sizeof(res->err),
            "Invalid value for %s at line %d", mnc, line_num);
            return;
         }
         res->bytes[res->len++] = opd;
    }
        else if (!strcmp(mnc, "LDA")  || !strcmp(mnc, "STA")  || !strcmp(mnc, "LHLD") ||
         !strcmp(mnc, "SHLD") || !strcmp(mnc, "LXI")  || !strcmp(mnc, "JMP")  ||
         !strcmp(mnc, "CALL") || !strcmp(mnc, "JZ")   || !strcmp(mnc, "JNZ")  ||
         !strcmp(mnc, "JC")   || !strcmp(mnc, "JNC")  || !strcmp(mnc, "JP")   ||
         !strcmp(mnc, "JM")   || !strcmp(mnc, "JPE")  || !strcmp(mnc, "JPO")  ||
         !strcmp(mnc, "CZ")   || !strcmp(mnc, "CNZ")  || !strcmp(mnc, "CC")   ||
         !strcmp(mnc, "CNC")  || !strcmp(mnc, "CP")   || !strcmp(mnc, "CM")   ||
         !strcmp(mnc, "CPE")  || !strcmp(mnc, "CPO")) {

         char *reg = strtok_r(NULL, " \t,", &tok_ptr);
         char *addr = strtok_r(NULL, " \t,", &tok_ptr);
         char lookup[16];
         snprintf(lookup, sizeof(lookup), "%s%s%s", mnc, !strcmp(mnc, "LXI") ? " " : "",
         !strcmp(mnc, "LXI") ? (reg ? reg : "") : "");
         Opcode *elem = opcode_lookup(lookup);
         if(!elem || !reg || (!strcmp(mnc, "LXI") && !addr)){
                res->ss = 0;
                snprintf(res->err, sizeof(res->err),
                "Invalid register for %s at line %d", mnc, line_num);
                return;
            }
          res->bytes[res->len++] = elem->op;
          size_t val = parse_opd(!strcmp(mnc, "LXI") ? addr : reg, &res->ss, 2);
          if (!res->ss) {
            res->ss = 0;
            snprintf(res->err, sizeof(res->err),
            "Invalid value for %s at line %d", mnc, line_num);
            return;
         }
         res->bytes[res->len++] = (uint8_t)(val & 0xFF);
         res->bytes[res->len++] = (uint8_t)((val >> 8) & 0xFF);
         if(!strcmp(mnc, "JMP")){
            if(res->num_seg >64){
                res->ss = 0;
                snprintf(res->err, sizeof(res->err),
                "Too many JMP relocations at line %d", line_num);
                return;
            }
            res->cur_addr = (uint16_t)val; // JMP target addr
            // final update of old segment
            res->seg[res->num_seg-1].len = res->len - res->seg[res->num_seg-1].offset;
            // new segment after current JMP
            res->seg[res->num_seg] =
            (Segment){.start_addr = res->cur_addr, .offset = res->len, .len=0};
            res->num_seg++;
         }
    }

         else if (!strcmp(mnc, "CMA")  || !strcmp(mnc, "CMC")  || !strcmp(mnc, "STC")  ||
         !strcmp(mnc, "RLC")  || !strcmp(mnc, "RRC")  || !strcmp(mnc, "RAL")  ||
         !strcmp(mnc, "RAR")  || !strcmp(mnc, "DAA")  || !strcmp(mnc, "XCHG") ||
         !strcmp(mnc, "EI")   || !strcmp(mnc, "DI") || !strcmp(mnc, "RIM")  ||
         !strcmp(mnc, "SIM")) {
            Opcode *elem = opcode_lookup(mnc);
            if (!elem) {
                res->ss = 0;
                snprintf(res->err, sizeof(res->err),
                "Invalid value for %s at line %d", mnc, line_num);
                return;
            }
            res->bytes[res->len++] = elem->op;
        }
        else if (!strcmp(mnc, "RST")) {
            char *rst_num = strtok_r(NULL, " \t,", &tok_ptr);
            char lookup[8];
            snprintf(lookup, sizeof(lookup), "%s %s", "RST", rst_num? rst_num : "");
            Opcode *elem = opcode_lookup(lookup);
            if (!rst_num || !elem) {
                res->ss = 0;
                snprintf(res->err, sizeof(res->err),
                "Invalid RST vector at line %d", line_num);
                return;
            }
            res->bytes[res->len++] = elem->op;
        }
         else {
            res->ss = 0;
            snprintf(res->err, sizeof(res->err),
            "Unknown instruction '%s' at line %d", mnc, line_num);
            return;
        }
        line = strtok_r(NULL, "\n", &line_ptr);
    }
}

