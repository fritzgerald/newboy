#include "CPU.h"
#import "Device.h"
#include "core/definitions.h"
#include "MMU.h"
#include <stdbool.h>
#include <stdint.h>

#define FLAG_ZERO                 0x80
#define FLAG_SUB                  0x40
#define FLAG_HALF                 0x20
#define FLAG_CARRY                0x10

#define DIV_CLOCK_INC             16

// MARK: Virtual registers

Word GB_register_get_AF(GB_cpu *cpu) {
    return (((Word) cpu->registers.a) << 8) | cpu->registers.f;
}

Word GB_register_get_BC(GB_cpu *cpu) {
    return (((Word) cpu->registers.b) << 8) | cpu->registers.c;
}

Word GB_register_get_DE(GB_cpu *cpu) {
    return (((Word) cpu->registers.d) << 8) | cpu->registers.e;
}

Word GB_register_get_HL(GB_cpu *cpu) {
    return (((Word) cpu->registers.h) << 8) | cpu->registers.l;
}

void GB_register_set_AF(GB_cpu *cpu, Word value) {
    cpu->registers.a = (value & 0xFF00) >> 8;
    cpu->registers.f = (value & 0xF0);
}

void GB_register_set_BC(GB_cpu *cpu, Word value) {
    cpu->registers.b = (value & 0xFF00) >> 8;
    cpu->registers.c = (value & 0xFF);
}

void GB_register_set_DE(GB_cpu *cpu, Word value) {
    cpu->registers.d = (value & 0xFF00) >> 8;
    cpu->registers.e = (value & 0xFF);
}

void GB_register_set_HL(GB_cpu *cpu, Word value) {
    cpu->registers.h = (value & 0xFF00) >> 8;
    cpu->registers.l = (value & 0xFF);
}

// MARK: Memory access
Byte GB_cpu_fetch_byte(GB_device *device, Word delta) {
    return GB_deviceReadByte(device, device->cpu->registers.pc + delta);
}

Word GB_cpu_fetch_word(GB_device *device, Word delta) {
    return GB_deviceReadWord(device, device->cpu->registers.pc + delta);
}

Word GB_cpu_pop_stack(GB_device *device) {
    Word x1 = GB_deviceReadWord(device, device->cpu->registers.sp);
    device->cpu->registers.sp += 2;
    return x1;
}

void GB_cpu_push_stack(GB_device *device, Word data) {
    device->cpu->registers.sp -= 2;
    GB_deviceWriteWord(device, device->cpu->registers.sp, data);
}

Byte GB_cpu_zero_flag(GB_cpu *cpu) {
    return cpu->registers.f & FLAG_ZERO;
}

Byte GB_cpu_get_carry_flag(GB_cpu *cpu) {
    return cpu->registers.f & FLAG_CARRY;
}

Byte GB_cpu_get_carry_flag_bit(GB_cpu *cpu) {
    return (cpu->registers.f & FLAG_CARRY) ? 1 : 0;
}

Byte GB_cpu_get_half_carry_flag_bit(GB_cpu *cpu) {
    return ((cpu->registers.f & FLAG_HALF) >> 5) & 0x01;
}

Byte GB_cpu_get_subtraction_flag_bit(GB_cpu *cpu) {
    return (cpu->registers.f & FLAG_SUB) ? 1 : 0;
}

void GB_cpu_set_carry(GB_cpu *cpu, Byte carryByte) {
    cpu->registers.f = (cpu->registers.f & 0xef) | carryByte << 4;
}

void GB_deviceCpuReset(GB_device* device) {
    GB_cpu *cpu = device->cpu;

    GB_register_set_AF(cpu, 0);
    GB_register_set_BC(cpu, 0);
    GB_register_set_DE(cpu, 0);
    GB_register_set_BC(cpu, 0);
    cpu->registers.sp = 0;
    cpu->registers.pc = 0;
    cpu->is_halted = false;
    cpu->IME = false;
    cpu->divCounter = 0;
    cpu->timaCounter = 0;
    cpu->prevCycles = 0;
}

#define PC_INC(self, val) device->cpu->registers.pc += val
#define ZeroFlagValue(xx)                  (((xx) == 0) ? FLAG_ZERO : 0)
#define HalfCarryFlagValue(x1, x2)         (((x1 & 0x0F) + (x2 & 0x0F) > 0x0F) ? FLAG_HALF : 0)
#define HalfCarryValueC(x1, x2, c)         (((x1 & 0x0F) + ((x2 & 0x0F) + c) > 0x0F) ? FLAG_HALF : 0)
#define HalfCarryFlagValueW(xx1, xx2)      ((((xx1)&0x0FFF) + ((xx2)&0x0FFF) > 0x0FFF) ? FLAG_HALF : 0)
#define HalfCarrySubFlagValue(n1, n2)      (((n1 & 0x0F) < (n2 & 0x0F)) ? FLAG_HALF : 0)
#define HalfCarrySubFlagValueC(n1, n2, c)  (((n1 & 0x0F) < ((n2 & 0x0F) + c)) ? FLAG_HALF : 0)
#define CarrySubFlagValueAdd(n1, n2)       ((n1 < n2) ? FLAG_CARRY : 0)
#define CarrySubFlagValueAddC(n1, n2, c)    (((n1 < n2) || (n1 < n2 + c))? FLAG_CARRY : 0)
#define CarryFlagValueAdd(xx, xx1, xx2)    (((xx < xx1) | (xx < xx2)) ? FLAG_CARRY : 0)
#define CarryFlagValueAddC(n, n1, n2, c)   (((((n) - (c)) < (n1)) | (((n) - (c)) < (n2))) << 4)

// MARK: CPU instructions
Byte ins_nop(GB_device* device) { device->cpu->registers.pc++; return 4; }
Byte ins_stop(GB_device* device) { 
    device->cpu->registers.pc+=2; 
    //device->cpu->is_halted = true; 
    return 4; 
}
Byte ins_bad_ins(GB_device* device) { 
    device->cpu->is_halted = true; 
    return 20; 
}
Byte ins_di(GB_device* device) { device->cpu->disableINT = 2; device->cpu->registers.pc++; return 4; }
Byte ins_ei(GB_device* device) { device->cpu->enableINT = 2; device->cpu->registers.pc++; return 4; }

Byte ins_scf(GB_device* device) { device->cpu->registers.f = GB_cpu_zero_flag(device->cpu) | FLAG_CARRY; device->cpu->registers.pc++; return 4; }
Byte ins_ccf(GB_device* device) { device->cpu->registers.f = GB_cpu_zero_flag(device->cpu) | ((GB_cpu_get_carry_flag(device->cpu) == 0) ? FLAG_CARRY : 0); device->cpu->registers.pc++; return 4; }

Byte ins_ld_bc_xx(GB_device* device) { GB_register_set_BC(device->cpu, GB_cpu_fetch_word(device, 1)); PC_INC(self, 3);  return 12; }
Byte ins_ld_de_xx(GB_device* device) { GB_register_set_DE(device->cpu, GB_cpu_fetch_word(device, 1)); PC_INC(self, 3);  return 12; }
Byte ins_ld_hl_xx(GB_device* device) { GB_register_set_HL(device->cpu, GB_cpu_fetch_word(device, 1)); PC_INC(self, 3);  return 12; }
Byte ins_ld_sp_xx(GB_device* device) { device->cpu->registers.sp = GB_cpu_fetch_word(device, 1); PC_INC(self, 3);  return 12; }
Byte ins_ld_sp_hl(GB_device* device) { device->cpu->registers.sp = GB_register_get_HL(device->cpu); PC_INC(self, 1); return 8; }

Byte ins_ld_hl_spx(GB_device* device) {
    int16_t offset = (int8_t) GB_cpu_fetch_byte(device, 1);
    GB_register_set_HL(device->cpu, device->cpu->registers.sp + offset);
    device->cpu->registers.f = 0;

    if ((device->cpu->registers.sp & 0xF) + (offset & 0xF) > 0xF) {
        device->cpu->registers.f |= FLAG_HALF;
    }

    if ((device->cpu->registers.sp & 0xFF)  + (offset & 0xFF) > 0xFF) {
        device->cpu->registers.f |= FLAG_CARRY;
    }
    PC_INC(self, 2);  
    return 12; 
}

Byte ins_ld_bc_a(GB_device* device) { GB_deviceWriteByte(device,GB_register_get_BC(device->cpu), device->cpu->registers.a); PC_INC(self, 1);  return 8; }
Byte ins_ld_de_a(GB_device* device) { GB_deviceWriteByte(device,GB_register_get_DE(device->cpu), device->cpu->registers.a); PC_INC(self, 1);  return 8; }
Byte ins_ld_hl_a(GB_device* device) { GB_deviceWriteByte(device,GB_register_get_HL(device->cpu), device->cpu->registers.a); PC_INC(self, 1);  return 8; }
Byte ins_ld_hl_b(GB_device* device) { GB_deviceWriteByte(device,GB_register_get_HL(device->cpu), device->cpu->registers.b); PC_INC(self, 1);  return 8; }
Byte ins_ld_hl_c(GB_device* device) { GB_deviceWriteByte(device,GB_register_get_HL(device->cpu), device->cpu->registers.c); PC_INC(self, 1);  return 8; }
Byte ins_ld_hl_d(GB_device* device) { GB_deviceWriteByte(device,GB_register_get_HL(device->cpu), device->cpu->registers.d); PC_INC(self, 1);  return 8; }
Byte ins_ld_hl_e(GB_device* device) { GB_deviceWriteByte(device,GB_register_get_HL(device->cpu), device->cpu->registers.e); PC_INC(self, 1);  return 8; }
Byte ins_ld_hl_h(GB_device* device) { GB_deviceWriteByte(device,GB_register_get_HL(device->cpu), device->cpu->registers.h); PC_INC(self, 1);  return 8; }
Byte ins_ld_hl_l(GB_device* device) { GB_deviceWriteByte(device,GB_register_get_HL(device->cpu), device->cpu->registers.l); PC_INC(self, 1);  return 8; }
Byte ins_ld_hl_x(GB_device* device) {
    GB_deviceWriteByte(device,GB_register_get_HL(device->cpu), GB_cpu_fetch_byte(device, 1));
    PC_INC(self, 2);  
    return 12; 
}

Byte ins_ld_inc_hl_a(GB_device* device) { Word hl = GB_register_get_HL(device->cpu); GB_deviceWriteByte(device,hl++, device->cpu->registers.a); GB_register_set_HL(device->cpu, hl); PC_INC(self, 1);  return 8; }

Byte ins_ld_dec_hl_a(GB_device* device) { Word hl = GB_register_get_HL(device->cpu); GB_deviceWriteByte(device,hl--, device->cpu->registers.a); GB_register_set_HL(device->cpu, hl); PC_INC(self, 1);  return 8; }
Byte ins_ld_a_hl_inc(GB_device* device) { Word hl = GB_register_get_HL(device->cpu); device->cpu->registers.a = GB_deviceReadByte(device, hl++); GB_register_set_HL(device->cpu, hl); PC_INC(self, 1);  return 8; }
Byte ins_ld_a_hl_dec(GB_device* device) { Word hl = GB_register_get_HL(device->cpu); device->cpu->registers.a = GB_deviceReadByte(device, hl--); GB_register_set_HL(device->cpu, hl); PC_INC(self, 1);  return 8; }

Byte ins_ld_a_x(GB_device* device) { device->cpu->registers.a = GB_cpu_fetch_byte(device, 1); PC_INC(self, 2); return 8; }
Byte ins_ld_b_x(GB_device* device) { device->cpu->registers.b = GB_cpu_fetch_byte(device, 1); PC_INC(self, 2); return 8; }
Byte ins_ld_c_x(GB_device* device) { device->cpu->registers.c = GB_cpu_fetch_byte(device, 1); PC_INC(self, 2); return 8; }
Byte ins_ld_d_x(GB_device* device) { device->cpu->registers.d = GB_cpu_fetch_byte(device, 1); PC_INC(self, 2); return 8; }
Byte ins_ld_e_x(GB_device* device) { device->cpu->registers.e = GB_cpu_fetch_byte(device, 1); PC_INC(self, 2); return 8; }
Byte ins_ld_h_x(GB_device* device) { device->cpu->registers.h = GB_cpu_fetch_byte(device, 1); PC_INC(self, 2); return 8; }
Byte ins_ld_l_x(GB_device* device) { device->cpu->registers.l = GB_cpu_fetch_byte(device, 1); PC_INC(self, 2); return 8; }
Byte ins_ld_a_xx(GB_device* device) { Word xx = GB_cpu_fetch_word(device, 1); device->cpu->registers.a = GB_deviceReadByte(device, xx); PC_INC(self, 3); return 16; }

Byte ins_ld_a_b(GB_device* device) { device->cpu->registers.a = device->cpu->registers.b; PC_INC(self, 1); return 4; }
Byte ins_ld_a_c(GB_device* device) { device->cpu->registers.a = device->cpu->registers.c; PC_INC(self, 1); return 4; }
Byte ins_ld_a_d(GB_device* device) { device->cpu->registers.a = device->cpu->registers.d; PC_INC(self, 1); return 4; }
Byte ins_ld_a_e(GB_device* device) { device->cpu->registers.a = device->cpu->registers.e; PC_INC(self, 1); return 4; }
Byte ins_ld_a_h(GB_device* device) { device->cpu->registers.a = device->cpu->registers.h; PC_INC(self, 1); return 4; }
Byte ins_ld_a_l(GB_device* device) { device->cpu->registers.a = device->cpu->registers.l; PC_INC(self, 1); return 4; }
Byte ins_ld_a_hl(GB_device* device) {device->cpu->registers.a = GB_deviceReadByte(device, GB_register_get_HL(device->cpu)); PC_INC(self, 1);  return 8; }

Byte ins_ld_b_a(GB_device* device) { device->cpu->registers.b = device->cpu->registers.a; PC_INC(self, 1); return 4; }
Byte ins_ld_b_c(GB_device* device) { device->cpu->registers.b = device->cpu->registers.c; PC_INC(self, 1); return 4; }
Byte ins_ld_b_d(GB_device* device) { device->cpu->registers.b = device->cpu->registers.d; PC_INC(self, 1); return 4; }
Byte ins_ld_b_e(GB_device* device) { device->cpu->registers.b = device->cpu->registers.e; PC_INC(self, 1); return 4; }
Byte ins_ld_b_h(GB_device* device) { device->cpu->registers.b = device->cpu->registers.h; PC_INC(self, 1); return 4; }
Byte ins_ld_b_l(GB_device* device) { device->cpu->registers.b = device->cpu->registers.l; PC_INC(self, 1); return 4; }
Byte ins_ld_b_hl(GB_device* device) {device->cpu->registers.b = GB_deviceReadByte(device, GB_register_get_HL(device->cpu)); PC_INC(self, 1);  return 8; }

Byte ins_ld_c_a(GB_device* device) { device->cpu->registers.c = device->cpu->registers.a; PC_INC(self, 1); return 4; }
Byte ins_ld_c_b(GB_device* device) { device->cpu->registers.c = device->cpu->registers.b; PC_INC(self, 1); return 4; }
Byte ins_ld_c_d(GB_device* device) { device->cpu->registers.c = device->cpu->registers.d; PC_INC(self, 1); return 4; }
Byte ins_ld_c_e(GB_device* device) { device->cpu->registers.c = device->cpu->registers.e; PC_INC(self, 1); return 4; }
Byte ins_ld_c_h(GB_device* device) { device->cpu->registers.c = device->cpu->registers.h; PC_INC(self, 1); return 4; }
Byte ins_ld_c_l(GB_device* device) { device->cpu->registers.c = device->cpu->registers.l; PC_INC(self, 1); return 4; }
Byte ins_ld_c_hl(GB_device* device) {device->cpu->registers.c = GB_deviceReadByte(device, GB_register_get_HL(device->cpu)); PC_INC(self, 1);  return 8; }

Byte ins_ld_d_a(GB_device* device) { device->cpu->registers.d = device->cpu->registers.a; PC_INC(self, 1); return 4; }
Byte ins_ld_d_b(GB_device* device) { device->cpu->registers.d = device->cpu->registers.b; PC_INC(self, 1); return 4; }
Byte ins_ld_d_c(GB_device* device) { device->cpu->registers.d = device->cpu->registers.c; PC_INC(self, 1); return 4; }
Byte ins_ld_d_e(GB_device* device) { device->cpu->registers.d = device->cpu->registers.e; PC_INC(self, 1); return 4; }
Byte ins_ld_d_h(GB_device* device) { device->cpu->registers.d = device->cpu->registers.h; PC_INC(self, 1); return 4; }
Byte ins_ld_d_l(GB_device* device) { device->cpu->registers.d = device->cpu->registers.l; PC_INC(self, 1); return 4; }
Byte ins_ld_d_hl(GB_device* device) {device->cpu->registers.d = GB_deviceReadByte(device, GB_register_get_HL(device->cpu)); PC_INC(self, 1);  return 8; }

