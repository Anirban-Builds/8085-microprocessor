#ifndef _CPU_8085_
#define _CPU_8085_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>


#include "assembler_8085.h"

#define MEM_SIZE    (64*1024)
#define START_ADDR  0x0040

#define FLAG_CY  0x01
#define FLAG_P   0x04
#define FLAG_AC  0x10
#define FLAG_Z   0x40
#define FLAG_S   0x80

typedef struct {
    uint8_t  A, B, C, D, E, H, L, F;
    uint16_t PC, SP;
    uint8_t  mem[MEM_SIZE];
    bool     halted;
    bool     err;
    char     err_msg[64];
} CPU;

extern Hashmap *sessions_mp;

void cpu_init(CPU *cpu);
void cpu_load(CPU *cpu, uint8_t *bytes, int num_seg, Segment *seg, uint16_t addr);
void cpu_run(CPU *cpu, int max_steps);
CPU *get_cpu(char *id);

#endif