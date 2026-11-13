#ifndef _OPCODES_8085_
#define _OPCODES_8085_

#include "common_8085.h"

#define BASE_MOV 0x40
#define BASE_ADD 0x80
#define BASE_ADC 0x88
#define BASE_SUB 0x90
#define BASE_SBB 0x98
#define BASE_ANA 0xA0
#define BASE_XRA 0xA8
#define BASE_ORA 0xB0
#define BASE_CMP 0xB8
#define BASE_INR 0x04
#define BASE_DCR 0x05

typedef struct {
    const char *name;   
    uint8_t op;
} OpcodeMap;

OpcodeMap *opcode_lookup(const char*);

extern OpcodeMap om[];
extern const int om_size;

#endif