Byte ins_ld_e_a(GB_device* device)  { device->cpu->registers.e = device->cpu->registers.a; PC_INC(self, 1); return 4; }
Byte ins_ld_e_b(GB_device* device)  { device->cpu->registers.e = device->cpu->registers.b; PC_INC(self, 1); return 4; }
Byte ins_ld_e_c(GB_device* device)  { device->cpu->registers.e = device->cpu->registers.c; PC_INC(self, 1); return 4; }
Byte ins_ld_e_d(GB_device* device)  { device->cpu->registers.e = device->cpu->registers.d; PC_INC(self, 1); return 4; }
Byte ins_ld_e_h(GB_device* device)  { device->cpu->registers.e = device->cpu->registers.h; PC_INC(self, 1); return 4; }
Byte ins_ld_e_l(GB_device* device)  { device->cpu->registers.e = device->cpu->registers.l; PC_INC(self, 1); return 4; }
Byte ins_ld_e_hl(GB_device* device) { device->cpu->registers.e = GB_deviceReadByte(device, GB_register_get_HL(device->cpu)); PC_INC(self, 1);  return 8; }

Byte ins_ld_h_a(GB_device* device) { device->cpu->registers.h = device->cpu->registers.a; PC_INC(self, 1); return 4; }
Byte ins_ld_h_b(GB_device* device) { device->cpu->registers.h = device->cpu->registers.b; PC_INC(self, 1); return 4; }
Byte ins_ld_h_c(GB_device* device) { device->cpu->registers.h = device->cpu->registers.c; PC_INC(self, 1); return 4; }
Byte ins_ld_h_d(GB_device* device) { device->cpu->registers.h = device->cpu->registers.d; PC_INC(self, 1); return 4; }
Byte ins_ld_h_e(GB_device* device) { device->cpu->registers.h = device->cpu->registers.e; PC_INC(self, 1); return 4; }
Byte ins_ld_h_l(GB_device* device) { device->cpu->registers.h = device->cpu->registers.l; PC_INC(self, 1); return 4; }
Byte ins_ld_h_hl(GB_device* device) {device->cpu->registers.h = GB_deviceReadByte(device, GB_register_get_HL(device->cpu)); PC_INC(self, 1);  return 8; }

Byte ins_ld_l_a(GB_device* device) { device->cpu->registers.l = device->cpu->registers.a; PC_INC(self, 1); return 4; }
Byte ins_ld_l_b(GB_device* device) { device->cpu->registers.l = device->cpu->registers.b; PC_INC(self, 1); return 4; }
Byte ins_ld_l_c(GB_device* device) { device->cpu->registers.l = device->cpu->registers.c; PC_INC(self, 1); return 4; }
Byte ins_ld_l_d(GB_device* device) { device->cpu->registers.l = device->cpu->registers.d; PC_INC(self, 1); return 4; }
Byte ins_ld_l_e(GB_device* device) { device->cpu->registers.l = device->cpu->registers.e; PC_INC(self, 1); return 4; }
Byte ins_ld_l_h(GB_device* device) { device->cpu->registers.l = device->cpu->registers.h; PC_INC(self, 1); return 4; }
Byte ins_ld_l_hl(GB_device* device) {device->cpu->registers.l = GB_deviceReadByte(device, GB_register_get_HL(device->cpu)); PC_INC(self, 1);  return 8; }

Byte ins_ld_xx_sp(GB_device* device) { Word xx = GB_cpu_fetch_word(device, 1); GB_deviceWriteWord(device, xx, device->cpu->registers.sp); PC_INC(self, 3); return 20; }
Byte ins_ld_xx_a(GB_device* device) { Word xx = GB_cpu_fetch_word(device, 1); GB_deviceWriteByte(device, xx, device->cpu->registers.a); PC_INC(self, 3); return 16; }

Byte ins_ld_ff00x_a(GB_device* device) { Byte delta = GB_cpu_fetch_byte(device, 1); GB_deviceWriteByte(device,0xff00 + delta, device->cpu->registers.a); PC_INC(self, 2); return 12; }
Byte ins_ld_ff00c_a(GB_device* device) { GB_deviceWriteByte(device, 0xff00 + device->cpu->registers.c, device->cpu->registers.a); PC_INC(self, 1); return 8; }

Byte ins_ld_a_ff00x(GB_device* device) { Byte delta = GB_cpu_fetch_byte(device, 1); device->cpu->registers.a = GB_deviceReadByte(device, 0xff00 + delta); PC_INC(self, 2); return 12; }
Byte ins_ld_a_ff00c(GB_device* device) { device->cpu->registers.a = GB_deviceReadByte(device, 0xff00 + device->cpu->registers.c); PC_INC(self, 1); return 8; }

Byte ins_ld_a_bc(GB_device* device) { device->cpu->registers.a = GB_deviceReadByte(device, GB_register_get_BC(device->cpu)); PC_INC(self, 1); return 8; }
Byte ins_ld_a_de(GB_device* device) { device->cpu->registers.a = GB_deviceReadByte(device, GB_register_get_DE(device->cpu)); PC_INC(self, 1); return 8; }


Byte ins_inc_bc(GB_device* device) { GB_register_set_BC(device->cpu, GB_register_get_BC(device->cpu) + 1); PC_INC(self, 1); return 8; }
Byte ins_inc_de(GB_device* device) { GB_register_set_DE(device->cpu, GB_register_get_DE(device->cpu) + 1); PC_INC(self, 1); return 8; }
Byte ins_inc_hl(GB_device* device) { GB_register_set_HL(device->cpu, GB_register_get_HL(device->cpu) + 1); PC_INC(self, 1); return 8; }
Byte ins_inc_sp(GB_device* device) { device->cpu->registers.sp++; PC_INC(self, 1); return 8; }
Byte ins_inc_hl_ptr(GB_device* device) { Byte value = GB_deviceReadByte(device, GB_register_get_HL(device->cpu)); value++; GB_deviceWriteByte(device,GB_register_get_HL(device->cpu), value); device->cpu->registers.f = ZeroFlagValue(value) | ((value & 0xf) == 0) << 5  | GB_cpu_get_carry_flag(device->cpu); PC_INC(self, 1); return 12; }

Byte ins_inc_a(GB_device* device) { device->cpu->registers.a++; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.a) | ((device->cpu->registers.a & 0xf) == 0) << 5  | GB_cpu_get_carry_flag(device->cpu); PC_INC(self, 1); return 4; }
Byte ins_inc_b(GB_device* device) { device->cpu->registers.b++; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.b) | ((device->cpu->registers.b & 0xf) == 0) << 5  | GB_cpu_get_carry_flag(device->cpu); PC_INC(self, 1); return 4; }
Byte ins_inc_c(GB_device* device) { device->cpu->registers.c++; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.c) | ((device->cpu->registers.c & 0xf) == 0) << 5  | GB_cpu_get_carry_flag(device->cpu); PC_INC(self, 1); return 4; }
Byte ins_inc_d(GB_device* device) { device->cpu->registers.d++; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.d) | ((device->cpu->registers.d & 0xf) == 0) << 5  | GB_cpu_get_carry_flag(device->cpu); PC_INC(self, 1); return 4; }
Byte ins_inc_e(GB_device* device) { device->cpu->registers.e++; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.e) | ((device->cpu->registers.e & 0xf) == 0) << 5  | GB_cpu_get_carry_flag(device->cpu); PC_INC(self, 1); return 4; }
Byte ins_inc_h(GB_device* device) { device->cpu->registers.h++; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.h) | ((device->cpu->registers.h & 0xf) == 0) << 5  | GB_cpu_get_carry_flag(device->cpu); PC_INC(self, 1); return 4; }
Byte ins_inc_l(GB_device* device) { device->cpu->registers.l++; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.l) | ((device->cpu->registers.l & 0xf) == 0) << 5  | GB_cpu_get_carry_flag(device->cpu); PC_INC(self, 1); return 4; }

Byte ins_dec_a(GB_device* device) { device->cpu->registers.a--;  device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.a) | FLAG_SUB | ((device->cpu->registers.a & 0xf) == 0xf) << 5 | GB_cpu_get_carry_flag(device->cpu); PC_INC(self, 1); return 4; }
Byte ins_dec_b(GB_device* device) { device->cpu->registers.b--;  device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.b) | FLAG_SUB | ((device->cpu->registers.b & 0xf) == 0xf) << 5 | GB_cpu_get_carry_flag(device->cpu); PC_INC(self, 1); return 4; }
Byte ins_dec_c(GB_device* device) { device->cpu->registers.c--;  device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.c) | FLAG_SUB | ((device->cpu->registers.c & 0xf) == 0xf) << 5 | GB_cpu_get_carry_flag(device->cpu); PC_INC(self, 1); return 4; }
Byte ins_dec_d(GB_device* device) { device->cpu->registers.d--;  device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.d) | FLAG_SUB | ((device->cpu->registers.d & 0xf) == 0xf) << 5 | GB_cpu_get_carry_flag(device->cpu); PC_INC(self, 1); return 4; }
Byte ins_dec_e(GB_device* device) { device->cpu->registers.e--;  device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.e) | FLAG_SUB | ((device->cpu->registers.e & 0xf) == 0xf) << 5 | GB_cpu_get_carry_flag(device->cpu); PC_INC(self, 1); return 4; }
Byte ins_dec_h(GB_device* device) { device->cpu->registers.h--;  device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.h) | FLAG_SUB | ((device->cpu->registers.h & 0xf) == 0xf) << 5 | GB_cpu_get_carry_flag(device->cpu); PC_INC(self, 1); return 4; }
Byte ins_dec_l(GB_device* device) { device->cpu->registers.l--;  device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.l) | FLAG_SUB | ((device->cpu->registers.l & 0xf) == 0xf) << 5 | GB_cpu_get_carry_flag(device->cpu); PC_INC(self, 1); return 4; }

Byte ins_dec_bc(GB_device* device) { GB_register_set_BC(device->cpu, GB_register_get_BC(device->cpu) - 1); PC_INC(self, 1); return 8; }
Byte ins_dec_de(GB_device* device) { GB_register_set_DE(device->cpu, GB_register_get_DE(device->cpu) - 1); PC_INC(self, 1); return 8; }
Byte ins_dec_hl(GB_device* device) { GB_register_set_HL(device->cpu, GB_register_get_HL(device->cpu) - 1); PC_INC(self, 1); return 8; }
Byte ins_dec_sp(GB_device* device) { device->cpu->registers.sp--; PC_INC(self, 1); return 8; }

Byte ins_dec_hl_ptr(GB_device* device) { Byte value = GB_deviceReadByte(device, GB_register_get_HL(device->cpu)); value--; GB_deviceWriteByte(device,GB_register_get_HL(device->cpu), value); device->cpu->registers.f = ZeroFlagValue(value) | FLAG_SUB | ((value & 0xf) == 0xf) << 5 | GB_cpu_get_carry_flag(device->cpu); PC_INC(self, 1); return 12; }

Byte ins_rlca(GB_device* device) { 
    Byte c = (device->cpu->registers.a >> 7) & 0x01; 
    device->cpu->registers.a = (device->cpu->registers.a << 1) | c; 
    device->cpu->registers.f = c << 4;
    PC_INC(self, 1); 
    return 4; 
}

Byte ins_rlc_a(GB_device* device)  { Byte c = (device->cpu->registers.a >> 7) & 0x01; device->cpu->registers.a = (device->cpu->registers.a << 1) | c; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.a) | (c << 4) ; PC_INC(self, 2); return 8; }
Byte ins_rlc_b(GB_device* device)  { Byte c = (device->cpu->registers.b >> 7) & 0x01; device->cpu->registers.b = (device->cpu->registers.b << 1) | c; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.b) | (c << 4) ; PC_INC(self, 2); return 8; }
Byte ins_rlc_c(GB_device* device)  { Byte c = (device->cpu->registers.c >> 7) & 0x01; device->cpu->registers.c = (device->cpu->registers.c << 1) | c; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.c) | (c << 4) ; PC_INC(self, 2); return 8; }
Byte ins_rlc_d(GB_device* device)  { Byte c = (device->cpu->registers.d >> 7) & 0x01; device->cpu->registers.d = (device->cpu->registers.d << 1) | c; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.d) | (c << 4) ; PC_INC(self, 2); return 8; }
Byte ins_rlc_e(GB_device* device)  { Byte c = (device->cpu->registers.e >> 7) & 0x01; device->cpu->registers.e = (device->cpu->registers.e << 1) | c; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.e) | (c << 4) ; PC_INC(self, 2); return 8; }
Byte ins_rlc_h(GB_device* device)  { Byte c = (device->cpu->registers.h >> 7) & 0x01; device->cpu->registers.h = (device->cpu->registers.h << 1) | c; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.h) | (c << 4) ; PC_INC(self, 2); return 8; }
Byte ins_rlc_l(GB_device* device)  { Byte c = (device->cpu->registers.l >> 7) & 0x01; device->cpu->registers.l = (device->cpu->registers.l << 1) | c; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.l) | (c << 4) ; PC_INC(self, 2); return 8; }
Byte ins_rlc_hl(GB_device* device)  { Word hl = GB_register_get_HL(device->cpu); Byte value = GB_deviceReadByte(device, hl); Byte c = (value >> 7) & 0x01; value = (value << 1) | c; GB_deviceWriteByte(device,hl, value); device->cpu->registers.f = ZeroFlagValue(value) | (c << 4) ; PC_INC(self, 2); return 16; }

Byte ins_rrca(GB_device* device) { 
    Byte c = device->cpu->registers.a & 0x01; 
    device->cpu->registers.a = (device->cpu->registers.a >> 1) | c << 7; 
    device->cpu->registers.f = c << 4;
    PC_INC(self, 1); 
    return 4; 
}

Byte ins_rrc_a(GB_device* device) { Byte c = device->cpu->registers.a & 0x01; device->cpu->registers.a = (device->cpu->registers.a >> 1) | c << 7; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.a) | (c << 4); PC_INC(self, 2); return 8; }
Byte ins_rrc_b(GB_device* device) { Byte c = device->cpu->registers.b & 0x01; device->cpu->registers.b = (device->cpu->registers.b >> 1) | c << 7; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.b) | (c << 4); PC_INC(self, 2); return 8; }
Byte ins_rrc_c(GB_device* device) { Byte c = device->cpu->registers.c & 0x01; device->cpu->registers.c = (device->cpu->registers.c >> 1) | c << 7; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.c) | (c << 4); PC_INC(self, 2); return 8; }
Byte ins_rrc_d(GB_device* device) { Byte c = device->cpu->registers.d & 0x01; device->cpu->registers.d = (device->cpu->registers.d >> 1) | c << 7; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.d) | (c << 4); PC_INC(self, 2); return 8; }
Byte ins_rrc_e(GB_device* device) { Byte c = device->cpu->registers.e & 0x01; device->cpu->registers.e = (device->cpu->registers.e >> 1) | c << 7; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.e) | (c << 4); PC_INC(self, 2); return 8; }
Byte ins_rrc_h(GB_device* device) { Byte c = device->cpu->registers.h & 0x01; device->cpu->registers.h = (device->cpu->registers.h >> 1) | c << 7; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.h) | (c << 4); PC_INC(self, 2); return 8; }
Byte ins_rrc_l(GB_device* device) { Byte c = device->cpu->registers.l & 0x01; device->cpu->registers.l = (device->cpu->registers.l >> 1) | c << 7; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.l) | (c << 4); PC_INC(self, 2); return 8; }
Byte ins_rrc_hl(GB_device* device)  {  
    Word hl = GB_register_get_HL(device->cpu); 
    Byte value = GB_deviceReadByte(device, hl); 
    Byte c = value & 0x01; 
    value = (value >> 1) | c << 7; 
    GB_deviceWriteByte(device,hl, value); 
    device->cpu->registers.f = ZeroFlagValue(value) | (c << 4); 
    PC_INC(self, 2); 
    return 16; 
}


Byte ins_rla (GB_device* device) {
    Byte c = device->cpu->registers.a >> 7;
    Byte oldC = GB_cpu_get_carry_flag_bit(device->cpu); 
    device->cpu->registers.a = (device->cpu->registers.a << 1) | oldC; 
    device->cpu->registers.f = c << 4;
    PC_INC(self, 1); 
    return 4; 
}

