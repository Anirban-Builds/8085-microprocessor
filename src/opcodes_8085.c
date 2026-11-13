#include "opcodes_8085.h"

OpcodeMap om[] = {
    {"ADD", BASE_ADD},
    {"ADC", BASE_ADC},
    {"SUB", BASE_SUB},
    {"SBB", BASE_SBB},
    {"ANA", BASE_ANA},
    {"XRA", BASE_XRA},
    {"ORA", BASE_ORA},
    {"CMP", BASE_CMP},
    {"INR", BASE_INR},
    {"DCR", BASE_DCR},
};

const int om_size = sizeof(om) / sizeof(om[0]);

OpcodeMap *opcode_lookup(const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < om_size; i++) {
        if (strcmp(om[i].name, name) == 0)
            return (OpcodeMap *)&om[i];
    }
    return NULL;
}
