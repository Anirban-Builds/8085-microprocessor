#ifndef _ASSEMBLER_8085_
#define _ASSEMBLER_8085_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>    // strcasecmp
#include <ctype.h>
#include <stdint.h>
#include <stddef.h>

#include "common_8085.h"
#include "utils_8085.h"    

#define MAX_SEG   64
#define BASE_MOV  0x40     
#define BASE_ADD  0x80    

typedef struct {
    const char *name;
    uint8_t     op;
} Opcode;

typedef struct {
    uint16_t start_addr;   
    size_t   offset;       
    size_t   len;         
} Segment;

typedef struct {
    uint8_t  bytes[BUFFER_SIZE];
    size_t   len;
    int      ss;              
    char     err[128];
    uint16_t cur_addr;
    int      num_seg;
    Segment  seg[MAX_SEG];
} Asmres;

extern Hashmap *opcode_mp;

void assemble(char *s, Asmres *res, Hashmap *mp);

#endif