Byte ins_rl_a (GB_device* device) { 
    Byte oldC = GB_cpu_get_carry_flag_bit(device->cpu);
    Byte co = (device->cpu->registers.a & 0x80) ? 0x10 : 0;
    device->cpu->registers.a = (device->cpu->registers.a << 1) + oldC; 
    device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.a) + co; 
    PC_INC(self, 2); 
    return 8; 
}
Byte ins_rl_b (GB_device* device) { 
    Byte oldC = GB_cpu_get_carry_flag_bit(device->cpu);
    Byte co = (device->cpu->registers.b & 0x80) ? 0x10 : 0;
    device->cpu->registers.b = (device->cpu->registers.b << 1) + oldC; 
    device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.b) + co; 
    PC_INC(self, 2); 
    return 8; 
}
Byte ins_rl_c (GB_device* device) { 
    Byte oldC = GB_cpu_get_carry_flag_bit(device->cpu);
    Byte co = (device->cpu->registers.c & 0x80) ? 0x10 : 0;
    device->cpu->registers.c = (device->cpu->registers.c << 1) + oldC; 
    device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.c) + co; 
    PC_INC(self, 2); 
    return 8; 
}
Byte ins_rl_d (GB_device* device) { 
    Byte oldC = GB_cpu_get_carry_flag_bit(device->cpu);
    Byte co = (device->cpu->registers.d & 0x80) ? 0x10 : 0;
    device->cpu->registers.d = (device->cpu->registers.d << 1) + oldC; 
    device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.d) + co; 
    PC_INC(self, 2); 
    return 8; 
}
Byte ins_rl_e (GB_device* device) { 
    Byte oldC = GB_cpu_get_carry_flag_bit(device->cpu);
    Byte co = (device->cpu->registers.e & 0x80) ? 0x10 : 0;
    device->cpu->registers.e = (device->cpu->registers.e << 1) + oldC; 
    device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.e) + co; 
    PC_INC(self, 2); 
    return 8; 
}
Byte ins_rl_h (GB_device* device) { 
    Byte oldC = GB_cpu_get_carry_flag_bit(device->cpu);
    Byte co = (device->cpu->registers.h & 0x80) ? 0x10 : 0;
    device->cpu->registers.h = (device->cpu->registers.h << 1) + oldC; 
    device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.h) + co; 
    PC_INC(self, 2); 
    return 8; 
}
Byte ins_rl_l (GB_device* device) { 
    Byte oldC = GB_cpu_get_carry_flag_bit(device->cpu);
    Byte co = (device->cpu->registers.l & 0x80) ? 0x10 : 0;
    device->cpu->registers.l = (device->cpu->registers.l << 1) + oldC; 
    device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.l) + co; 
    PC_INC(self, 2); 
    return 8; 
}
Byte ins_rl_hl (GB_device* device) { 
    Word hl = GB_register_get_HL(device->cpu); 
    Byte value = GB_deviceReadByte(device, hl); 
    Byte oldC = GB_cpu_get_carry_flag_bit(device->cpu);
    Byte co = (value & 0x80) ? 0x10 : 0;
    value = (value << 1) + oldC; 
    GB_deviceWriteByte(device,hl, value); 
    device->cpu->registers.f = ZeroFlagValue(value) + co; 
    PC_INC(self, 2); 
    return 16; 
}

Byte ins_rra (GB_device* device)  { 
    Byte oldC = GB_cpu_get_carry_flag_bit(device->cpu);
    Byte lbit = (device->cpu->registers.a & 0x01);
    
    device->cpu->registers.a = device->cpu->registers.a >> 1;
    device->cpu->registers.f = 0;
    if (oldC) {
        device->cpu->registers.a |= 0x80;
    }
    if (lbit) {
        device->cpu->registers.f |= FLAG_CARRY;
    }
    PC_INC(self, 1); 
    return 4; 
}

Byte ins_rr_a (GB_device* device) { bool cary = GB_cpu_get_carry_flag_bit(device->cpu); bool lbit = (device->cpu->registers.a & 0x01) != 0; device->cpu->registers.a = (device->cpu->registers.a >> 1) | (cary << 7); device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.a) | (lbit << 4); PC_INC(self, 2); return 8; }
Byte ins_rr_b (GB_device* device) { bool cary = GB_cpu_get_carry_flag_bit(device->cpu); bool lbit = (device->cpu->registers.b & 0x01) != 0; device->cpu->registers.b = (device->cpu->registers.b >> 1) | (cary << 7); device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.b) | (lbit << 4); PC_INC(self, 2); return 8; }
Byte ins_rr_c (GB_device* device) { 
    bool lbit = (device->cpu->registers.c & 0x01) != 0;
    bool cary = GB_cpu_get_carry_flag_bit(device->cpu);
    device->cpu->registers.c = (device->cpu->registers.c >> 1) | (cary << 7); 
    device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.c) | (lbit << 4); 
    PC_INC(self, 2); 
    return 8;
}
Byte ins_rr_d (GB_device* device) 
{
    bool cary = GB_cpu_get_carry_flag_bit(device->cpu);
    bool lbit = (device->cpu->registers.d & 0x01) != 0; 
    device->cpu->registers.d = (device->cpu->registers.d >> 1) | (cary << 7); 
    device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.d) | (lbit << 4); 
    PC_INC(self, 2); 
    return 8; 
}
Byte ins_rr_e (GB_device* device) { bool cary = GB_cpu_get_carry_flag_bit(device->cpu); bool lbit = (device->cpu->registers.e & 0x01) != 0; device->cpu->registers.e = (device->cpu->registers.e >> 1) | (cary << 7); device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.e) | (lbit << 4); PC_INC(self, 2); return 8; }
Byte ins_rr_h (GB_device* device) { bool cary = GB_cpu_get_carry_flag_bit(device->cpu); bool lbit = (device->cpu->registers.h & 0x01) != 0; device->cpu->registers.h = (device->cpu->registers.h >> 1) | (cary << 7); device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.h) | (lbit << 4); PC_INC(self, 2); return 8; }
Byte ins_rr_l (GB_device* device) { bool cary = GB_cpu_get_carry_flag_bit(device->cpu); bool lbit = (device->cpu->registers.l & 0x01) != 0; device->cpu->registers.l = (device->cpu->registers.l >> 1) | (cary << 7); device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.l) | (lbit << 4); PC_INC(self, 2); return 8; }
Byte ins_rr_hl (GB_device* device) { 
    Word hl = GB_register_get_HL(device->cpu); 
    Byte value = GB_deviceReadByte(device, hl); 
    bool cary = GB_cpu_get_carry_flag_bit(device->cpu);
    bool lbit = (value & 0x01) != 0; 
    value = (value >> 1) | (cary << 7); 
    device->cpu->registers.f = ZeroFlagValue(value) | (lbit << 4); 
    GB_deviceWriteByte(device,hl, value);
    PC_INC(self, 2); 
    return 12; 
}

Byte ins_sla_a (GB_device* device) { Byte hBit = device->cpu->registers.a >> 7; device->cpu->registers.a = device->cpu->registers.a << 1; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.a) | (hBit << 4); PC_INC(self, 2); return 8; }
Byte ins_sla_b (GB_device* device) { Byte hBit = device->cpu->registers.b >> 7; device->cpu->registers.b = device->cpu->registers.b << 1; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.b) | (hBit << 4); PC_INC(self, 2); return 8; }
Byte ins_sla_c (GB_device* device) { Byte hBit = device->cpu->registers.c >> 7; device->cpu->registers.c = device->cpu->registers.c << 1; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.c) | (hBit << 4); PC_INC(self, 2); return 8; }
Byte ins_sla_d (GB_device* device) { Byte hBit = device->cpu->registers.d >> 7; device->cpu->registers.d = device->cpu->registers.d << 1; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.d) | (hBit << 4); PC_INC(self, 2); return 8; }
Byte ins_sla_e (GB_device* device) { Byte hBit = device->cpu->registers.e >> 7; device->cpu->registers.e = device->cpu->registers.e << 1; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.e) | (hBit << 4); PC_INC(self, 2); return 8; }
Byte ins_sla_h (GB_device* device) { Byte hBit = device->cpu->registers.h >> 7; device->cpu->registers.h = device->cpu->registers.h << 1; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.h) | (hBit << 4); PC_INC(self, 2); return 8; }
Byte ins_sla_l (GB_device* device) { Byte hBit = device->cpu->registers.l >> 7; device->cpu->registers.l = device->cpu->registers.l << 1; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.l) | (hBit << 4); PC_INC(self, 2); return 8; }
Byte ins_sla_hl (GB_device* device) { Word hl = GB_register_get_HL(device->cpu); Byte value = GB_deviceReadByte(device, hl); Byte hBit = value >> 7; value = value << 1; device->cpu->registers.f = ZeroFlagValue(value) | (hBit << 4); GB_deviceWriteByte(device,hl, value); PC_INC(self, 2); return 16; }

Byte ins_sra_a (GB_device* device) { 
    Byte hBit = device->cpu->registers.a & 0x1; 
    device->cpu->registers.a = (device->cpu->registers.a >> 1 | device->cpu->registers.a & 0x80) ; 
    device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.a) | (hBit << 4); 
    PC_INC(self, 2); 
    return 8; 
}
Byte ins_sra_b (GB_device* device) { 
    Byte hBit = device->cpu->registers.b & 0x1; 
    device->cpu->registers.b = (device->cpu->registers.b >> 1 | device->cpu->registers.b & 0x80);
    device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.b) | (hBit << 4); 
    PC_INC(self, 2); 
    return 8;
}
Byte ins_sra_c (GB_device* device) { Byte hBit = device->cpu->registers.c & 0x1; device->cpu->registers.c = (device->cpu->registers.c >> 1 | device->cpu->registers.c & 0x80); device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.c) | (hBit << 4); PC_INC(self, 2); return 8; }
Byte ins_sra_d (GB_device* device) { Byte hBit = device->cpu->registers.d & 0x1; device->cpu->registers.d = (device->cpu->registers.d >> 1 | device->cpu->registers.d & 0x80); device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.d) | (hBit << 4); PC_INC(self, 2); return 8; }
Byte ins_sra_e (GB_device* device) { Byte hBit = device->cpu->registers.e & 0x1; device->cpu->registers.e = (device->cpu->registers.e >> 1 | device->cpu->registers.e & 0x80); device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.e) | (hBit << 4); PC_INC(self, 2); return 8; }
Byte ins_sra_h (GB_device* device) { Byte hBit = device->cpu->registers.h & 0x1; device->cpu->registers.h = (device->cpu->registers.h >> 1 | device->cpu->registers.h & 0x80); device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.h) | (hBit << 4); PC_INC(self, 2); return 8; }
Byte ins_sra_l (GB_device* device) { Byte hBit = device->cpu->registers.l & 0x1; device->cpu->registers.l = (device->cpu->registers.l >> 1 | device->cpu->registers.l & 0x80); device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.l) | (hBit << 4); PC_INC(self, 2); return 8; }
Byte ins_sra_hl (GB_device* device) { Word hl = GB_register_get_HL(device->cpu); Byte value = GB_deviceReadByte(device, hl); Byte hBit = value & 0x1; value = (value >> 1 | value & 0x80); device->cpu->registers.f = ZeroFlagValue(value) | (hBit << 4); GB_deviceWriteByte(device,hl, value); PC_INC(self, 2); return 16; }

Byte ins_srl_a (GB_device* device) { Byte lBit = device->cpu->registers.a & 0x01; device->cpu->registers.a = device->cpu->registers.a >> 1; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.a) | (lBit << 4); PC_INC(self, 2); return 8; }
Byte ins_srl_b (GB_device* device) { 
    Byte lBit = device->cpu->registers.b & 0x01; 
    device->cpu->registers.b = device->cpu->registers.b >> 1; 
    device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.b) | (lBit << 4);
    PC_INC(self, 2); 
    return 8; 
}
Byte ins_srl_c (GB_device* device) { Byte lBit = device->cpu->registers.c & 0x01; device->cpu->registers.c = device->cpu->registers.c >> 1; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.c) | (lBit << 4); PC_INC(self, 2); return 8; }
Byte ins_srl_d (GB_device* device) { Byte lBit = device->cpu->registers.d & 0x01; device->cpu->registers.d = device->cpu->registers.d >> 1; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.d) | (lBit << 4); PC_INC(self, 2); return 8; }
Byte ins_srl_e (GB_device* device) { Byte lBit = device->cpu->registers.e & 0x01; device->cpu->registers.e = device->cpu->registers.e >> 1; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.e) | (lBit << 4); PC_INC(self, 2); return 8; }
Byte ins_srl_h (GB_device* device) { Byte lBit = device->cpu->registers.h & 0x01; device->cpu->registers.h = device->cpu->registers.h >> 1; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.h) | (lBit << 4); PC_INC(self, 2); return 8; }
Byte ins_srl_l (GB_device* device) { Byte lBit = device->cpu->registers.l & 0x01; device->cpu->registers.l = device->cpu->registers.l >> 1; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.l) | (lBit << 4); PC_INC(self, 2); return 8; }
Byte ins_srl_hl (GB_device* device) { Word hl = GB_register_get_HL(device->cpu); Byte value = GB_deviceReadByte(device, hl); Byte lBit = value & 0x01; value = value >> 1; device->cpu->registers.f = ZeroFlagValue(value) | (lBit << 4); GB_deviceWriteByte(device,hl, value); PC_INC(self, 2); return 16; }


Byte ins_swap_a(GB_device* device) { device->cpu->registers.a = ((device->cpu->registers.a & 0xf0) >> 4) | ((device->cpu->registers.a & 0x0f)) << 4; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.a); PC_INC(self, 2); return 8; }
Byte ins_swap_b(GB_device* device) { device->cpu->registers.b = ((device->cpu->registers.b & 0xf0) >> 4) | ((device->cpu->registers.b & 0x0f)) << 4; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.b); PC_INC(self, 2); return 8; }
Byte ins_swap_c(GB_device* device) { device->cpu->registers.c = ((device->cpu->registers.c & 0xf0) >> 4) | ((device->cpu->registers.c & 0x0f)) << 4; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.c); PC_INC(self, 2); return 8; }
Byte ins_swap_d(GB_device* device) { device->cpu->registers.d = ((device->cpu->registers.d & 0xf0) >> 4) | ((device->cpu->registers.d & 0x0f)) << 4; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.d); PC_INC(self, 2); return 8; }
Byte ins_swap_e(GB_device* device) { device->cpu->registers.e = ((device->cpu->registers.e & 0xf0) >> 4) | ((device->cpu->registers.e & 0x0f)) << 4; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.e); PC_INC(self, 2); return 8; }
Byte ins_swap_h(GB_device* device) { device->cpu->registers.h = ((device->cpu->registers.h & 0xf0) >> 4) | ((device->cpu->registers.h & 0x0f)) << 4; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.h); PC_INC(self, 2); return 8; }
Byte ins_swap_l(GB_device* device) { device->cpu->registers.l = ((device->cpu->registers.l & 0xf0) >> 4) | ((device->cpu->registers.l & 0x0f)) << 4; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.l); PC_INC(self, 2); return 8; }
Byte ins_swap_hl(GB_device* device) { Word hl = GB_register_get_HL(device->cpu); Byte value = GB_deviceReadByte(device, hl); value = ((value & 0xf0) >> 4) | ((value & 0x0f)) << 4; device->cpu->registers.f = ZeroFlagValue(value); GB_deviceWriteByte(device,hl, value); PC_INC(self, 2); return 16; }

Byte ins_daa1 (GB_device* device) { 
    Byte ajustment = 0;
    if (GB_cpu_get_half_carry_flag_bit(device->cpu) || (GB_cpu_get_subtraction_flag_bit(device->cpu) == 0 && ((device->cpu->registers.a & 0x0f) > 0x09))) {
        ajustment = 6;
    }
    if (GB_cpu_get_carry_flag_bit(device->cpu) || (GB_cpu_get_subtraction_flag_bit(device->cpu) == 0 && device->cpu->registers.a > 0x99)) {
         ajustment |= 0x60;
    }
    Byte value = GB_cpu_get_subtraction_flag_bit(device->cpu) ? device->cpu->registers.a - ajustment : device->cpu->registers.a + ajustment;

    device->cpu->registers.a = value;
    device->cpu->registers.f = ZeroFlagValue(value) | GB_cpu_get_subtraction_flag_bit(device->cpu) << 6 | ((ajustment > 6) ? FLAG_CARRY : 0);
    PC_INC(self, 1); 
    return 4;
}

Byte ins_daa2 (GB_device* device) { 
    int result = device->cpu->registers.a;
    unsigned short mask = 0xFF00;
    GB_register_set_AF(device->cpu, ~(mask | FLAG_ZERO));
    if(device->cpu->registers.f & FLAG_SUB) {
        if (device->cpu->registers.f & FLAG_HALF) {
            result = (result - 0x06);
        }
        if (device->cpu->registers.f & FLAG_CARRY) {
            result -= 0x60;
        }
    } else {
        if ((device->cpu->registers.f & FLAG_HALF) || (result & 0x0F) > 0x09) {
            result += 0x06;
        }
        if ((device->cpu->registers.f & FLAG_CARRY) || result > 0x9F) {
            result += 0x60;
        }
    }
    if ((result & 0xFF) == 0) {
        device->cpu->registers.f |= FLAG_ZERO;
    }

    if ((result & 0x100) == 0x100) {
        device->cpu->registers.f |= FLAG_CARRY;
    }

    device->cpu->registers.a |= result;
    device->cpu->registers.f &= ~FLAG_HALF;
    PC_INC(self, 1);
    return 4;
}

