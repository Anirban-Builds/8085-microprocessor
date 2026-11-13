#include "cpu_8085.h"

void cpu_init(CPU* cpu){
    memset(cpu, 0, sizeof(CPU));
    cpu->PC = START_ADDR; // 16 bit addr
    cpu->SP = 0xFFFF; // 16 bit addr
    cpu->halted = false;
    cpu->err = false;
    cpu->err_msg[0] = '\0';
}

void cpu_load(CPU* cpu, uint8_t* bytes, int num_seg, Segment* seg, uint16_t addr){
    for(int s=0; s<num_seg; s++){
    for(size_t i=0; i<seg[s].len; i++) {
        size_t target = seg[s].start_addr+i;
        if(target >= MEM_SIZE){
                // overflow
                cpu->err = true;
                snprintf(cpu->err_msg, sizeof(cpu->err_msg), "memory overflow");
                return;
            }
        cpu->mem[target] = bytes[i+seg[s].offset]; // JMP relocation
        }
    }
    cpu->PC = addr; // PC point to start of program
}

static void cpu_set_flg(CPU* cpu, uint8_t flag, bool val){
    if(val) cpu->F |= flag;
    else cpu->F &= ~flag;
}

static bool cpu_get_flg(CPU* cpu, uint8_t flag){
    return(cpu->F & flag) != 0;
}

static bool even_parity(uint8_t num){
    int cnt = 0;
    for(int i=0; i<8; i++) if(num & (1<<i)) cnt++;
    return (cnt & 1) == 0;
}

static void cpu_upd_zsp_flg(CPU *cpu, uint8_t res) {
    cpu_set_flg(cpu, FLAG_Z, res == 0);
    cpu_set_flg(cpu, FLAG_S, (res & FLAG_S) != 0);
    cpu_set_flg(cpu, FLAG_P, even_parity(res));
}

static uint8_t* reg_ptr(CPU* cpu, uint8_t code){
    switch(code){
        case 0: return &cpu->B;
        case 1: return &cpu->C;
        case 2: return &cpu->D;
        case 3: return &cpu->E;
        case 4: return &cpu->H;
        case 5: return &cpu->L;
        case 6: return &cpu-> mem[(cpu->H << 8) | cpu->L];
        case 7 : return &cpu->A;
        default : return NULL;
    }
}

static uint16_t get_reg_pr(uint8_t r1, uint8_t r2) {return (r1<<8) | r2;}

static void set_reg_pr(uint8_t *r1, uint8_t *r2, uint16_t v){
    *r1 = (v>>8)&0xFF; *r2 = v&0xFF;}

static void cpu_inr(CPU* cpu, uint8_t *r){
    int ac = ((*r&0x0F)+1)>0x0F; (*r)++;
    cpu_set_flg(cpu, FLAG_AC, ac);
    cpu_upd_zsp_flg(cpu, *r);
}

static void cpu_dcr(CPU* cpu, uint8_t *r){
    (*r)--;
    cpu_set_flg(cpu, FLAG_AC, (*r&0x0F)!=0x0F);
    cpu_upd_zsp_flg(cpu, *r);
}

static void alu_add(CPU* cpu, uint8_t val, int cin){
    uint16_t res = (uint16_t)(cpu->A+val+cin);
    int ac = ((cpu->A&0x0F)+(val&0x0F)+cin)>0x0F;
    cpu_set_flg(cpu,FLAG_CY,res>0xFF);cpu_set_flg(cpu,FLAG_AC,ac);
    cpu->A=(uint8_t)res; cpu_upd_zsp_flg(cpu, cpu->A);
}

static void alu_sub(CPU* cpu, uint8_t val, int cin, bool s){
    int full = val+cin; uint8_t res = (uint8_t)(cpu->A-full);
    int ac =((cpu->A&0x0F) - (val&0x0F) - cin) < 0;
    cpu_set_flg(cpu, FLAG_CY, cpu->A<full);cpu_set_flg(cpu,FLAG_AC,ac);
    cpu_upd_zsp_flg(cpu, res);if(s) cpu->A=res;
}