Byte ins_daa (GB_device* device) { 
    int result = device->cpu->registers.a;
    if(device->cpu->registers.f & FLAG_SUB) {
        if (device->cpu->registers.f & FLAG_HALF) {
            result -= 0x06;
            if (! (device->cpu->registers.f & FLAG_CARRY)) {
                result &= 0xff;
            }
        }
        if (device->cpu->registers.f & FLAG_CARRY) {
            result -= 0x60;
        }
    } else {
        if ((device->cpu->registers.f & FLAG_HALF) || (result & 0x0F) > 0x09) {
            result += 0x06;
        }
        if ((device->cpu->registers.f & FLAG_CARRY) || result > 0x9F) {
            result += 0x60;
        }
    }

    device->cpu->registers.f &= ~ (FLAG_HALF | FLAG_ZERO);

    if (result & 0x100) {
        device->cpu->registers.f |= FLAG_CARRY;
    }
    device->cpu->registers.a = result & 0xff;

    if (! device->cpu->registers.a) {
        device->cpu->registers.f |= FLAG_ZERO;
    }

    PC_INC(self, 1);
    return 4;
}

Byte ins_cpl(GB_device* device) {
    device->cpu->registers.a = ~device->cpu->registers.a;
    device->cpu->registers.f = GB_cpu_zero_flag(device->cpu) | FLAG_SUB | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);
    PC_INC(self, 1);
    return 4;
}


Byte ins_jr_x    (GB_device* device) { device->cpu->registers.pc += 2 + ((char)GB_deviceReadByte(device, device->cpu->registers.pc + 1)); return 12; }
Byte ins_jr_nz_x (GB_device* device) { 
    if(GB_cpu_zero_flag(device->cpu) == 0) { 
        device->cpu->registers.pc += 2 + ((char)GB_cpu_fetch_byte(device, 1)); 
        return 12;
    }
    PC_INC(self, 2);
    return 8;
 }
Byte ins_jr_nc_x (GB_device* device) { 
    if(GB_cpu_get_carry_flag_bit(device->cpu) == 0) { 
        device->cpu->registers.pc += 2 +((char) GB_cpu_fetch_byte(device, 1)); 
        return 12;
    }
    PC_INC(self, 2);
    return 8;
}
Byte ins_jr_c_x (GB_device* device) { 
    if(GB_cpu_get_carry_flag_bit(device->cpu) != 0) { 
        device->cpu->registers.pc += 2 + ((char) GB_cpu_fetch_byte(device, 1)); 
        return 12;
    }
    PC_INC(self, 2);
    return 8;
}
Byte ins_jr_z_x  (GB_device* device) { 
    if(GB_cpu_zero_flag(device->cpu) != 0) { 
        device->cpu->registers.pc += 2 + ((char)GB_cpu_fetch_byte(device, 1)); 
        return 12;
    }
    PC_INC(self, 2);
    return 8;
 }

Byte ins_jp_xx    (GB_device* device) { device->cpu->registers.pc = GB_cpu_fetch_word(device, 1); return 16; }
Byte ins_jp_nz_xx (GB_device* device) { 
    if(GB_cpu_zero_flag(device->cpu) == 0) { 
        device->cpu->registers.pc = GB_cpu_fetch_word(device, 1); 
        return 16;
    }
    PC_INC(self, 3);
    return 12; 
}
Byte ins_jp_z_xx (GB_device* device) { if(GB_cpu_zero_flag(device->cpu) != 0) { device->cpu->registers.pc = GB_cpu_fetch_word(device, 1); return 16;}  PC_INC(self, 3); return 12; }
Byte ins_jp_nc_xx (GB_device* device) { 
    if(GB_cpu_get_carry_flag(device->cpu) == 0) { 
        device->cpu->registers.pc = GB_cpu_fetch_word(device, 1);
        return 16;
    }
    PC_INC(self, 3); 
    return 12; 
}
Byte ins_jp_c_xx (GB_device* device) { if(GB_cpu_get_carry_flag(device->cpu) != 0) { device->cpu->registers.pc = GB_cpu_fetch_word(device, 1); return 16;} PC_INC(self, 3); return 12; }
Byte ins_jp_hl   (GB_device* device) { device->cpu->registers.pc = GB_register_get_HL(device->cpu); return 4; }

Byte ins_add_hl_bc(GB_device* device) { 
    Word x1 = GB_register_get_HL(device->cpu), x2 = GB_register_get_BC(device->cpu);
    Word value = x1 + x2;
    GB_register_set_HL(device->cpu, value);
    device->cpu->registers.f = (device->cpu->registers.f & FLAG_ZERO) | HalfCarryFlagValueW(x1, x2) | CarryFlagValueAdd(value, x1, x2);
    PC_INC(self, 1);
    return 8; 
}
Byte ins_add_hl_de(GB_device* device) { 
    Word x1 = GB_register_get_HL(device->cpu), x2 = GB_register_get_DE(device->cpu);
    Word value = x1 + x2;
    GB_register_set_HL(device->cpu, value);
    device->cpu->registers.f = (device->cpu->registers.f & FLAG_ZERO) | HalfCarryFlagValueW(x1, x2) | CarryFlagValueAdd(value, x1, x2);
    PC_INC(self, 1);
    return 8; 
}
Byte ins_add_hl_hl(GB_device* device) { 
    Word x1 = GB_register_get_HL(device->cpu);
    Word value = x1 + x1;
    GB_register_set_HL(device->cpu, value);
    device->cpu->registers.f = (device->cpu->registers.f & FLAG_ZERO) | HalfCarryFlagValueW(x1, x1) | CarryFlagValueAdd(value, x1, x1);
    PC_INC(self, 1);
    return 8;
}
Byte ins_add_hl_sp(GB_device* device) { 
    Word x1 = GB_register_get_HL(device->cpu), x2 = device->cpu->registers.sp;
    Word value = x1 + x2;
    GB_register_set_HL(device->cpu, value);
    device->cpu->registers.f = (device->cpu->registers.f & FLAG_ZERO) | HalfCarryFlagValueW(x1, x2) | CarryFlagValueAdd(value, x1, x2);
    PC_INC(self, 1);
    return 8;
}

Byte ins_add_a_a(GB_device* device) { 
    Byte value = device->cpu->registers.a + device->cpu->registers.a; 
    device->cpu->registers.f = ZeroFlagValue(value) | HalfCarryFlagValue(device->cpu->registers.a, device->cpu->registers.a) | CarryFlagValueAdd(value, device->cpu->registers.a, device->cpu->registers.a); 
    device->cpu->registers.a = value; 
    PC_INC(self, 1); 
    return 4; 
}
Byte ins_add_a_b(GB_device* device) { 
    Byte value = device->cpu->registers.a + device->cpu->registers.b; 
    device->cpu->registers.f = ZeroFlagValue(value) | HalfCarryFlagValue(device->cpu->registers.a, device->cpu->registers.b) | CarryFlagValueAdd(value, device->cpu->registers.a, device->cpu->registers.b); 
    device->cpu->registers.a = value; 
    PC_INC(self, 1); 
    return 4; 
}
Byte ins_add_a_c(GB_device* device) { 
    Byte value = device->cpu->registers.a + device->cpu->registers.c; 
    device->cpu->registers.f = ZeroFlagValue(value) | HalfCarryFlagValue(device->cpu->registers.a, device->cpu->registers.c) | CarryFlagValueAdd(value, device->cpu->registers.a, device->cpu->registers.c); 
    device->cpu->registers.a = value;
    PC_INC(self, 1);
    return 4; 
}
Byte ins_add_a_d(GB_device* device) { 
    Byte value = device->cpu->registers.a + device->cpu->registers.d; 
    device->cpu->registers.f = ZeroFlagValue(value) | HalfCarryFlagValue(device->cpu->registers.a, device->cpu->registers.d) | CarryFlagValueAdd(value, device->cpu->registers.a, device->cpu->registers.d); 
    device->cpu->registers.a = value;
    PC_INC(self, 1);
    return 4; 
}
Byte ins_add_a_e(GB_device* device) { 
    Byte value = device->cpu->registers.a + device->cpu->registers.e; 
    device->cpu->registers.f = ZeroFlagValue(value) | HalfCarryFlagValue(device->cpu->registers.a, device->cpu->registers.e) | CarryFlagValueAdd(value, device->cpu->registers.a, device->cpu->registers.e); 
    device->cpu->registers.a = value;
    PC_INC(self, 1);
    return 4; 
}
Byte ins_add_a_h(GB_device* device) { 
    Byte value = device->cpu->registers.a + device->cpu->registers.h; 
    device->cpu->registers.f = ZeroFlagValue(value) | HalfCarryFlagValue(device->cpu->registers.a, device->cpu->registers.h) | CarryFlagValueAdd(value, device->cpu->registers.a, device->cpu->registers.h); 
    device->cpu->registers.a = value;
    PC_INC(self, 1);
    return 4; 
}
Byte ins_add_a_l(GB_device* device) { 
    Byte value = device->cpu->registers.a + device->cpu->registers.l; 
    device->cpu->registers.f = ZeroFlagValue(value) | HalfCarryFlagValue(device->cpu->registers.a, device->cpu->registers.l) | CarryFlagValueAdd(value, device->cpu->registers.a, device->cpu->registers.l); 
    device->cpu->registers.a = value;
    PC_INC(self, 1);
    return 4; 
}
Byte ins_add_a_hl(GB_device* device) { 
    Byte hlValue =  GB_deviceReadByte(device, GB_register_get_HL(device->cpu));
    Byte value = device->cpu->registers.a + hlValue; 
    device->cpu->registers.f = ZeroFlagValue(value) | HalfCarryFlagValue(device->cpu->registers.a, hlValue) | CarryFlagValueAdd(value, device->cpu->registers.a, hlValue); 
    device->cpu->registers.a = value;
    PC_INC(self, 1);
    return 4; 
}
Byte ins_add_a_x(GB_device* device) { 
    Byte x = GB_cpu_fetch_byte(device, 1);
    Byte value = device->cpu->registers.a + x; 
    device->cpu->registers.f = ZeroFlagValue(value) | HalfCarryFlagValue(device->cpu->registers.a, x) | CarryFlagValueAdd(value, device->cpu->registers.a, x); 
    device->cpu->registers.a = value;
    PC_INC(self, 2);
    return 4; 
}
Byte ins_add_sp_x(GB_device* device) { 
    int16_t offset = (int8_t) GB_cpu_fetch_byte(device, 1);
    Word sp = device->cpu->registers.sp;
    device->cpu->registers.sp += offset;

    device->cpu->registers.f = 0;

    /* A new instruction, a new meaning for Half Carry! Thanks Sameboy */
    if ((sp & 0xF) + (offset & 0xF) > 0xF) {
       device->cpu->registers.f |= FLAG_HALF;
    }
    if ((sp & 0xFF) + (offset & 0xFF) > 0xFF)  {
        device->cpu->registers.f |= FLAG_CARRY;
    }
    PC_INC(self, 2);
    return 16; 
}

Byte ins_sub_a_a(GB_device* device) { 
    Byte value = device->cpu->registers.a - device->cpu->registers.a; 
    device->cpu->registers.f = ZeroFlagValue(value) | FLAG_SUB | HalfCarrySubFlagValue(device->cpu->registers.a, device->cpu->registers.a) | CarrySubFlagValueAdd(device->cpu->registers.a, device->cpu->registers.a); 
    device->cpu->registers.a = value; 
    PC_INC(self, 1); 
    return 4; 
}
Byte ins_sub_a_b(GB_device* device) { 
    Byte value = device->cpu->registers.a - device->cpu->registers.b; 
    device->cpu->registers.f = ZeroFlagValue(value) | FLAG_SUB | HalfCarrySubFlagValue(device->cpu->registers.a, device->cpu->registers.b) | CarrySubFlagValueAdd(device->cpu->registers.a, device->cpu->registers.b); 
    device->cpu->registers.a = value; 
    PC_INC(self, 1); 
    return 4; 
}
Byte ins_sub_a_c(GB_device* device) {
    Byte value = device->cpu->registers.a - device->cpu->registers.c; 
    device->cpu->registers.f = ZeroFlagValue(value) | FLAG_SUB | HalfCarrySubFlagValue(device->cpu->registers.a, device->cpu->registers.c) | CarrySubFlagValueAdd(device->cpu->registers.a, device->cpu->registers.c); 
    device->cpu->registers.a = value; 
    PC_INC(self, 1); 
    return 4; 
}
Byte ins_sub_a_d(GB_device* device) {
    Byte value = device->cpu->registers.a - device->cpu->registers.d; 
    device->cpu->registers.f = ZeroFlagValue(value) | FLAG_SUB | HalfCarrySubFlagValue(device->cpu->registers.a, device->cpu->registers.d) | CarrySubFlagValueAdd(device->cpu->registers.a, device->cpu->registers.d); 
    device->cpu->registers.a = value; 
    PC_INC(self, 1);
    return 4; 
}
Byte ins_sub_a_e(GB_device* device) {
    Byte value = device->cpu->registers.a - device->cpu->registers.e; 
    device->cpu->registers.f = ZeroFlagValue(value) | FLAG_SUB | HalfCarrySubFlagValue(device->cpu->registers.a, device->cpu->registers.e) | CarrySubFlagValueAdd(device->cpu->registers.a, device->cpu->registers.e); 
    device->cpu->registers.a = value; 
    PC_INC(self, 1);
    return 4; 
}
Byte ins_sub_a_h(GB_device* device) {
    Byte value = device->cpu->registers.a - device->cpu->registers.h; 
    device->cpu->registers.f = ZeroFlagValue(value) | FLAG_SUB | HalfCarrySubFlagValue(device->cpu->registers.a, device->cpu->registers.h) | CarrySubFlagValueAdd(device->cpu->registers.a, device->cpu->registers.h); 
    device->cpu->registers.a = value; 
    PC_INC(self, 1);
    return 4; 
}
Byte ins_sub_a_l(GB_device* device) {
    Byte value = device->cpu->registers.a - device->cpu->registers.l; 
    device->cpu->registers.f = ZeroFlagValue(value) | FLAG_SUB | HalfCarrySubFlagValue(device->cpu->registers.a, device->cpu->registers.l) | CarrySubFlagValueAdd(device->cpu->registers.a, device->cpu->registers.l); 
    device->cpu->registers.a = value; 
    PC_INC(self, 1);
    return 4; 
}
Byte ins_sub_a_hl(GB_device* device) {
    Byte hlValue =  GB_deviceReadByte(device, GB_register_get_HL(device->cpu));
    Byte value = device->cpu->registers.a - hlValue; 
    device->cpu->registers.f = ZeroFlagValue(value) | FLAG_SUB | HalfCarrySubFlagValue(device->cpu->registers.a, hlValue) | CarrySubFlagValueAdd(device->cpu->registers.a, hlValue); 
    device->cpu->registers.a = value; 
    PC_INC(self, 1);
    return 4; 
}
Byte ins_sub_a_x(GB_device* device) {
    Byte x = GB_cpu_fetch_byte(device, 1);
    Byte value = device->cpu->registers.a - x; 
    device->cpu->registers.f = ZeroFlagValue(value) | FLAG_SUB | HalfCarrySubFlagValue(device->cpu->registers.a, x) | CarrySubFlagValueAdd(device->cpu->registers.a, x); 
    device->cpu->registers.a = value; 
    PC_INC(self, 2);
    return 4; 
}

Byte ins_adc_a_a(GB_device* device) { 
    Byte c = GB_cpu_get_carry_flag_bit(device->cpu); 
    Byte v = device->cpu->registers.a + device->cpu->registers.a + c; 
    device->cpu->registers.f = ZeroFlagValue(v) | HalfCarryValueC(device->cpu->registers.a, device->cpu->registers.a, c) | CarryFlagValueAddC(v, device->cpu->registers.a, device->cpu->registers.a, c); 
    device->cpu->registers.a = v;
    PC_INC(self, 1); 
    return 4; 
}
Byte ins_adc_a_b(GB_device* device) { Byte c = GB_cpu_get_carry_flag_bit(device->cpu); Byte v = device->cpu->registers.a + device->cpu->registers.b + c; device->cpu->registers.f = ZeroFlagValue(v) | HalfCarryValueC(device->cpu->registers.a, device->cpu->registers.b, c) | CarryFlagValueAddC(v, device->cpu->registers.a, device->cpu->registers.b, c); device->cpu->registers.a = v;PC_INC(self, 1); return 4; }
Byte ins_adc_a_c(GB_device* device) { Byte c = GB_cpu_get_carry_flag_bit(device->cpu); Byte v = device->cpu->registers.a + device->cpu->registers.c + c; device->cpu->registers.f = ZeroFlagValue(v) | HalfCarryValueC(device->cpu->registers.a, device->cpu->registers.c, c) | CarryFlagValueAddC(v, device->cpu->registers.a, device->cpu->registers.c, c); device->cpu->registers.a = v;PC_INC(self, 1); return 4; }
Byte ins_adc_a_d(GB_device* device) { Byte c = GB_cpu_get_carry_flag_bit(device->cpu); Byte v = device->cpu->registers.a + device->cpu->registers.d + c; device->cpu->registers.f = ZeroFlagValue(v) | HalfCarryValueC(device->cpu->registers.a, device->cpu->registers.d, c) | CarryFlagValueAddC(v, device->cpu->registers.a, device->cpu->registers.d, c); device->cpu->registers.a = v;PC_INC(self, 1); return 4; }
Byte ins_adc_a_e(GB_device* device) { Byte c = GB_cpu_get_carry_flag_bit(device->cpu); Byte v = device->cpu->registers.a + device->cpu->registers.e + c; device->cpu->registers.f = ZeroFlagValue(v) | HalfCarryValueC(device->cpu->registers.a, device->cpu->registers.e, c) | CarryFlagValueAddC(v, device->cpu->registers.a, device->cpu->registers.e, c); device->cpu->registers.a = v;PC_INC(self, 1); return 4; }
Byte ins_adc_a_h(GB_device* device) { Byte c = GB_cpu_get_carry_flag_bit(device->cpu); Byte v = device->cpu->registers.a + device->cpu->registers.h + c; device->cpu->registers.f = ZeroFlagValue(v) | HalfCarryValueC(device->cpu->registers.a, device->cpu->registers.h, c) | CarryFlagValueAddC(v, device->cpu->registers.a, device->cpu->registers.h, c); device->cpu->registers.a = v;PC_INC(self, 1); return 4; }
Byte ins_adc_a_l(GB_device* device) { Byte c = GB_cpu_get_carry_flag_bit(device->cpu); Byte v = device->cpu->registers.a + device->cpu->registers.l + c; device->cpu->registers.f = ZeroFlagValue(v) | HalfCarryValueC(device->cpu->registers.a, device->cpu->registers.l, c) | CarryFlagValueAddC(v, device->cpu->registers.a, device->cpu->registers.l, c); device->cpu->registers.a = v;PC_INC(self, 1); return 4; }
Byte ins_adc_a_x(GB_device* device) { 
    Byte c = GB_cpu_get_carry_flag_bit(device->cpu), x = GB_cpu_fetch_byte(device, 1); 
    Byte v = device->cpu->registers.a + x + c; 
    device->cpu->registers.f = ZeroFlagValue(v) | HalfCarryValueC(device->cpu->registers.a, x, c) | CarryFlagValueAddC(v, device->cpu->registers.a, x, c); 
    device->cpu->registers.a = v;
    PC_INC(self, 2); 
    return 8; 
}
Byte ins_adc_a_hl(GB_device* device){ 
    Byte c = GB_cpu_get_carry_flag_bit(device->cpu), x2 = GB_deviceReadByte(device, GB_register_get_HL(device->cpu)); 
    Byte v = device->cpu->registers.a + x2 + c; 
    device->cpu->registers.f = ZeroFlagValue(v) | HalfCarryValueC(device->cpu->registers.a, x2, c) | CarryFlagValueAddC(v, device->cpu->registers.a, x2, c); 
    device->cpu->registers.a = v; 
    PC_INC(self, 1); 
    return 8; 
}

Byte ins_sdc_a_a(GB_device* device) { Byte c = GB_cpu_get_carry_flag_bit(device->cpu); Byte v = device->cpu->registers.a - device->cpu->registers.a - c; device->cpu->registers.f = ZeroFlagValue(v) | FLAG_SUB | HalfCarrySubFlagValueC(device->cpu->registers.a, device->cpu->registers.a, c) | CarrySubFlagValueAddC(device->cpu->registers.a, device->cpu->registers.a, c); device->cpu->registers.a = v; PC_INC(self, 1); return 4; }
Byte ins_sdc_a_b(GB_device* device) { Byte c = GB_cpu_get_carry_flag_bit(device->cpu); Byte v = device->cpu->registers.a - device->cpu->registers.b - c; device->cpu->registers.f = ZeroFlagValue(v) | FLAG_SUB | HalfCarrySubFlagValueC(device->cpu->registers.a, device->cpu->registers.b, c) | CarrySubFlagValueAddC(device->cpu->registers.a, device->cpu->registers.b, c); device->cpu->registers.a = v; PC_INC(self, 1); return 4; }
Byte ins_sdc_a_c(GB_device* device) { Byte c = GB_cpu_get_carry_flag_bit(device->cpu); Byte v = device->cpu->registers.a - device->cpu->registers.c - c; device->cpu->registers.f = ZeroFlagValue(v) | FLAG_SUB | HalfCarrySubFlagValueC(device->cpu->registers.a, device->cpu->registers.c, c) | CarrySubFlagValueAddC(device->cpu->registers.a, device->cpu->registers.c, c); device->cpu->registers.a = v; PC_INC(self, 1); return 4; }
Byte ins_sdc_a_d(GB_device* device) { Byte c = GB_cpu_get_carry_flag_bit(device->cpu); Byte v = device->cpu->registers.a - device->cpu->registers.d - c; device->cpu->registers.f = ZeroFlagValue(v) | FLAG_SUB | HalfCarrySubFlagValueC(device->cpu->registers.a, device->cpu->registers.d, c) | CarrySubFlagValueAddC(device->cpu->registers.a, device->cpu->registers.d, c); device->cpu->registers.a = v; PC_INC(self, 1); return 4; }
Byte ins_sdc_a_e(GB_device* device) { Byte c = GB_cpu_get_carry_flag_bit(device->cpu); Byte v = device->cpu->registers.a - device->cpu->registers.e - c; device->cpu->registers.f = ZeroFlagValue(v) | FLAG_SUB | HalfCarrySubFlagValueC(device->cpu->registers.a, device->cpu->registers.e, c) | CarrySubFlagValueAddC(device->cpu->registers.a, device->cpu->registers.e, c); device->cpu->registers.a = v; PC_INC(self, 1); return 4; }
Byte ins_sdc_a_h(GB_device* device) { Byte c = GB_cpu_get_carry_flag_bit(device->cpu); Byte v = device->cpu->registers.a - device->cpu->registers.h - c; device->cpu->registers.f = ZeroFlagValue(v) | FLAG_SUB | HalfCarrySubFlagValueC(device->cpu->registers.a, device->cpu->registers.h, c) | CarrySubFlagValueAddC(device->cpu->registers.a, device->cpu->registers.h, c); device->cpu->registers.a = v; PC_INC(self, 1); return 4; }
Byte ins_sdc_a_l(GB_device* device) { Byte c = GB_cpu_get_carry_flag_bit(device->cpu); Byte v = device->cpu->registers.a - device->cpu->registers.l - c; device->cpu->registers.f = ZeroFlagValue(v) | FLAG_SUB | HalfCarrySubFlagValueC(device->cpu->registers.a, device->cpu->registers.l, c) | CarrySubFlagValueAddC(device->cpu->registers.a, device->cpu->registers.l, c); device->cpu->registers.a = v; PC_INC(self, 1); return 4; }
Byte ins_sdc_a_x(GB_device* device) { Byte c = GB_cpu_get_carry_flag_bit(device->cpu), x = GB_cpu_fetch_byte(device, 1); Byte v = device->cpu->registers.a - x - c; device->cpu->registers.f = ZeroFlagValue(v) | FLAG_SUB | HalfCarrySubFlagValueC(device->cpu->registers.a, x, c) | CarrySubFlagValueAddC(device->cpu->registers.a, x, c); device->cpu->registers.a = v; PC_INC(self, 2); return 8; }
Byte ins_sdc_a_hl(GB_device* device){ Byte c = GB_cpu_get_carry_flag_bit(device->cpu), x2 = GB_deviceReadByte(device, GB_register_get_HL(device->cpu)); Byte v = device->cpu->registers.a - x2 - c; device->cpu->registers.f = ZeroFlagValue(v) | FLAG_SUB | HalfCarrySubFlagValueC(device->cpu->registers.a, x2, c) | CarrySubFlagValueAddC(device->cpu->registers.a, x2, c); device->cpu->registers.a = v; PC_INC(self, 1); return 8; }

Byte ins_and_a_a(GB_device* device) { device->cpu->registers.a = device->cpu->registers.a & device->cpu->registers.a; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.a) | FLAG_HALF; PC_INC(self, 1); return 4; }
Byte ins_and_a_b(GB_device* device) { device->cpu->registers.a = device->cpu->registers.a & device->cpu->registers.b; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.a) | FLAG_HALF; PC_INC(self, 1); return 4; }
Byte ins_and_a_c(GB_device* device) { device->cpu->registers.a = device->cpu->registers.a & device->cpu->registers.c; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.a) | FLAG_HALF; PC_INC(self, 1); return 4; }
Byte ins_and_a_d(GB_device* device) { device->cpu->registers.a = device->cpu->registers.a & device->cpu->registers.d; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.a) | FLAG_HALF; PC_INC(self, 1); return 4; }
Byte ins_and_a_e(GB_device* device) { device->cpu->registers.a = device->cpu->registers.a & device->cpu->registers.e; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.a) | FLAG_HALF; PC_INC(self, 1); return 4; }
Byte ins_and_a_h(GB_device* device) { device->cpu->registers.a = device->cpu->registers.a & device->cpu->registers.h; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.a) | FLAG_HALF; PC_INC(self, 1); return 4; }
Byte ins_and_a_l(GB_device* device) { device->cpu->registers.a = device->cpu->registers.a & device->cpu->registers.l; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.a) | FLAG_HALF; PC_INC(self, 1); return 4; }
Byte ins_and_a_hl(GB_device* device) { Byte hlv =  GB_deviceReadByte(device, GB_register_get_HL(device->cpu)); device->cpu->registers.a = device->cpu->registers.a & hlv; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.a) | FLAG_HALF; PC_INC(self, 1); return 8; }
Byte ins_and_a_x(GB_device* device) { 
    Byte val = GB_cpu_fetch_byte(device, 1);
    device->cpu->registers.a = device->cpu->registers.a & val; 
    device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.a) | FLAG_HALF; 
    PC_INC(self, 2); 
    return 8; 
}

Byte ins_or_a_a(GB_device* device) { device->cpu->registers.a = device->cpu->registers.a | device->cpu->registers.a; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.a); PC_INC(self, 1); return 4; }
Byte ins_or_a_b(GB_device* device) { device->cpu->registers.a = device->cpu->registers.a | device->cpu->registers.b; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.a); PC_INC(self, 1); return 4; }
Byte ins_or_a_c(GB_device* device) { device->cpu->registers.a = device->cpu->registers.a | device->cpu->registers.c; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.a); PC_INC(self, 1); return 4; }
Byte ins_or_a_d(GB_device* device) { device->cpu->registers.a = device->cpu->registers.a | device->cpu->registers.d; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.a); PC_INC(self, 1); return 4; }
Byte ins_or_a_e(GB_device* device) { device->cpu->registers.a = device->cpu->registers.a | device->cpu->registers.e; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.a); PC_INC(self, 1); return 4; }
Byte ins_or_a_h(GB_device* device) { device->cpu->registers.a = device->cpu->registers.a | device->cpu->registers.h; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.a); PC_INC(self, 1); return 4; }
Byte ins_or_a_l(GB_device* device) { device->cpu->registers.a = device->cpu->registers.a | device->cpu->registers.l; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.a); PC_INC(self, 1); return 4; }
Byte ins_or_a_x(GB_device* device) { device->cpu->registers.a = device->cpu->registers.a | GB_cpu_fetch_byte(device, 1); device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.a); PC_INC(self, 2); return 8; }
Byte ins_or_a_hl(GB_device* device) { Byte hlv =  GB_deviceReadByte(device, GB_register_get_HL(device->cpu)); device->cpu->registers.a = device->cpu->registers.a | hlv; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.a); PC_INC(self, 1); return 8; }

Byte ins_xor_a_a(GB_device* device) { device->cpu->registers.a = device->cpu->registers.a ^ device->cpu->registers.a; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.a); PC_INC(self, 1); return 4; }
Byte ins_xor_a_b(GB_device* device) { device->cpu->registers.a = device->cpu->registers.a ^ device->cpu->registers.b; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.a); PC_INC(self, 1); return 4; }
Byte ins_xor_a_c(GB_device* device) { device->cpu->registers.a = device->cpu->registers.a ^ device->cpu->registers.c; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.a); PC_INC(self, 1); return 4; }
Byte ins_xor_a_d(GB_device* device) { device->cpu->registers.a = device->cpu->registers.a ^ device->cpu->registers.d; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.a); PC_INC(self, 1); return 4; }
Byte ins_xor_a_e(GB_device* device) { device->cpu->registers.a = device->cpu->registers.a ^ device->cpu->registers.e; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.a); PC_INC(self, 1); return 4; }
Byte ins_xor_a_h(GB_device* device) { device->cpu->registers.a = device->cpu->registers.a ^ device->cpu->registers.h; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.a); PC_INC(self, 1); return 4; }
Byte ins_xor_a_l(GB_device* device) { device->cpu->registers.a = device->cpu->registers.a ^ device->cpu->registers.l; device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.a); PC_INC(self, 1); return 4; }
Byte ins_xor_a_x(GB_device* device) { device->cpu->registers.a = device->cpu->registers.a ^ GB_cpu_fetch_byte(device, 1); device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.a); PC_INC(self, 2); return 8; }
Byte ins_xor_a_hl(GB_device* device) {
    Byte hlv =  GB_deviceReadByte(device, GB_register_get_HL(device->cpu)); 
    device->cpu->registers.a = device->cpu->registers.a ^ hlv; 
    device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.a); 
    PC_INC(self, 1); 
    return 8;
}

Byte ins_cp_a_a(GB_device* device) { Byte value = device->cpu->registers.a - device->cpu->registers.a;  device->cpu->registers.f = ZeroFlagValue(value) | FLAG_SUB | HalfCarrySubFlagValue(device->cpu->registers.a, device->cpu->registers.a) | CarrySubFlagValueAdd(device->cpu->registers.a, device->cpu->registers.a); PC_INC(self, 1); return 4;  }
Byte ins_cp_a_b(GB_device* device) { 
    Byte value = device->cpu->registers.a - device->cpu->registers.b; 
    device->cpu->registers.f = ZeroFlagValue(value) | FLAG_SUB | HalfCarrySubFlagValue(device->cpu->registers.a, device->cpu->registers.b) | CarrySubFlagValueAdd(device->cpu->registers.a, device->cpu->registers.b); 
    PC_INC(self, 1); 
    return 4; 
}
Byte ins_cp_a_c(GB_device* device) {
    Byte value = device->cpu->registers.a - device->cpu->registers.c; 
    device->cpu->registers.f = ZeroFlagValue(value) | FLAG_SUB | HalfCarrySubFlagValue(device->cpu->registers.a, device->cpu->registers.c) | CarrySubFlagValueAdd(device->cpu->registers.a, device->cpu->registers.c); 
    PC_INC(self, 1); 
    return 4; 
}
Byte ins_cp_a_d(GB_device* device) {
    Byte value = device->cpu->registers.a - device->cpu->registers.d; 
    device->cpu->registers.f = ZeroFlagValue(value) | FLAG_SUB | HalfCarrySubFlagValue(device->cpu->registers.a, device->cpu->registers.d) | CarrySubFlagValueAdd(device->cpu->registers.a, device->cpu->registers.d); 
    PC_INC(self, 1);
    return 4; 
}
Byte ins_cp_a_e(GB_device* device) {
    Byte value = device->cpu->registers.a - device->cpu->registers.e; 
    device->cpu->registers.f = ZeroFlagValue(value) | FLAG_SUB | HalfCarrySubFlagValue(device->cpu->registers.a, device->cpu->registers.e) | CarrySubFlagValueAdd(device->cpu->registers.a, device->cpu->registers.e); 
    PC_INC(self, 1);
    return 4; 
}
Byte ins_cp_a_h(GB_device* device) {
    Byte value = device->cpu->registers.a - device->cpu->registers.h; 
    device->cpu->registers.f = ZeroFlagValue(value) | FLAG_SUB | HalfCarrySubFlagValue(device->cpu->registers.a, device->cpu->registers.h) | CarrySubFlagValueAdd(device->cpu->registers.a, device->cpu->registers.h); 
    PC_INC(self, 1);
    return 4; 
}
Byte ins_cp_a_l(GB_device* device) {
    Byte value = device->cpu->registers.a - device->cpu->registers.l; 
    device->cpu->registers.f = ZeroFlagValue(value) | FLAG_SUB | HalfCarrySubFlagValue(device->cpu->registers.a, device->cpu->registers.l) | CarrySubFlagValueAdd(device->cpu->registers.a, device->cpu->registers.l); 
    PC_INC(self, 1);
    return 4; 
}
Byte ins_cp_a_hl(GB_device* device) {
    Byte hlValue =  GB_deviceReadByte(device, GB_register_get_HL(device->cpu));
    Byte value = device->cpu->registers.a - hlValue; 
    device->cpu->registers.f = ZeroFlagValue(value) | FLAG_SUB | HalfCarrySubFlagValue(device->cpu->registers.a, hlValue) | CarrySubFlagValueAdd(device->cpu->registers.a, hlValue); 
    PC_INC(self, 1);
    return 4; 
}
Byte ins_cp_a_x(GB_device* device) {
    Byte x = GB_cpu_fetch_byte(device, 1);
    Byte value = device->cpu->registers.a - x; 
    device->cpu->registers.f = ZeroFlagValue(value) | FLAG_SUB | HalfCarrySubFlagValue(device->cpu->registers.a, x) | CarrySubFlagValueAdd(device->cpu->registers.a, x); 
    PC_INC(self, 2);
    return 4; 
}