static void alu_bitwise(CPU* cpu, uint8_t val, int c){
    uint8_t ac_src=(cpu->A|val)&0x08;
    switch(c){
    case 1:{cpu->A&=val;break;}
    case 2:{cpu->A^=val;break;}
    case 3:{cpu->A|=val;break;}
    }
    cpu_set_flg(cpu, FLAG_CY, false);
    cpu_set_flg(cpu, FLAG_AC, c == 1 ? ac_src!=0 :false);
    cpu_upd_zsp_flg(cpu, cpu->A);
}

static void cpu_step(CPU* cpu){
    if(cpu->halted) return;
    uint8_t opcode = cpu-> mem[cpu->PC++]; // incr PC
    switch (opcode){
        case 0x00: break;
        case 0x76: cpu->halted = true; break;
        case 0x3E: cpu->A = cpu-> mem[cpu->PC++]; break;
        case 0x06: cpu->B = cpu-> mem[cpu->PC++]; break;
        case 0x0E: cpu->C = cpu-> mem[cpu->PC++]; break;
        case 0x16: cpu->D = cpu-> mem[cpu->PC++]; break;
        case 0x1E: cpu->E = cpu-> mem[cpu->PC++]; break;
        case 0x26: cpu->H = cpu-> mem[cpu->PC++]; break;
        case 0x2E: cpu->L = cpu-> mem[cpu->PC++]; break;
        case 0x36 : cpu->mem[get_reg_pr(cpu->H, cpu->L)] = cpu->mem[cpu->PC++]; break;
        // sp grows downwards
        case 0xC5:
        if(cpu->SP-2>cpu->PC){cpu->mem[--cpu->SP]=cpu->B;cpu->mem[--cpu->SP]=cpu->C; break;}
        cpu->err = true; break;
        case 0xD5:
        if(cpu->SP-2>cpu->PC){cpu->mem[--cpu->SP]=cpu->D;cpu->mem[--cpu->SP]=cpu->E; break;}
        cpu->err = true; break;
        case 0xE5:
        if(cpu->SP-2>cpu->PC){cpu->mem[--cpu->SP]=cpu->H;cpu->mem[--cpu->SP]=cpu->L; break;}
        cpu->err = true; break;
        case 0xF5: // PSW
        if(cpu->SP-2>cpu->PC){cpu->mem[--cpu->SP]=cpu->A;cpu->mem[--cpu->SP]=cpu->F; break;}
         cpu->err = true; break;
        // sp shrinks towards 0xFFFFH
        case 0xC1: cpu->C = cpu->mem[cpu->SP++]; cpu->B = cpu-> mem[cpu->SP++]; break;
        case 0xD1: cpu->E = cpu->mem[cpu->SP++]; cpu->D = cpu-> mem[cpu->SP++]; break;
        case 0xE1: cpu->L = cpu->mem[cpu->SP++]; cpu->H = cpu-> mem[cpu->SP++]; break;
        case 0xF1: cpu->F = cpu->mem[cpu->SP++]; cpu->A = cpu-> mem[cpu->SP++]; break;
        // jump instrs
        case 0xC3: {uint16_t addr = cpu->mem[cpu->PC] | (cpu->mem[cpu->PC+1]<<8);
        cpu->PC = addr; break;}
        case 0xCA: {uint16_t addr = cpu->mem[cpu->PC] | (cpu->mem[cpu->PC+1]<<8);
        cpu->PC+=2; if(cpu_get_flg(cpu, FLAG_Z))cpu->PC = addr; break;}
        case 0xC2: {uint16_t addr = cpu->mem[cpu->PC] | (cpu->mem[cpu->PC+1]<<8);
        cpu->PC+=2; if(!cpu_get_flg(cpu, FLAG_Z))cpu->PC = addr; break;}
        case 0xDA: {uint16_t addr = cpu->mem[cpu->PC] | (cpu->mem[cpu->PC+1]<<8);
        cpu->PC+=2; if(cpu_get_flg(cpu, FLAG_CY))cpu->PC = addr; break;}
        case 0xD2: {uint16_t addr = cpu->mem[cpu->PC] | (cpu->mem[cpu->PC+1]<<8);
        cpu->PC+=2; if(!cpu_get_flg(cpu, FLAG_CY))cpu->PC = addr; break;}
        case 0xEA: {uint16_t addr = cpu->mem[cpu->PC] | (cpu->mem[cpu->PC+1]<<8);
        cpu->PC+=2; if(cpu_get_flg(cpu, FLAG_P))cpu->PC = addr; break;}
        case 0xE2: {uint16_t addr = cpu->mem[cpu->PC] | (cpu->mem[cpu->PC+1]<<8);
        cpu->PC+=2; if(!cpu_get_flg(cpu, FLAG_P))cpu->PC = addr; break;}
        case 0xFA: {uint16_t addr = cpu->mem[cpu->PC] | (cpu->mem[cpu->PC+1]<<8);
        cpu->PC+=2; if(cpu_get_flg(cpu, FLAG_S))cpu->PC = addr; break;}
        case 0xF2: {uint16_t addr = cpu->mem[cpu->PC] | (cpu->mem[cpu->PC+1]<<8);
        cpu->PC+=2; if(!cpu_get_flg(cpu, FLAG_S))cpu->PC = addr; break;}
        // subroutine call instrs
        case 0xCD:{uint16_t addr = cpu->mem[cpu->PC] | (cpu->mem[cpu->PC+1]<<8); cpu->PC+=2;
        cpu->mem[--cpu->SP] = (uint8_t)(cpu->PC>>8); cpu->mem[--cpu->SP] = (uint8_t)cpu->PC;
        cpu->PC = addr;break;}
        case 0xCC:{uint16_t addr = cpu->mem[cpu->PC] | (cpu->mem[cpu->PC+1]<<8); cpu->PC+=2;
        if(cpu_get_flg(cpu, FLAG_Z)){cpu->mem[--cpu->SP] = (uint8_t)(cpu->PC>>8);
        cpu->mem[--cpu->SP] = (uint8_t)cpu->PC;cpu->PC = addr;}break;}
        case 0xC4:{uint16_t addr = cpu->mem[cpu->PC] | (cpu->mem[cpu->PC+1]<<8); cpu->PC+=2;
        if(!cpu_get_flg(cpu, FLAG_Z)){cpu->mem[--cpu->SP] = (uint8_t)(cpu->PC>>8);
        cpu->mem[--cpu->SP] = (uint8_t)cpu->PC;cpu->PC = addr;}break;}
        case 0xDC:{uint16_t addr = cpu->mem[cpu->PC] | (cpu->mem[cpu->PC+1]<<8); cpu->PC+=2;
        if(cpu_get_flg(cpu, FLAG_CY)){cpu->mem[--cpu->SP] = (uint8_t)(cpu->PC>>8);
        cpu->mem[--cpu->SP] = (uint8_t)cpu->PC;cpu->PC = addr;}break;}
        case 0xD4:{uint16_t addr = cpu->mem[cpu->PC] | (cpu->mem[cpu->PC+1]<<8); cpu->PC+=2;
        if(!cpu_get_flg(cpu, FLAG_CY)){cpu->mem[--cpu->SP] = (uint8_t)(cpu->PC>>8);
        cpu->mem[--cpu->SP] = (uint8_t)cpu->PC;cpu->PC = addr;}break;}
        case 0xEC:{uint16_t addr = cpu->mem[cpu->PC] | (cpu->mem[cpu->PC+1]<<8); cpu->PC+=2;
        if(cpu_get_flg(cpu, FLAG_P)){cpu->mem[--cpu->SP] = (uint8_t)(cpu->PC>>8);
        cpu->mem[--cpu->SP] = (uint8_t)cpu->PC;cpu->PC = addr;}break;}
        case 0xE4:{uint16_t addr = cpu->mem[cpu->PC] | (cpu->mem[cpu->PC+1]<<8); cpu->PC+=2;
        if(!cpu_get_flg(cpu, FLAG_P)){cpu->mem[--cpu->SP] = (uint8_t)(cpu->PC>>8);
        cpu->mem[--cpu->SP] = (uint8_t)cpu->PC;cpu->PC = addr;}break;}
        case 0xFC:{uint16_t addr = cpu->mem[cpu->PC] | (cpu->mem[cpu->PC+1]<<8); cpu->PC+=2;
        if(cpu_get_flg(cpu, FLAG_S)){cpu->mem[--cpu->SP] = (uint8_t)(cpu->PC>>8);
        cpu->mem[--cpu->SP] = (uint8_t)cpu->PC;cpu->PC = addr;}break;}
        case 0xF4:{uint16_t addr = cpu->mem[cpu->PC] | (cpu->mem[cpu->PC+1]<<8); cpu->PC+=2;
        if(!cpu_get_flg(cpu, FLAG_S)){cpu->mem[--cpu->SP] = (uint8_t)(cpu->PC>>8);
        cpu->mem[--cpu->SP] = (uint8_t)cpu->PC;cpu->PC = addr;}break;}
        // subroutine ret instrs
        case 0xC9:{uint16_t low=cpu->mem[cpu->SP++];uint16_t high=cpu->mem[cpu->SP++];
        cpu->PC=(high<<8)|low;break;}
        case 0xC8:{if(cpu_get_flg(cpu, FLAG_Z)){uint16_t low=cpu->mem[cpu->SP++];
        uint16_t high=cpu->mem[cpu->SP++];cpu->PC=(high<<8)|low;}break;}
        case 0xC0:{if(!cpu_get_flg(cpu, FLAG_Z)){uint16_t low=cpu->mem[cpu->SP++];
        uint16_t high=cpu->mem[cpu->SP++];cpu->PC=(high<<8)|low;}break;}
        case 0xD8:{if(cpu_get_flg(cpu, FLAG_CY)){uint16_t low=cpu->mem[cpu->SP++];
        uint16_t high=cpu->mem[cpu->SP++];cpu->PC=(high<<8)|low;}break;}
        case 0xD0:{if(!cpu_get_flg(cpu, FLAG_CY)){uint16_t low=cpu->mem[cpu->SP++];
        uint16_t high=cpu->mem[cpu->SP++];cpu->PC=(high<<8)|low;}break;}
        case 0xE8:{if(cpu_get_flg(cpu, FLAG_P)){uint16_t low=cpu->mem[cpu->SP++];
        uint16_t high=cpu->mem[cpu->SP++];cpu->PC=(high<<8)|low;}break;}
        case 0xE0:{if(!cpu_get_flg(cpu, FLAG_P)){uint16_t low=cpu->mem[cpu->SP++];
        uint16_t high=cpu->mem[cpu->SP++];cpu->PC=(high<<8)|low;}break;}
        case 0xF8:{if(cpu_get_flg(cpu, FLAG_S)){uint16_t low=cpu->mem[cpu->SP++];
        uint16_t high=cpu->mem[cpu->SP++];cpu->PC=(high<<8)|low;}break;}
        case 0xF0:{if(!cpu_get_flg(cpu, FLAG_S)){uint16_t low=cpu->mem[cpu->SP++];
        uint16_t high=cpu->mem[cpu->SP++];cpu->PC=(high<<8)|low;}break;}
        // LXI
        case 0x01:{uint16_t v=cpu->mem[cpu->PC]|(cpu->mem[cpu->PC+1]<<8);
        cpu->PC+=2; set_reg_pr(&cpu->B, &cpu->C, v); break;}
        case 0x11:{uint16_t v=cpu->mem[cpu->PC]|(cpu->mem[cpu->PC+1]<<8);
        cpu->PC+=2; set_reg_pr(&cpu->D, &cpu->E, v); break;}
        case 0x21:{uint16_t v=cpu->mem[cpu->PC]|(cpu->mem[cpu->PC+1]<<8);
        cpu->PC+=2; set_reg_pr(&cpu->H, &cpu->L, v); break;}
        case 0x31:{uint16_t v=cpu->mem[cpu->PC]|(cpu->mem[cpu->PC+1]<<8);
        cpu->PC+=2; cpu->SP=v; break;}
        // STAX/LDAX
        case 0x02: cpu->mem[get_reg_pr(cpu->B, cpu->C)]=cpu->A; break;
        case 0x12: cpu->mem[get_reg_pr(cpu->D, cpu->E)]=cpu->A; break;
        case 0x0A: cpu->A = cpu->mem[get_reg_pr(cpu->B, cpu->C)]; break;
        case 0x1A: cpu->A = cpu->mem[get_reg_pr(cpu->D, cpu->E)]; break;
        // INX/DCX
        case 0x03: set_reg_pr(&cpu->B, &cpu->C, get_reg_pr(cpu->B, cpu->C)+1); break;
        case 0x13: set_reg_pr(&cpu->D, &cpu->E, get_reg_pr(cpu->D, cpu->E)+1); break;
        case 0x23: set_reg_pr(&cpu->H, &cpu->L, get_reg_pr(cpu->H, cpu->L)+1); break;
        case 0x33: cpu->SP++; break;
        case 0x0B: set_reg_pr(&cpu->B, &cpu->C, get_reg_pr(cpu->B, cpu->C)-1); break;
        case 0x1B: set_reg_pr(&cpu->D, &cpu->E, get_reg_pr(cpu->D, cpu->E)-1); break;
        case 0x2B: set_reg_pr(&cpu->H, &cpu->L, get_reg_pr(cpu->H, cpu->L)-1); break;
        case 0x3B: cpu->SP--; break;
        //DAD
        case 0x09: {uint32_t r=(uint32_t)get_reg_pr(cpu->H,cpu->L)+get_reg_pr(cpu->B,cpu->C);
            cpu_set_flg(cpu,FLAG_CY,r>0xFFFF);set_reg_pr(&cpu->H,&cpu->L,(uint16_t)r);break;}
        case 0x19: {uint32_t r=(uint32_t)get_reg_pr(cpu->H,cpu->L)+get_reg_pr(cpu->D,cpu->E);
            cpu_set_flg(cpu,FLAG_CY,r>0xFFFF);set_reg_pr(&cpu->H,&cpu->L,(uint16_t)r);break;}
        case 0x29: {uint32_t r=(uint32_t)get_reg_pr(cpu->H,cpu->L)*2;
            cpu_set_flg(cpu,FLAG_CY,r>0xFFFF);set_reg_pr(&cpu->H,&cpu->L,(uint16_t)r);break;}
        case 0x39: {uint32_t r=(uint32_t)get_reg_pr(cpu->H,cpu->L)+cpu->SP;
            cpu_set_flg(cpu,FLAG_CY,r>0xFFFF);set_reg_pr(&cpu->H,&cpu->L,(uint16_t)r);break;}
        // INR/DCR
        case 0x04:cpu_inr(cpu, &cpu->B); break;
        case 0x0C:cpu_inr(cpu, &cpu->C); break;
        case 0x14:cpu_inr(cpu, &cpu->D); break;
        case 0x1C:cpu_inr(cpu, &cpu->E); break;
        case 0x24:cpu_inr(cpu, &cpu->H); break;
        case 0x2C:cpu_inr(cpu, &cpu->L); break;
        case 0x34:cpu_inr(cpu, &cpu->mem[get_reg_pr(cpu->H, cpu->L)]); break;
        case 0x3C:cpu_inr(cpu, &cpu->A); break;
        case 0x05:cpu_dcr(cpu, &cpu->B); break;
        case 0x0D:cpu_dcr(cpu, &cpu->C); break;
        case 0x15:cpu_dcr(cpu, &cpu->D); break;
        case 0x1D:cpu_dcr(cpu, &cpu->E); break;
        case 0x25:cpu_dcr(cpu, &cpu->H); break;
        case 0x2D:cpu_dcr(cpu, &cpu->L); break;
        case 0x35:cpu_dcr(cpu, &cpu->mem[get_reg_pr(cpu->H, cpu->L)]); break;
        case 0x3D:cpu_dcr(cpu, &cpu->A); break;
        // rotations flag ops DAA
        case 0x07:{uint8_t b=(cpu->A&0x80)?1:0;cpu->A=(cpu->A<<1)|b;
            cpu_set_flg(cpu,FLAG_CY,b);break;}
        case 0x0F:{uint8_t b=(cpu->A&0x01)?1:0;cpu->A=(cpu->A>>1)|(b<<7);
            cpu_set_flg(cpu,FLAG_CY,b);break;}
        case 0x17: {uint8_t c=cpu_get_flg(cpu,FLAG_CY)?1:0;uint8_t b=(cpu->A&FLAG_S)?1:0;
        cpu->A=(cpu->A<<1)|c;cpu_set_flg(cpu,FLAG_CY,b);break;}
        case 0x1F: {uint8_t c=cpu_get_flg(cpu,FLAG_CY)?1:0;uint8_t b=cpu->A&FLAG_CY;
        cpu->A=(cpu->A<<1)|(c<<7);cpu_set_flg(cpu,FLAG_CY,b);break;}
        case 0x2F:cpu->A = ~cpu->A; break;
        case 0x37:cpu_set_flg(cpu,FLAG_CY,true);break;
        case 0x3F:cpu_set_flg(cpu,FLAG_CY,!cpu_get_flg(cpu,FLAG_CY));break;
        case 0x27:{
            uint8_t a=0;bool c=cpu_get_flg(cpu, FLAG_CY);
            if((cpu->A&0x0F)>9||cpu_get_flg(cpu,FLAG_AC)) a|=0x06;
            if((cpu->A>>4)>9||c||((cpu->A>>4)==9&&(cpu->A&0x0F)>9)){a|=0x60;c=true;}
            cpu_set_flg(cpu,FLAG_AC,((cpu->A&0x0F)+(a&0x0F))>0x0F);
            cpu->A=(uint8_t)(cpu->A+a);cpu_set_flg(cpu,FLAG_CY,c);
            cpu_upd_zsp_flg(cpu,cpu->A);break;}
        case 0x20: break; case 0x30: break;  // RIM/SIM
        // ALU immediate
        case 0xC6: alu_add(cpu, cpu->mem[cpu->PC++], 0); break;
        case 0xCE: alu_add(cpu, cpu->mem[cpu->PC++], cpu_get_flg(cpu, FLAG_CY)?1:0); break;
        case 0xD6: alu_sub(cpu, cpu->mem[cpu->PC++], 0, true); break;
        case 0xDE: alu_sub(cpu, cpu->mem[cpu->PC++], cpu_get_flg(cpu,FLAG_CY)?1:0,true);break;
        case 0xE6: alu_bitwise(cpu, cpu->mem[cpu->PC++], 1); break;
        case 0xEE: alu_bitwise(cpu, cpu->mem[cpu->PC++], 2); break;
        case 0xF6: alu_bitwise(cpu, cpu->mem[cpu->PC++], 3); break;
        case 0xFE: alu_sub(cpu, cpu->mem[cpu->PC++], 0, false); break;
        // direct load/store
        case 0x32:{uint16_t a=cpu->mem[cpu->PC]|(cpu->mem[cpu->PC+1]<<8);cpu->PC+=2;cpu->mem[a]=cpu->A; break;}
        case 0x3A:{uint16_t a=cpu->mem[cpu->PC]|(cpu->mem[cpu->PC+1]<<8);cpu->PC+=2;cpu->A=cpu->mem[a];break;}
        case 0x22:{uint16_t a=cpu->mem[cpu->PC]|(cpu->mem[cpu->PC+1]<<8);cpu->PC+=2;cpu->mem[a]=cpu->L;cpu->mem[a+1]=cpu->H;break;}
        case 0x2A:{uint16_t a=cpu->mem[cpu->PC]|(cpu->mem[cpu->PC+1]<<8);cpu->PC+=2;cpu->L=cpu->mem[a];cpu->H=cpu->mem[a+1];break;}
        // exchange and stack/pc HL
        case 0xEB:{uint16_t t=cpu->H;cpu->H=cpu->D;cpu->D=t;t=cpu->L;cpu->L=cpu->E;cpu->E=t;break;}
        case 0xE3:{uint16_t tl=cpu->mem[cpu->SP],th=cpu->mem[cpu->SP+1];cpu->mem[cpu->SP]=cpu->L;
        cpu->mem[cpu->SP+1]=cpu->H;cpu->L=tl;cpu->H=th;break;}
        case 0xE9:cpu->PC=get_reg_pr(cpu->H, cpu->L);break;
        case 0xF9:cpu->SP=get_reg_pr(cpu->H, cpu->L);break;
        // intr EI/DI
        case 0xFB: break; case 0xF3: break;
        // RST
        case 0xC7: case 0xCF: case 0xD7: case 0xDF: case 0xE7: case 0xEF: case 0xF7: case 0xFF:{
            uint8_t n = (opcode - 0xC7) >> 3;
            cpu->mem[--cpu->SP]=(uint8_t)(cpu->PC>>8); cpu->mem[--cpu->SP]=(uint8_t)cpu->PC;
            cpu->PC=n<<3; break;
        }
        // I/O
        case 0xDB: case 0xD3: cpu->PC++ ; break;
        default:{
        if(opcode>=BASE_MOV && opcode <= 0x7F){
            uint8_t dest_code = (opcode >> 3) & 0x07;
            uint8_t src_code = opcode & 0x07;
            uint8_t* dest = reg_ptr(cpu, dest_code);
            uint8_t* src = reg_ptr(cpu, src_code);
            if(!dest || !src){
                cpu->err = true;
                snprintf(cpu->err_msg, sizeof(cpu->err_msg), " Invalid register");
                return;
            }
            *dest = *src; // MOV
        }
        else if(opcode >= BASE_ADD && opcode <= 0x87){
            uint8_t src_code = opcode & 0x07;
            uint8_t* src = reg_ptr(cpu, src_code);
            if(!src){
                cpu->err = true;
                snprintf(cpu->err_msg, sizeof(cpu->err_msg), " Invalid register");
                return;}
                alu_add(cpu, *src, 0);
            }
        else if(opcode>=0x88 && opcode<=0x8F){
            uint8_t* s=reg_ptr(cpu, opcode&7);
            if(!s){cpu->err =true;
            snprintf(cpu->err_msg, sizeof(cpu->err_msg), "Invalid register");
            return;
        }
            alu_add(cpu,*s,cpu_get_flg(cpu,FLAG_CY)?1:0);
        }
        else if(opcode>=0x90 && opcode<=0x97){
            uint8_t* s=reg_ptr(cpu, opcode&7);
            if(!s){cpu->err =true;
            snprintf(cpu->err_msg, sizeof(cpu->err_msg), "Invalid register");
            return;
        }
            alu_sub(cpu, *s, 0, true);
        }
        else if(opcode>=0x98 && opcode<=0x9F){
            uint8_t* s=reg_ptr(cpu, opcode&7);
            if(!s){cpu->err =true;
            snprintf(cpu->err_msg, sizeof(cpu->err_msg), "Invalid register");
            return;
        }
            alu_sub(cpu, *s, cpu_get_flg(cpu, FLAG_CY)?1:0, true);
        }
        else if(opcode>=0xA0 && opcode<=0xA7){
            uint8_t* s=reg_ptr(cpu, opcode&7);
            if(!s){cpu->err =true;
            snprintf(cpu->err_msg, sizeof(cpu->err_msg), "Invalid register");
            return;
        }
            alu_bitwise(cpu, *s, 1);
        }
        else if(opcode>=0xA8 && opcode<=0xAF){
            uint8_t* s=reg_ptr(cpu, opcode&7);
            if(!s){cpu->err =true;
            snprintf(cpu->err_msg, sizeof(cpu->err_msg), "Invalid register");
            return;
        }
            alu_bitwise(cpu, *s, 2);
        }
        else if(opcode>=0xB0 && opcode<=0xB7){
            uint8_t* s=reg_ptr(cpu, opcode&7);
            if(!s){cpu->err =true;
            snprintf(cpu->err_msg, sizeof(cpu->err_msg), "Invalid register");
            return;
        }
            alu_bitwise(cpu, *s, 3);
        }
        else if(opcode>=0xB8 && opcode<=0xBF){
            uint8_t* s=reg_ptr(cpu, opcode&7);
            if(!s){cpu->err =true;
            snprintf(cpu->err_msg, sizeof(cpu->err_msg), "Invalid register");
            return;
        }
            alu_sub(cpu, *s, 0, false);
        }
        else{
                cpu->err = true;
                snprintf(cpu->err_msg, sizeof(cpu->err_msg), " Invalid opcode");
            }
        }
    }
}

void cpu_run(CPU* cpu, int max_steps){
    // max_steps to prevent infinite loop
    int steps=0;
    while(!cpu->halted && !cpu->err && steps < max_steps){
        cpu_step(cpu);
        steps++;
    }
}

CPU* get_cpu(char* id){
    extern Hashmap* sessions_mp;
    extern mutex_t session_lock;
    MUTEX_LOCK(session_lock);
    CPU* cpu = (CPU*)map_get(sessions_mp, id);
    if(!cpu){
        cpu = malloc(sizeof(CPU));
        cpu_init(cpu);
        gen_id(id);
        if(!map_set(sessions_mp, id, cpu)){
            free(cpu);
            return NULL;
        }
    }
    else id[0] = '\0';
    MUTEX_UNLOCK(session_lock);
    return cpu;
}