Byte ins_ret_nz(GB_device* device) { if(GB_cpu_zero_flag(device->cpu) == 0) { device->cpu->registers.pc = GB_cpu_pop_stack(device); return 20; } PC_INC(self, 1); return 8; }
Byte ins_ret_z(GB_device* device)  { if(GB_cpu_zero_flag(device->cpu) != 0) { device->cpu->registers.pc = GB_cpu_pop_stack(device); return 20; } PC_INC(self, 1); return 8; }
Byte ins_ret_nc(GB_device* device) { if(GB_cpu_get_carry_flag(device->cpu) == 0) { device->cpu->registers.pc = GB_cpu_pop_stack(device); return 20; } PC_INC(self, 1); return 8; }
Byte ins_ret_c(GB_device* device)  { if(GB_cpu_get_carry_flag(device->cpu) != 0) { device->cpu->registers.pc = GB_cpu_pop_stack(device); return 20; } PC_INC(self, 1); return 8; }
Byte ins_ret(GB_device* device)    { device->cpu->registers.pc = GB_cpu_pop_stack(device); return 16; }
Byte ins_reti(GB_device* device)   { device->cpu->registers.pc = GB_cpu_pop_stack(device); device->cpu->enableINT = 2; return 16; }

Byte ins_pop_bc(GB_device* device) { GB_register_set_BC(device->cpu, GB_cpu_pop_stack(device)); PC_INC(self, 1); return 12; }
Byte ins_pop_de(GB_device* device) { GB_register_set_DE(device->cpu, GB_cpu_pop_stack(device)); PC_INC(self, 1); return 12; }
Byte ins_pop_hl(GB_device* device) { GB_register_set_HL(device->cpu, GB_cpu_pop_stack(device)); PC_INC(self, 1); return 12; }
Byte ins_pop_af(GB_device* device) { GB_register_set_AF(device->cpu, GB_cpu_pop_stack(device) & 0xFFF0); PC_INC(self, 1); return 12; }

Byte ins_push_bc(GB_device* device) {
    GB_cpu_push_stack(device, GB_register_get_BC(device->cpu)); 
    PC_INC(self, 1); 
    return 16; 
}
Byte ins_push_de(GB_device* device) { GB_cpu_push_stack(device, GB_register_get_DE(device->cpu)); PC_INC(self, 1); return 16; }
Byte ins_push_hl(GB_device* device) { GB_cpu_push_stack(device, GB_register_get_HL(device->cpu)); PC_INC(self, 1); return 16; }
Byte ins_push_af(GB_device* device) { GB_cpu_push_stack(device, GB_register_get_AF(device->cpu)); PC_INC(self, 1); return 16; }

Byte ins_call_nz_xx(GB_device* device) { if(GB_cpu_zero_flag(device->cpu) == 0) { GB_cpu_push_stack(device, device->cpu->registers.pc + 3); device->cpu->registers.pc = GB_cpu_fetch_word(device, 1); return 24; } PC_INC(self, 3); return 12; }
Byte ins_call_z_xx(GB_device* device) { if(GB_cpu_zero_flag(device->cpu) != 0) { GB_cpu_push_stack(device, device->cpu->registers.pc + 3); device->cpu->registers.pc = GB_cpu_fetch_word(device, 1); return 24; } PC_INC(self, 3); return 12; }
Byte ins_call_nc_xx(GB_device* device) { if(GB_cpu_get_carry_flag(device->cpu) == 0) { GB_cpu_push_stack(device, device->cpu->registers.pc + 3); device->cpu->registers.pc = GB_cpu_fetch_word(device, 1); return 24; } PC_INC(self, 3); return 12; }
Byte ins_call_c_xx(GB_device* device) { if(GB_cpu_get_carry_flag(device->cpu) != 0) { GB_cpu_push_stack(device, device->cpu->registers.pc + 3); device->cpu->registers.pc = GB_cpu_fetch_word(device, 1); return 24; } PC_INC(self, 3); return 12; }
Byte ins_call_xx(GB_device* device) { GB_cpu_push_stack(device, device->cpu->registers.pc + 3); device->cpu->registers.pc = GB_cpu_fetch_word(device, 1); return 24; } 

Byte ins_rst_00(GB_device* device) { GB_cpu_push_stack(device, device->cpu->registers.pc + 1); device->cpu->registers.pc = 0x00; return 16; }
Byte ins_rst_08(GB_device* device) { GB_cpu_push_stack(device, device->cpu->registers.pc + 1); device->cpu->registers.pc = 0x08; return 16; }
Byte ins_rst_10(GB_device* device) { GB_cpu_push_stack(device, device->cpu->registers.pc + 1); device->cpu->registers.pc = 0x10; return 16; }
Byte ins_rst_18(GB_device* device) { GB_cpu_push_stack(device, device->cpu->registers.pc + 1); device->cpu->registers.pc = 0x18; return 16; }
Byte ins_rst_20(GB_device* device) { GB_cpu_push_stack(device, device->cpu->registers.pc + 1); device->cpu->registers.pc = 0x20; return 16; }
Byte ins_rst_28(GB_device* device) { GB_cpu_push_stack(device, device->cpu->registers.pc + 1); device->cpu->registers.pc = 0x28; return 16; }
Byte ins_rst_30(GB_device* device) { GB_cpu_push_stack(device, device->cpu->registers.pc + 1); device->cpu->registers.pc = 0x30; return 16; }
Byte ins_rst_38(GB_device* device) { GB_cpu_push_stack(device, device->cpu->registers.pc + 1); device->cpu->registers.pc = 0x38; return 16; }

Byte ins_halt(GB_device* device) { 
    device->cpu->is_halted = true; 
    PC_INC(self, 1); 
    return 4; 
}

Byte ins_bit_a_0(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.a & 0x01) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_b_0(GB_device* device) { 
    device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.b & 0x01) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);
    PC_INC(self, 2); 
    return 8; 
}
Byte ins_bit_c_0(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.c & 0x01) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_d_0(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.d & 0x01) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_e_0(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.e & 0x01) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_h_0(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.h & 0x01) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_l_0(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.l & 0x01) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_hl_0(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(GB_deviceReadByte(device, GB_register_get_HL(device->cpu)) & 0x01) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 16; }

Byte ins_bit_a_1(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.a & 0x02) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_b_1(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.b & 0x02) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_c_1(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.c & 0x02) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_d_1(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.d & 0x02) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_e_1(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.e & 0x02) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_h_1(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.h & 0x02) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_l_1(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.l & 0x02) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_hl_1(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(GB_deviceReadByte(device, GB_register_get_HL(device->cpu)) & 0x02) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 16; }

Byte ins_bit_a_2(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.a & 0x04) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_b_2(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.b & 0x04) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_c_2(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.c & 0x04) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_d_2(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.d & 0x04) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_e_2(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.e & 0x04) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_h_2(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.h & 0x04) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_l_2(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.l & 0x04) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_hl_2(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(GB_deviceReadByte(device, GB_register_get_HL(device->cpu)) & 0x04) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 16; }

Byte ins_bit_a_3(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.a & 0x08) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_b_3(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.b & 0x08) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_c_3(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.c & 0x08) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_d_3(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.d & 0x08) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_e_3(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.e & 0x08) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_h_3(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.h & 0x08) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_l_3(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.l & 0x08) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_hl_3(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(GB_deviceReadByte(device, GB_register_get_HL(device->cpu)) & 0x08) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 16; }

Byte ins_bit_a_4(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.a & 0x10) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_b_4(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.b & 0x10) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_c_4(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.c & 0x10) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_d_4(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.d & 0x10) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_e_4(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.e & 0x10) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_h_4(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.h & 0x10) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_l_4(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.l & 0x10) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_hl_4(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(GB_deviceReadByte(device, GB_register_get_HL(device->cpu)) & 0x10) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 16; }

Byte ins_bit_a_5(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.a & 0x20) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_b_5(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.b & 0x20) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_c_5(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.c & 0x20) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_d_5(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.d & 0x20) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_e_5(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.e & 0x20) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_h_5(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.h & 0x20) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_l_5(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.l & 0x20) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_hl_5(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(GB_deviceReadByte(device, GB_register_get_HL(device->cpu)) & 0x20) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 16; }

Byte ins_bit_a_6(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.a & 0x40) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_b_6(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.b & 0x40) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_c_6(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.c & 0x40) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_d_6(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.d & 0x40) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_e_6(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.e & 0x40) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_h_6(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.h & 0x40) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_l_6(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.l & 0x40) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_hl_6(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(GB_deviceReadByte(device, GB_register_get_HL(device->cpu)) & 0x40) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 16; }

Byte ins_bit_a_7(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.a & 0x80) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_b_7(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.b & 0x80) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_c_7(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.c & 0x80) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_d_7(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.d & 0x80) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_e_7(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.e & 0x80) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_h_7(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.h & 0x80) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_l_7(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(device->cpu->registers.l & 0x80) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 8; }
Byte ins_bit_hl_7(GB_device* device) { device->cpu->registers.f = ZeroFlagValue(GB_deviceReadByte(device, GB_register_get_HL(device->cpu))  & 0x80) | FLAG_HALF | GB_cpu_get_carry_flag(device->cpu);  PC_INC(self, 2); return 16; }

Byte ins_res_a_0(GB_device* device) { device->cpu->registers.a = device->cpu->registers.a & 0xfe; PC_INC(self, 2); return 8; }
Byte ins_res_b_0(GB_device* device) { device->cpu->registers.b = device->cpu->registers.b & 0xfe; PC_INC(self, 2); return 8; }
Byte ins_res_c_0(GB_device* device) { device->cpu->registers.c = device->cpu->registers.c & 0xfe; PC_INC(self, 2); return 8; }
Byte ins_res_d_0(GB_device* device) { device->cpu->registers.d = device->cpu->registers.d & 0xfe; PC_INC(self, 2); return 8; }
Byte ins_res_e_0(GB_device* device) { device->cpu->registers.e = device->cpu->registers.e & 0xfe; PC_INC(self, 2); return 8; }
Byte ins_res_h_0(GB_device* device) { device->cpu->registers.h = device->cpu->registers.h & 0xfe; PC_INC(self, 2); return 8; }
Byte ins_res_l_0(GB_device* device) { device->cpu->registers.l = device->cpu->registers.l & 0xfe; PC_INC(self, 2); return 8; }
Byte ins_res_hl_0(GB_device* device) { GB_deviceWriteByte(device,GB_register_get_HL(device->cpu), GB_deviceReadByte(device, GB_register_get_HL(device->cpu)) & 0xfe); PC_INC(self, 2); return 16; }

Byte ins_res_a_1(GB_device* device) { device->cpu->registers.a = device->cpu->registers.a & 0xfd; PC_INC(self, 2); return 8; }
Byte ins_res_b_1(GB_device* device) { device->cpu->registers.b = device->cpu->registers.b & 0xfd; PC_INC(self, 2); return 8; }
Byte ins_res_c_1(GB_device* device) { device->cpu->registers.c = device->cpu->registers.c & 0xfd; PC_INC(self, 2); return 8; }
Byte ins_res_d_1(GB_device* device) { device->cpu->registers.d = device->cpu->registers.d & 0xfd; PC_INC(self, 2); return 8; }
Byte ins_res_e_1(GB_device* device) { device->cpu->registers.e = device->cpu->registers.e & 0xfd; PC_INC(self, 2); return 8; }
Byte ins_res_h_1(GB_device* device) { device->cpu->registers.h = device->cpu->registers.h & 0xfd; PC_INC(self, 2); return 8; }
Byte ins_res_l_1(GB_device* device) { device->cpu->registers.l = device->cpu->registers.l & 0xfd; PC_INC(self, 2); return 8; }
Byte ins_res_hl_1(GB_device* device) { GB_deviceWriteByte(device,GB_register_get_HL(device->cpu), GB_deviceReadByte(device, GB_register_get_HL(device->cpu)) & 0xfd); PC_INC(self, 2); return 16; }

Byte ins_res_a_2(GB_device* device) { device->cpu->registers.a = device->cpu->registers.a & 0xfb; PC_INC(self, 2); return 8; }
Byte ins_res_b_2(GB_device* device) { device->cpu->registers.b = device->cpu->registers.b & 0xfb; PC_INC(self, 2); return 8; }
Byte ins_res_c_2(GB_device* device) { device->cpu->registers.c = device->cpu->registers.c & 0xfb; PC_INC(self, 2); return 8; }
Byte ins_res_d_2(GB_device* device) { device->cpu->registers.d = device->cpu->registers.d & 0xfb; PC_INC(self, 2); return 8; }
Byte ins_res_e_2(GB_device* device) { device->cpu->registers.e = device->cpu->registers.e & 0xfb; PC_INC(self, 2); return 8; }
Byte ins_res_h_2(GB_device* device) { device->cpu->registers.h = device->cpu->registers.h & 0xfb; PC_INC(self, 2); return 8; }
Byte ins_res_l_2(GB_device* device) { device->cpu->registers.l = device->cpu->registers.l & 0xfb; PC_INC(self, 2); return 8; }
Byte ins_res_hl_2(GB_device* device) { GB_deviceWriteByte(device,GB_register_get_HL(device->cpu), GB_deviceReadByte(device, GB_register_get_HL(device->cpu)) & 0xfb); PC_INC(self, 2); return 16; }

Byte ins_res_a_3(GB_device* device) { device->cpu->registers.a = device->cpu->registers.a & 0xf7; PC_INC(self, 2); return 8; }
Byte ins_res_b_3(GB_device* device) { device->cpu->registers.b = device->cpu->registers.b & 0xf7; PC_INC(self, 2); return 8; }
Byte ins_res_c_3(GB_device* device) { device->cpu->registers.c = device->cpu->registers.c & 0xf7; PC_INC(self, 2); return 8; }
Byte ins_res_d_3(GB_device* device) { device->cpu->registers.d = device->cpu->registers.d & 0xf7; PC_INC(self, 2); return 8; }
Byte ins_res_e_3(GB_device* device) { device->cpu->registers.e = device->cpu->registers.e & 0xf7; PC_INC(self, 2); return 8; }
Byte ins_res_h_3(GB_device* device) { device->cpu->registers.h = device->cpu->registers.h & 0xf7; PC_INC(self, 2); return 8; }
Byte ins_res_l_3(GB_device* device) { device->cpu->registers.l = device->cpu->registers.l & 0xf7; PC_INC(self, 2); return 8; }
Byte ins_res_hl_3(GB_device* device) { GB_deviceWriteByte(device,GB_register_get_HL(device->cpu), GB_deviceReadByte(device, GB_register_get_HL(device->cpu)) & 0xf7); PC_INC(self, 2); return 16; }

Byte ins_res_a_4(GB_device* device) { device->cpu->registers.a = device->cpu->registers.a & 0xef; PC_INC(self, 2); return 8; }
Byte ins_res_b_4(GB_device* device) { device->cpu->registers.b = device->cpu->registers.b & 0xef; PC_INC(self, 2); return 8; }
Byte ins_res_c_4(GB_device* device) { device->cpu->registers.c = device->cpu->registers.c & 0xef; PC_INC(self, 2); return 8; }
Byte ins_res_d_4(GB_device* device) { device->cpu->registers.d = device->cpu->registers.d & 0xef; PC_INC(self, 2); return 8; }
Byte ins_res_e_4(GB_device* device) { device->cpu->registers.e = device->cpu->registers.e & 0xef; PC_INC(self, 2); return 8; }
Byte ins_res_h_4(GB_device* device) { device->cpu->registers.h = device->cpu->registers.h & 0xef; PC_INC(self, 2); return 8; }
Byte ins_res_l_4(GB_device* device) { device->cpu->registers.l = device->cpu->registers.l & 0xef; PC_INC(self, 2); return 8; }
Byte ins_res_hl_4(GB_device* device) { GB_deviceWriteByte(device,GB_register_get_HL(device->cpu), GB_deviceReadByte(device, GB_register_get_HL(device->cpu)) & 0xef); PC_INC(self, 2); return 16; }

Byte ins_res_a_5(GB_device* device) { device->cpu->registers.a = device->cpu->registers.a & 0xdf; PC_INC(self, 2); return 8; }
Byte ins_res_b_5(GB_device* device) { device->cpu->registers.b = device->cpu->registers.b & 0xdf; PC_INC(self, 2); return 8; }
Byte ins_res_c_5(GB_device* device) { device->cpu->registers.c = device->cpu->registers.c & 0xdf; PC_INC(self, 2); return 8; }
Byte ins_res_d_5(GB_device* device) { device->cpu->registers.d = device->cpu->registers.d & 0xdf; PC_INC(self, 2); return 8; }
Byte ins_res_e_5(GB_device* device) { device->cpu->registers.e = device->cpu->registers.e & 0xdf; PC_INC(self, 2); return 8; }
Byte ins_res_h_5(GB_device* device) { device->cpu->registers.h = device->cpu->registers.h & 0xdf; PC_INC(self, 2); return 8; }
Byte ins_res_l_5(GB_device* device) { device->cpu->registers.l = device->cpu->registers.l & 0xdf; PC_INC(self, 2); return 8; }
Byte ins_res_hl_5(GB_device* device) { GB_deviceWriteByte(device,GB_register_get_HL(device->cpu), GB_deviceReadByte(device, GB_register_get_HL(device->cpu)) & 0xdf); PC_INC(self, 2); return 16; }

Byte ins_res_a_6(GB_device* device) { device->cpu->registers.a = device->cpu->registers.a & 0xbf; PC_INC(self, 2); return 8; }
Byte ins_res_b_6(GB_device* device) { device->cpu->registers.b = device->cpu->registers.b & 0xbf; PC_INC(self, 2); return 8; }
Byte ins_res_c_6(GB_device* device) { device->cpu->registers.c = device->cpu->registers.c & 0xbf; PC_INC(self, 2); return 8; }
Byte ins_res_d_6(GB_device* device) { device->cpu->registers.d = device->cpu->registers.d & 0xbf; PC_INC(self, 2); return 8; }
Byte ins_res_e_6(GB_device* device) { device->cpu->registers.e = device->cpu->registers.e & 0xbf; PC_INC(self, 2); return 8; }
Byte ins_res_h_6(GB_device* device) { device->cpu->registers.h = device->cpu->registers.h & 0xbf; PC_INC(self, 2); return 8; }
Byte ins_res_l_6(GB_device* device) { device->cpu->registers.l = device->cpu->registers.l & 0xbf; PC_INC(self, 2); return 8; }
Byte ins_res_hl_6(GB_device* device) { GB_deviceWriteByte(device,GB_register_get_HL(device->cpu), GB_deviceReadByte(device, GB_register_get_HL(device->cpu)) & 0xbf); PC_INC(self, 2); return 16; }

Byte ins_res_a_7(GB_device* device) { device->cpu->registers.a = device->cpu->registers.a & 0x7f; PC_INC(self, 2); return 8; }
Byte ins_res_b_7(GB_device* device) { device->cpu->registers.b = device->cpu->registers.b & 0x7f; PC_INC(self, 2); return 8; }
Byte ins_res_c_7(GB_device* device) { device->cpu->registers.c = device->cpu->registers.c & 0x7f; PC_INC(self, 2); return 8; }
Byte ins_res_d_7(GB_device* device) { device->cpu->registers.d = device->cpu->registers.d & 0x7f; PC_INC(self, 2); return 8; }
Byte ins_res_e_7(GB_device* device) { device->cpu->registers.e = device->cpu->registers.e & 0x7f; PC_INC(self, 2); return 8; }
Byte ins_res_h_7(GB_device* device) { device->cpu->registers.h = device->cpu->registers.h & 0x7f; PC_INC(self, 2); return 8; }
Byte ins_res_l_7(GB_device* device) { device->cpu->registers.l = device->cpu->registers.l & 0x7f; PC_INC(self, 2); return 8; }
Byte ins_res_hl_7(GB_device* device) { GB_deviceWriteByte(device,GB_register_get_HL(device->cpu), GB_deviceReadByte(device, GB_register_get_HL(device->cpu)) & 0x7f); PC_INC(self, 2); return 16; }

Byte ins_set_a_0(GB_device* device) { device->cpu->registers.a = device->cpu->registers.a | 0x01; PC_INC(self, 2); return 8; }
Byte ins_set_b_0(GB_device* device) { device->cpu->registers.b = device->cpu->registers.b | 0x01; PC_INC(self, 2); return 8; }
Byte ins_set_c_0(GB_device* device) { device->cpu->registers.c = device->cpu->registers.c | 0x01; PC_INC(self, 2); return 8; }
Byte ins_set_d_0(GB_device* device) { device->cpu->registers.d = device->cpu->registers.d | 0x01; PC_INC(self, 2); return 8; }
Byte ins_set_e_0(GB_device* device) { device->cpu->registers.e = device->cpu->registers.e | 0x01; PC_INC(self, 2); return 8; }
Byte ins_set_h_0(GB_device* device) { device->cpu->registers.h = device->cpu->registers.h | 0x01; PC_INC(self, 2); return 8; }
Byte ins_set_l_0(GB_device* device) { device->cpu->registers.l = device->cpu->registers.l | 0x01; PC_INC(self, 2); return 8; }
Byte ins_set_hl_0(GB_device* device) { GB_deviceWriteByte(device,GB_register_get_HL(device->cpu), GB_deviceReadByte(device, GB_register_get_HL(device->cpu)) | 0x01); PC_INC(self, 2); return 8; }

Byte ins_set_a_1(GB_device* device) { device->cpu->registers.a = device->cpu->registers.a | 0x02; PC_INC(self, 2); return 8; }
Byte ins_set_b_1(GB_device* device) { device->cpu->registers.b = device->cpu->registers.b | 0x02; PC_INC(self, 2); return 8; }
Byte ins_set_c_1(GB_device* device) { device->cpu->registers.c = device->cpu->registers.c | 0x02; PC_INC(self, 2); return 8; }
Byte ins_set_d_1(GB_device* device) { device->cpu->registers.d = device->cpu->registers.d | 0x02; PC_INC(self, 2); return 8; }
Byte ins_set_e_1(GB_device* device) { device->cpu->registers.e = device->cpu->registers.e | 0x02; PC_INC(self, 2); return 8; }
Byte ins_set_h_1(GB_device* device) { device->cpu->registers.h = device->cpu->registers.h | 0x02; PC_INC(self, 2); return 8; }
Byte ins_set_l_1(GB_device* device) { device->cpu->registers.l = device->cpu->registers.l | 0x02; PC_INC(self, 2); return 8; }
Byte ins_set_hl_1(GB_device* device) { GB_deviceWriteByte(device,GB_register_get_HL(device->cpu), GB_deviceReadByte(device, GB_register_get_HL(device->cpu)) | 0x02); PC_INC(self, 2); return 8; }

Byte ins_set_a_2(GB_device* device) { device->cpu->registers.a = device->cpu->registers.a | 0x04; PC_INC(self, 2); return 8; }
Byte ins_set_b_2(GB_device* device) { device->cpu->registers.b = device->cpu->registers.b | 0x04; PC_INC(self, 2); return 8; }
Byte ins_set_c_2(GB_device* device) { device->cpu->registers.c = device->cpu->registers.c | 0x04; PC_INC(self, 2); return 8; }
Byte ins_set_d_2(GB_device* device) { device->cpu->registers.d = device->cpu->registers.d | 0x04; PC_INC(self, 2); return 8; }
Byte ins_set_e_2(GB_device* device) { device->cpu->registers.e = device->cpu->registers.e | 0x04; PC_INC(self, 2); return 8; }
Byte ins_set_h_2(GB_device* device) { device->cpu->registers.h = device->cpu->registers.h | 0x04; PC_INC(self, 2); return 8; }
Byte ins_set_l_2(GB_device* device) { device->cpu->registers.l = device->cpu->registers.l | 0x04; PC_INC(self, 2); return 8; }
Byte ins_set_hl_2(GB_device* device) { GB_deviceWriteByte(device,GB_register_get_HL(device->cpu), GB_deviceReadByte(device, GB_register_get_HL(device->cpu)) | 0x04); PC_INC(self, 2); return 8; }

Byte ins_set_a_3(GB_device* device) { device->cpu->registers.a = device->cpu->registers.a | 0x08; PC_INC(self, 2); return 8; }
Byte ins_set_b_3(GB_device* device) { device->cpu->registers.b = device->cpu->registers.b | 0x08; PC_INC(self, 2); return 8; }
Byte ins_set_c_3(GB_device* device) { device->cpu->registers.c = device->cpu->registers.c | 0x08; PC_INC(self, 2); return 8; }
Byte ins_set_d_3(GB_device* device) { device->cpu->registers.d = device->cpu->registers.d | 0x08; PC_INC(self, 2); return 8; }
Byte ins_set_e_3(GB_device* device) { device->cpu->registers.e = device->cpu->registers.e | 0x08; PC_INC(self, 2); return 8; }
Byte ins_set_h_3(GB_device* device) { device->cpu->registers.h = device->cpu->registers.h | 0x08; PC_INC(self, 2); return 8; }
Byte ins_set_l_3(GB_device* device) { device->cpu->registers.l = device->cpu->registers.l | 0x08; PC_INC(self, 2); return 8; }
Byte ins_set_hl_3(GB_device* device) { GB_deviceWriteByte(device,GB_register_get_HL(device->cpu), GB_deviceReadByte(device, GB_register_get_HL(device->cpu)) | 0x08); PC_INC(self, 2); return 8; }

Byte ins_set_a_4(GB_device* device) { device->cpu->registers.a = device->cpu->registers.a | 0x10; PC_INC(self, 2); return 8; }
Byte ins_set_b_4(GB_device* device) { device->cpu->registers.b = device->cpu->registers.b | 0x10; PC_INC(self, 2); return 8; }
Byte ins_set_c_4(GB_device* device) { device->cpu->registers.c = device->cpu->registers.c | 0x10; PC_INC(self, 2); return 8; }
Byte ins_set_d_4(GB_device* device) { device->cpu->registers.d = device->cpu->registers.d | 0x10; PC_INC(self, 2); return 8; }
Byte ins_set_e_4(GB_device* device) { device->cpu->registers.e = device->cpu->registers.e | 0x10; PC_INC(self, 2); return 8; }
Byte ins_set_h_4(GB_device* device) { device->cpu->registers.h = device->cpu->registers.h | 0x10; PC_INC(self, 2); return 8; }
Byte ins_set_l_4(GB_device* device) { device->cpu->registers.l = device->cpu->registers.l | 0x10; PC_INC(self, 2); return 8; }
Byte ins_set_hl_4(GB_device* device) { GB_deviceWriteByte(device,GB_register_get_HL(device->cpu), GB_deviceReadByte(device, GB_register_get_HL(device->cpu)) | 0x10); PC_INC(self, 2); return 8; }

Byte ins_set_a_5(GB_device* device) { device->cpu->registers.a = device->cpu->registers.a | 0x20; PC_INC(self, 2); return 8; }
Byte ins_set_b_5(GB_device* device) { device->cpu->registers.b = device->cpu->registers.b | 0x20; PC_INC(self, 2); return 8; }
Byte ins_set_c_5(GB_device* device) { device->cpu->registers.c = device->cpu->registers.c | 0x20; PC_INC(self, 2); return 8; }
Byte ins_set_d_5(GB_device* device) { device->cpu->registers.d = device->cpu->registers.d | 0x20; PC_INC(self, 2); return 8; }
Byte ins_set_e_5(GB_device* device) { device->cpu->registers.e = device->cpu->registers.e | 0x20; PC_INC(self, 2); return 8; }
Byte ins_set_h_5(GB_device* device) { device->cpu->registers.h = device->cpu->registers.h | 0x20; PC_INC(self, 2); return 8; }
Byte ins_set_l_5(GB_device* device) { device->cpu->registers.l = device->cpu->registers.l | 0x20; PC_INC(self, 2); return 8; }
Byte ins_set_hl_5(GB_device* device) { GB_deviceWriteByte(device,GB_register_get_HL(device->cpu), GB_deviceReadByte(device, GB_register_get_HL(device->cpu)) | 0x20); PC_INC(self, 2); return 8; }

Byte ins_set_a_6(GB_device* device) { device->cpu->registers.a = device->cpu->registers.a | 0x40; PC_INC(self, 2); return 8; }
Byte ins_set_b_6(GB_device* device) { device->cpu->registers.b = device->cpu->registers.b | 0x40; PC_INC(self, 2); return 8; }
Byte ins_set_c_6(GB_device* device) { device->cpu->registers.c = device->cpu->registers.c | 0x40; PC_INC(self, 2); return 8; }
Byte ins_set_d_6(GB_device* device) { device->cpu->registers.d = device->cpu->registers.d | 0x40; PC_INC(self, 2); return 8; }
Byte ins_set_e_6(GB_device* device) { device->cpu->registers.e = device->cpu->registers.e | 0x40; PC_INC(self, 2); return 8; }
Byte ins_set_h_6(GB_device* device) { device->cpu->registers.h = device->cpu->registers.h | 0x40; PC_INC(self, 2); return 8; }
Byte ins_set_l_6(GB_device* device) { device->cpu->registers.l = device->cpu->registers.l | 0x40; PC_INC(self, 2); return 8; }
Byte ins_set_hl_6(GB_device* device) { GB_deviceWriteByte(device,GB_register_get_HL(device->cpu), GB_deviceReadByte(device, GB_register_get_HL(device->cpu)) | 0x40); PC_INC(self, 2); return 8; }

Byte ins_set_a_7(GB_device* device) { device->cpu->registers.a = device->cpu->registers.a | 0x80; PC_INC(self, 2); return 8; }
Byte ins_set_b_7(GB_device* device) { device->cpu->registers.b = device->cpu->registers.b | 0x80; PC_INC(self, 2); return 8; }
Byte ins_set_c_7(GB_device* device) { device->cpu->registers.c = device->cpu->registers.c | 0x80; PC_INC(self, 2); return 8; }
Byte ins_set_d_7(GB_device* device) { device->cpu->registers.d = device->cpu->registers.d | 0x80; PC_INC(self, 2); return 8; }
Byte ins_set_e_7(GB_device* device) { device->cpu->registers.e = device->cpu->registers.e | 0x80; PC_INC(self, 2); return 8; }
Byte ins_set_h_7(GB_device* device) { device->cpu->registers.h = device->cpu->registers.h | 0x80; PC_INC(self, 2); return 8; }
Byte ins_set_l_7(GB_device* device) { device->cpu->registers.l = device->cpu->registers.l | 0x80; PC_INC(self, 2); return 8; }
Byte ins_set_hl_7(GB_device* device) { GB_deviceWriteByte(device,GB_register_get_HL(device->cpu), GB_deviceReadByte(device, GB_register_get_HL(device->cpu)) | 0x80); PC_INC(self, 2); return 8; }

typedef Byte (*ins_func)(GB_device*);

ins_func ins_CB_table[256] = {
    /*	     0               1	         2	          3		         4    	     5	           6	        7	          8          9            A              B             C           D             E           F      */
    /* 0 */ ins_rlc_b  , ins_rlc_c  , ins_rlc_d  , ins_rlc_e  , ins_rlc_h  , ins_rlc_l  , ins_rlc_hl  , ins_rlc_a  , ins_rrc_b  , ins_rrc_c  , ins_rrc_d  , ins_rrc_e  , ins_rrc_h , ins_rrc_l   , ins_rrc_hl  , ins_rrc_a  ,
    /* 1 */ ins_rl_b   , ins_rl_c   , ins_rl_d   , ins_rl_e   , ins_rl_h   , ins_rl_l   , ins_rl_hl   , ins_rl_a   , ins_rr_b   , ins_rr_c   , ins_rr_d   , ins_rr_e   , ins_rr_h  , ins_rr_l    , ins_rr_hl   , ins_rr_a   ,
    /* 2 */ ins_sla_b  , ins_sla_c  , ins_sla_d  , ins_sla_e  , ins_sla_h  , ins_sla_l  , ins_sla_hl  , ins_sla_a  , ins_sra_b  , ins_sra_c  , ins_sra_d  , ins_sra_e  , ins_sra_h , ins_sra_l   , ins_sra_hl  , ins_sra_a  ,
    /* 3 */ ins_swap_b , ins_swap_c , ins_swap_d , ins_swap_e , ins_swap_h , ins_swap_l , ins_swap_hl , ins_swap_a , ins_srl_b  , ins_srl_c  , ins_srl_d  , ins_srl_e  , ins_srl_h , ins_srl_l   , ins_srl_hl  , ins_srl_a  ,
    /* 4 */ ins_bit_b_0, ins_bit_c_0, ins_bit_d_0, ins_bit_e_0, ins_bit_h_0, ins_bit_l_0, ins_bit_hl_0, ins_bit_a_0, ins_bit_b_1, ins_bit_c_1, ins_bit_d_1, ins_bit_e_1, ins_bit_h_1, ins_bit_l_1, ins_bit_hl_1, ins_bit_a_1,
    /* 5 */ ins_bit_b_2, ins_bit_c_2, ins_bit_d_2, ins_bit_e_2, ins_bit_h_2, ins_bit_l_2, ins_bit_hl_2, ins_bit_a_2, ins_bit_b_3, ins_bit_c_3, ins_bit_d_3, ins_bit_e_3, ins_bit_h_3, ins_bit_l_3, ins_bit_hl_3, ins_bit_a_3,
    /* 6 */ ins_bit_b_4, ins_bit_c_4, ins_bit_d_4, ins_bit_e_4, ins_bit_h_4, ins_bit_l_4, ins_bit_hl_4, ins_bit_a_4, ins_bit_b_5, ins_bit_c_5, ins_bit_d_5, ins_bit_e_5, ins_bit_h_5, ins_bit_l_5, ins_bit_hl_5, ins_bit_a_5,
    /* 7 */ ins_bit_b_6, ins_bit_c_6, ins_bit_d_6, ins_bit_e_6, ins_bit_h_6, ins_bit_l_6, ins_bit_hl_6, ins_bit_a_6, ins_bit_b_7, ins_bit_c_7, ins_bit_d_7, ins_bit_e_7, ins_bit_h_7, ins_bit_l_7, ins_bit_hl_7, ins_bit_a_7,
    /* 8 */ ins_res_b_0, ins_res_c_0, ins_res_d_0, ins_res_e_0, ins_res_h_0, ins_res_l_0, ins_res_hl_0, ins_res_a_0, ins_res_b_1, ins_res_c_1, ins_res_d_1, ins_res_e_1, ins_res_h_1, ins_res_l_1, ins_res_hl_1, ins_res_a_1, 
    /* 9 */ ins_res_b_2, ins_res_c_2, ins_res_d_2, ins_res_e_2, ins_res_h_2, ins_res_l_2, ins_res_hl_2, ins_res_a_2, ins_res_b_3, ins_res_c_3, ins_res_d_3, ins_res_e_3, ins_res_h_3, ins_res_l_3, ins_res_hl_3, ins_res_a_3,
    /* A */ ins_res_b_4, ins_res_c_4, ins_res_d_4, ins_res_e_4, ins_res_h_4, ins_res_l_4, ins_res_hl_4, ins_res_a_4, ins_res_b_5, ins_res_c_5, ins_res_d_5, ins_res_e_5, ins_res_h_5, ins_res_l_5, ins_res_hl_5, ins_res_a_5,
    /* B */ ins_res_b_6, ins_res_c_6, ins_res_d_6, ins_res_e_6, ins_res_h_6, ins_res_l_6, ins_res_hl_6, ins_res_a_6, ins_res_b_7, ins_res_c_7, ins_res_d_7, ins_res_e_7, ins_res_h_7, ins_res_l_7, ins_res_hl_7, ins_res_a_7,
    /* C */ ins_set_b_0, ins_set_c_0, ins_set_d_0, ins_set_e_0, ins_set_h_0, ins_set_l_0, ins_set_hl_0, ins_set_a_0, ins_set_b_1, ins_set_c_1, ins_set_d_1, ins_set_e_1, ins_set_h_1, ins_set_l_1, ins_set_hl_1, ins_set_a_1,
    /* D */ ins_set_b_2, ins_set_c_2, ins_set_d_2, ins_set_e_2, ins_set_h_2, ins_set_l_2, ins_set_hl_2, ins_set_a_2, ins_set_b_3, ins_set_c_3, ins_set_d_3, ins_set_e_3, ins_set_h_3, ins_set_l_3, ins_set_hl_3, ins_set_a_3,
    /* E */ ins_set_b_4, ins_set_c_4, ins_set_d_4, ins_set_e_4, ins_set_h_4, ins_set_l_4, ins_set_hl_4, ins_set_a_4, ins_set_b_5, ins_set_c_5, ins_set_d_5, ins_set_e_5, ins_set_h_5, ins_set_l_5, ins_set_hl_5, ins_set_a_5,
    /* F */ ins_set_b_6, ins_set_c_6, ins_set_d_6, ins_set_e_6, ins_set_h_6, ins_set_l_6, ins_set_hl_6, ins_set_a_6, ins_set_b_7, ins_set_c_7, ins_set_d_7, ins_set_e_7, ins_set_h_7, ins_set_l_7, ins_set_hl_7, ins_set_a_7
};

Byte ins_prefixCB(GB_device* device) { 
    Byte insCode = GB_cpu_fetch_byte(device, 1);
    //printf("executing instruction CB => 0x%02x; PC=0x%04x \n", insCode, device->cpu->registers.pc);
    Byte ticks = ins_CB_table[insCode](device); 
    return 4 + ticks; 
}

ins_func ins_table[256] = {
    /*	     0	                   1	         2	              3		    4    		      5	           6	        7	           8              9                 A              B           C                 D            E           F      */
    /* 0 */ ins_nop       , ins_ld_bc_xx, ins_ld_bc_a    , ins_inc_bc , ins_inc_b     , ins_dec_b     , ins_ld_b_x  , ins_rlca   ,  ins_ld_xx_sp, ins_add_hl_bc, ins_ld_a_bc    , ins_dec_bc  , ins_inc_c    , ins_dec_c  , ins_ld_c_x  , ins_rrca   ,
    /* 1 */ ins_stop      , ins_ld_de_xx, ins_ld_de_a    , ins_inc_de , ins_inc_d     , ins_dec_d     , ins_ld_d_x  , ins_rla    ,  ins_jr_x    , ins_add_hl_de, ins_ld_a_de    , ins_dec_de  , ins_inc_e    , ins_dec_e  , ins_ld_e_x  , ins_rra    ,
    /* 2 */ ins_jr_nz_x   , ins_ld_hl_xx, ins_ld_inc_hl_a, ins_inc_hl , ins_inc_h     , ins_dec_h     , ins_ld_h_x  , ins_daa    , ins_jr_z_x   , ins_add_hl_hl, ins_ld_a_hl_inc, ins_dec_hl  , ins_inc_l    , ins_dec_l  , ins_ld_l_x  , ins_cpl    ,
    /* 3 */ ins_jr_nc_x   , ins_ld_sp_xx, ins_ld_dec_hl_a, ins_inc_sp , ins_inc_hl_ptr, ins_dec_hl_ptr, ins_ld_hl_x , ins_scf    , ins_jr_c_x   , ins_add_hl_sp, ins_ld_a_hl_dec, ins_dec_sp  , ins_inc_a    , ins_dec_a  , ins_ld_a_x  , ins_ccf    ,
    /* 4 */ ins_nop       , ins_ld_b_c  , ins_ld_b_d     , ins_ld_b_e , ins_ld_b_h    , ins_ld_b_l    , ins_ld_b_hl , ins_ld_b_a , ins_ld_c_b   , ins_nop      , ins_ld_c_d     , ins_ld_c_e  , ins_ld_c_h   , ins_ld_c_l , ins_ld_c_hl , ins_ld_c_a ,
    /* 5 */ ins_ld_d_b    , ins_ld_d_c  , ins_nop        , ins_ld_d_e , ins_ld_d_h    , ins_ld_d_l    , ins_ld_d_hl , ins_ld_d_a , ins_ld_e_b   , ins_ld_e_c   , ins_ld_e_d     , ins_nop     , ins_ld_e_h   , ins_ld_e_l , ins_ld_e_hl , ins_ld_e_a ,
    /* 6 */ ins_ld_h_b    , ins_ld_h_c  , ins_ld_h_d     , ins_ld_h_e , ins_nop       , ins_ld_h_l    , ins_ld_h_hl , ins_ld_h_a , ins_ld_l_b   , ins_ld_l_c   , ins_ld_l_d     , ins_ld_l_e  , ins_ld_l_h   , ins_nop    , ins_ld_l_hl , ins_ld_l_a ,
    /* 7 */ ins_ld_hl_b   , ins_ld_hl_c , ins_ld_hl_d    , ins_ld_hl_e, ins_ld_hl_h   , ins_ld_hl_l   , ins_halt    , ins_ld_hl_a, ins_ld_a_b   , ins_ld_a_c   , ins_ld_a_d     , ins_ld_a_e  , ins_ld_a_h   , ins_ld_a_l , ins_ld_a_hl , ins_nop    ,
    /* 8 */ ins_add_a_b   , ins_add_a_c , ins_add_a_d    , ins_add_a_e, ins_add_a_h   , ins_add_a_l   , ins_add_a_hl, ins_add_a_a, ins_adc_a_b  , ins_adc_a_c  , ins_adc_a_d    , ins_adc_a_e , ins_adc_a_h  , ins_adc_a_l, ins_adc_a_hl, ins_adc_a_a,
    /* 9 */ ins_sub_a_b   , ins_sub_a_c , ins_sub_a_d    , ins_sub_a_e, ins_sub_a_h   , ins_sub_a_l   , ins_sub_a_hl, ins_sub_a_a, ins_sdc_a_b  , ins_sdc_a_c  , ins_sdc_a_d    , ins_sdc_a_e , ins_sdc_a_h  , ins_sdc_a_l, ins_sdc_a_hl, ins_sdc_a_a,
    /* A */ ins_and_a_b   , ins_and_a_c , ins_and_a_d    , ins_and_a_e, ins_and_a_h   , ins_and_a_l   , ins_and_a_hl, ins_and_a_a, ins_xor_a_b  , ins_xor_a_c  , ins_xor_a_d    , ins_xor_a_e , ins_xor_a_h  , ins_xor_a_l, ins_xor_a_hl, ins_xor_a_a,
    /* B */ ins_or_a_b    , ins_or_a_c  , ins_or_a_d     , ins_or_a_e , ins_or_a_h    , ins_or_a_l    , ins_or_a_hl , ins_or_a_a , ins_cp_a_b   , ins_cp_a_c   , ins_cp_a_d     , ins_cp_a_e  , ins_cp_a_h   , ins_cp_a_l , ins_cp_a_hl , ins_cp_a_a ,
    /* C */ ins_ret_nz    , ins_pop_bc  , ins_jp_nz_xx   , ins_jp_xx  , ins_call_nz_xx, ins_push_bc   , ins_add_a_x , ins_rst_00 , ins_ret_z    , ins_ret      , ins_jp_z_xx    , ins_prefixCB, ins_call_z_xx, ins_call_xx, ins_adc_a_x , ins_rst_08 ,
    /* D */ ins_ret_nc    , ins_pop_de  , ins_jp_nc_xx   , ins_bad_ins, ins_call_nc_xx, ins_push_de   , ins_sub_a_x , ins_rst_10 , ins_ret_c    , ins_reti     , ins_jp_c_xx    , ins_bad_ins , ins_call_c_xx, ins_bad_ins, ins_sdc_a_x , ins_rst_18 ,
    /* E */ ins_ld_ff00x_a, ins_pop_hl  , ins_ld_ff00c_a , ins_bad_ins, ins_bad_ins   , ins_push_hl   , ins_and_a_x , ins_rst_20 , ins_add_sp_x , ins_jp_hl    , ins_ld_xx_a    , ins_bad_ins , ins_bad_ins  , ins_bad_ins, ins_xor_a_x , ins_rst_28 ,
    /* F */ ins_ld_a_ff00x, ins_pop_af  , ins_ld_a_ff00c , ins_di     , ins_bad_ins   , ins_push_af   , ins_or_a_x  , ins_rst_30 , ins_ld_hl_spx, ins_ld_sp_hl , ins_ld_a_xx    , ins_ei      , ins_bad_ins  , ins_bad_ins, ins_cp_a_x  , ins_rst_38
};

unsigned int GB_TIMA_clock_inc_value(GBTimaClockCycles cycles) {
    switch (cycles) {
        case GBTimaClockCycles4:
            return 4;
        case GBTimaClockCycles16:
            return 16;
        case GBTimaClockCycles64:
            return 64;
        case GBTimaClockCycles256:
            return 256;
        default:
            return 4;
    }
}

void _GB_handle_interrupt(GB_device* device) {
    if(device->cpu->IME == false && device->cpu->is_halted == false) {
        return;
    }

    Byte interrupt = device->mmu->interruptRequest & device->mmu->interruptEnable & 0x1F;
    if(interrupt == 0) {
        // nothing to do
        return;
    } else if (true == device->cpu->is_halted) {
        // restart CPU
        device->cpu->is_halted = false;
        return;
    }

    device->cpu->IME = false;
    if((interrupt & GB_INTERRUPT_FLAG_VBLANK) == GB_INTERRUPT_FLAG_VBLANK) {
        device->mmu->interruptRequest = device->mmu->interruptRequest & ~(GB_INTERRUPT_FLAG_VBLANK);
        GB_cpu_push_stack(device, device->cpu->registers.pc);
        device->cpu->registers.pc = GB_PC_VBLANK_IR;
    } else if ((interrupt & GB_INTERRUPT_FLAG_LCD_STAT) == GB_INTERRUPT_FLAG_LCD_STAT) {
        device->mmu->interruptRequest = device->mmu->interruptRequest & ~(GB_INTERRUPT_FLAG_LCD_STAT);
        GB_cpu_push_stack(device, device->cpu->registers.pc);
        device->cpu->registers.pc = GB_PC_STAT_IR;
    } else if ((interrupt & GB_INTERRUPT_FLAG_TIMER) == GB_INTERRUPT_FLAG_TIMER) {
        device->mmu->interruptRequest = device->mmu->interruptRequest & ~(GB_INTERRUPT_FLAG_TIMER);
        GB_cpu_push_stack(device, device->cpu->registers.pc);
        device->cpu->registers.pc = GB_PC_TIMER_IR;
    } else if ((interrupt & GB_INTERRUPT_FLAG_SERIAL) == GB_INTERRUPT_FLAG_SERIAL) {
        device->mmu->interruptRequest = device->mmu->interruptRequest & ~(GB_INTERRUPT_FLAG_SERIAL);
        GB_cpu_push_stack(device, device->cpu->registers.pc);
        device->cpu->registers.pc = GB_PC_SERIAL_IR;
    } else if ((interrupt & GB_INTERRUPT_FLAG_JOYPAD) == GB_INTERRUPT_FLAG_JOYPAD) {
        device->mmu->interruptRequest = device->mmu->interruptRequest & ~(GB_INTERRUPT_FLAG_JOYPAD);
        GB_cpu_push_stack(device, device->cpu->registers.pc);
        device->cpu->registers.pc = GB_PC_JOYPAD_IR;
    }
}

Byte GB_deviceCpuStep(GB_device* device) {
    GB_cpu* cpu = device->cpu;
    GB_mmu* mmu = device->mmu;

    Byte ticks = cpu->prevCycles / 4;
    // update DIV register
    cpu->divCounter += ticks;
    if(cpu->divCounter >= DIV_CLOCK_INC) { // TODO: Handle double speed
        cpu->divCounter = 0;
        mmu->div++;
    }
    // Update TIMER counter if enabled
    if (mmu->isTimaEnabled == true) {
        uint8_t maxCycles = GB_TIMA_clock_inc_value(mmu->timaClockCycles);
        for (int i = 0; i < ticks; i++) {
            cpu->timaCounter++;
            if (cpu->timaCounter >= maxCycles) {
                mmu->tima++;
                if (mmu->tima == 0) {
                    // overflow
                    mmu->tima = mmu->tma;
                    GB_interrupt_request(device, GB_INTERRUPT_FLAG_TIMER);
                    mmu->interruptRequest = mmu->interruptRequest;
                }
            }
        }
        cpu->timaCounter %= maxCycles;
    }

    _GB_handle_interrupt(device);
    if(cpu->is_halted == true) {
        // if the cpu is halted no operation can be performed exept interups
        return 0;
    }

    Byte ins_code = GB_cpu_fetch_byte(device, 0);
    ins_func insToExec = ins_table[ins_code];
    //printf("executing instruction 0x%02x; PC=0x%04x \n", ins_code, cpu->registers.pc);

    cpu->prevCycles = (*insToExec)(device);

    if(cpu->registers.pc == GB_PC_START) {
        // trying to execute rom code so leave bios mode.
        mmu->in_bios = false;
    }

    if (cpu->enableINT != 0 && --cpu->enableINT == 0) {
        cpu->IME = true;
    }

    if (cpu->disableINT != 0 && --cpu->disableINT == 0) {
        cpu->IME = false;
    }

    return cpu->prevCycles;
}
