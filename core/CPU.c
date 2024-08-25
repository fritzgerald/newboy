#include "CPU.h"
#include "MMU.h"
#include "definitions.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

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
Byte GB_cpu_fetch_byte(GB_cpu *cpu, Word delta) {
    return GB_mmu_read_byte(&cpu->memory, cpu->registers.pc + delta);
}

Word GB_cpu_fetch_word(GB_cpu *cpu, Word delta) {
    return GB_mmu_read_word(&cpu->memory, cpu->registers.pc + delta);
}

Word GB_cpu_pop_stack(GB_cpu *cpu) {
    Word x1 = GB_mmu_read_word(&cpu->memory, cpu->registers.sp);
    cpu->registers.sp += 2;
    return x1;
}

void GB_cpu_push_stack(GB_cpu *cpu, Word data) {
    cpu->registers.sp -= 2;
    GB_mmu_write_word(&cpu->memory, cpu->registers.sp, data);
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

void GB_cpu_reset(GB_cpu* cpu) {
    GB_register_set_AF(cpu, 0);
    GB_register_set_BC(cpu, 0);
    GB_register_set_DE(cpu, 0);
    GB_register_set_BC(cpu, 0);
    cpu->registers.sp = 0;
    cpu->registers.pc = 0;
    cpu->is_halted = false;
    cpu->IME = 0;
    cpu->divCounter = 0;
    cpu->timaCounter = 0;

    GB_mmu_reset(&cpu->memory);
}

#define PC_INC(self, val) self->registers.pc += val
#define ZeroFlagValue(xx)                  (((xx) == 0) ? FLAG_ZERO : 0)
#define HalfCarryFlagValue(x1, x2)         (((x1 & 0x0F) + (x2 & 0x0F) > 0x0F) ? FLAG_HALF : 0)
#define HalfCarryValueC(x1, x2, c)         (((x1 & 0x0F) + ((x2 & 0x0F) + c) > 0x0F) ? FLAG_HALF : 0)
#define HalfCarryFlagValueW(xx1, xx2)      ((((xx1)&0x0FFF) + ((xx2)&0x0FFF) > 0x0FFF) ? FLAG_HALF : 0)
#define HalfCarrySubFlagValue(n1, n2)      (((n1 & 0x0F) < (n2 & 0x0F)) ? FLAG_HALF : 0)
#define HalfCarrySubFlagValueC(n1, n2, c)  (((n1 & 0x0F) < ((n2 & 0x0F) + c)) ? FLAG_HALF : 0)
#define CarrySubFlagValueAdd(n1, n2)       ((n1 < n2) ? FLAG_CARRY : 0)
#define CarrySubFlagValueAddC(n1, n2, c)    (((n1 < n2) || (n1 < n2 + c))? FLAG_CARRY : 0)
#define CarryFlagValueAdd(xx, xx1, xx2)    (((xx < xx1) | (xx < xx2)) ? FLAG_CARRY : 0)

// MARK: CPU instructions
Byte ins_nop(GB_cpu *self) { self->registers.pc++; return 4; }
Byte ins_stop(GB_cpu *self) { 
    self->registers.pc+=2; 
    self->is_halted = 1; 
    return 4; 
}
Byte ins_bad_ins(GB_cpu *self) { 
    self->is_halted = true; 
    return 20; 
}
Byte ins_di(GB_cpu *self) { self->IME = false; self->registers.pc++; return 4; }
Byte ins_ei(GB_cpu *self) { self->IME = true; self->registers.pc++; return 4; }

Byte ins_scf(GB_cpu *self) { self->registers.f = GB_cpu_zero_flag(self) | FLAG_CARRY; self->registers.pc++; return 4; }
Byte ins_ccf(GB_cpu *self) { self->registers.f = GB_cpu_zero_flag(self) | ((GB_cpu_get_carry_flag(self) == 0) ? FLAG_CARRY : 0); self->registers.pc++; return 4; }

Byte ins_ld_bc_xx(GB_cpu *self) { GB_register_set_BC(self, GB_cpu_fetch_word(self, 1)); PC_INC(self, 3);  return 12; }
Byte ins_ld_de_xx(GB_cpu *self) { GB_register_set_DE(self, GB_cpu_fetch_word(self, 1)); PC_INC(self, 3);  return 12; }
Byte ins_ld_hl_xx(GB_cpu *self) { GB_register_set_HL(self, GB_cpu_fetch_word(self, 1)); PC_INC(self, 3);  return 12; }
Byte ins_ld_sp_xx(GB_cpu *self) { self->registers.sp = GB_cpu_fetch_word(self, 1); PC_INC(self, 3);  return 12; }
Byte ins_ld_sp_hl(GB_cpu *self) { self->registers.sp = GB_register_get_HL(self); PC_INC(self, 1); return 8; }

Byte ins_ld_hl_spx(GB_cpu *self) {
    int16_t offset = (int8_t) GB_cpu_fetch_byte(self, 1);
    GB_register_set_HL(self, self->registers.sp + offset);
    self->registers.f = 0;

    if ((self->registers.sp & 0xF) + (offset & 0xF) > 0xF) {
        self->registers.f |= FLAG_HALF;
    }

    if ((self->registers.sp & 0xFF)  + (offset & 0xFF) > 0xFF) {
        self->registers.f |= FLAG_CARRY;
    }
    PC_INC(self, 2);  
    return 12; 
}

Byte ins_ld_bc_a(GB_cpu *self) { GB_mmu_write_byte(&self->memory,GB_register_get_BC(self), self->registers.a); PC_INC(self, 1);  return 8; }
Byte ins_ld_de_a(GB_cpu *self) { GB_mmu_write_byte(&self->memory,GB_register_get_DE(self), self->registers.a); PC_INC(self, 1);  return 8; }
Byte ins_ld_hl_a(GB_cpu *self) { GB_mmu_write_byte(&self->memory,GB_register_get_HL(self), self->registers.a); PC_INC(self, 1);  return 8; }
Byte ins_ld_hl_b(GB_cpu *self) { GB_mmu_write_byte(&self->memory,GB_register_get_HL(self), self->registers.b); PC_INC(self, 1);  return 8; }
Byte ins_ld_hl_c(GB_cpu *self) { GB_mmu_write_byte(&self->memory,GB_register_get_HL(self), self->registers.c); PC_INC(self, 1);  return 8; }
Byte ins_ld_hl_d(GB_cpu *self) { GB_mmu_write_byte(&self->memory,GB_register_get_HL(self), self->registers.d); PC_INC(self, 1);  return 8; }
Byte ins_ld_hl_e(GB_cpu *self) { GB_mmu_write_byte(&self->memory,GB_register_get_HL(self), self->registers.e); PC_INC(self, 1);  return 8; }
Byte ins_ld_hl_h(GB_cpu *self) { GB_mmu_write_byte(&self->memory,GB_register_get_HL(self), self->registers.h); PC_INC(self, 1);  return 8; }
Byte ins_ld_hl_l(GB_cpu *self) { GB_mmu_write_byte(&self->memory,GB_register_get_HL(self), self->registers.l); PC_INC(self, 1);  return 8; }
Byte ins_ld_hl_x(GB_cpu *self) {
    GB_mmu_write_byte(&self->memory,GB_register_get_HL(self), GB_cpu_fetch_byte(self, 1));
    PC_INC(self, 2);  
    return 12; 
}

Byte ins_ld_inc_hl_a(GB_cpu *self) { Word hl = GB_register_get_HL(self); GB_mmu_write_byte(&self->memory,hl++, self->registers.a); GB_register_set_HL(self, hl); PC_INC(self, 1);  return 8; }

Byte ins_ld_dec_hl_a(GB_cpu *self) { Word hl = GB_register_get_HL(self); GB_mmu_write_byte(&self->memory,hl--, self->registers.a); GB_register_set_HL(self, hl); PC_INC(self, 1);  return 8; }
Byte ins_ld_a_hl_inc(GB_cpu *self) { Word hl = GB_register_get_HL(self); self->registers.a = GB_mmu_read_byte(&self->memory, hl++); GB_register_set_HL(self, hl); PC_INC(self, 1);  return 8; }
Byte ins_ld_a_hl_dec(GB_cpu *self) { Word hl = GB_register_get_HL(self); self->registers.a = GB_mmu_read_byte(&self->memory, hl--); GB_register_set_HL(self, hl); PC_INC(self, 1);  return 8; }

Byte ins_ld_a_x(GB_cpu *self) { self->registers.a = GB_cpu_fetch_byte(self, 1); PC_INC(self, 2); return 8; }
Byte ins_ld_b_x(GB_cpu *self) { self->registers.b = GB_cpu_fetch_byte(self, 1); PC_INC(self, 2); return 8; }
Byte ins_ld_c_x(GB_cpu *self) { self->registers.c = GB_cpu_fetch_byte(self, 1); PC_INC(self, 2); return 8; }
Byte ins_ld_d_x(GB_cpu *self) { self->registers.d = GB_cpu_fetch_byte(self, 1); PC_INC(self, 2); return 8; }
Byte ins_ld_e_x(GB_cpu *self) { self->registers.e = GB_cpu_fetch_byte(self, 1); PC_INC(self, 2); return 8; }
Byte ins_ld_h_x(GB_cpu *self) { self->registers.h = GB_cpu_fetch_byte(self, 1); PC_INC(self, 2); return 8; }
Byte ins_ld_l_x(GB_cpu *self) { self->registers.l = GB_cpu_fetch_byte(self, 1); PC_INC(self, 2); return 8; }
Byte ins_ld_a_xx(GB_cpu *self) { Word xx = GB_cpu_fetch_word(self, 1); self->registers.a = GB_mmu_read_byte(&self->memory, xx); PC_INC(self, 3); return 16; }

Byte ins_ld_a_b(GB_cpu *self) { self->registers.a = self->registers.b; PC_INC(self, 1); return 4; }
Byte ins_ld_a_c(GB_cpu *self) { self->registers.a = self->registers.c; PC_INC(self, 1); return 4; }
Byte ins_ld_a_d(GB_cpu *self) { self->registers.a = self->registers.d; PC_INC(self, 1); return 4; }
Byte ins_ld_a_e(GB_cpu *self) { self->registers.a = self->registers.e; PC_INC(self, 1); return 4; }
Byte ins_ld_a_h(GB_cpu *self) { self->registers.a = self->registers.h; PC_INC(self, 1); return 4; }
Byte ins_ld_a_l(GB_cpu *self) { self->registers.a = self->registers.l; PC_INC(self, 1); return 4; }
Byte ins_ld_a_hl(GB_cpu *self) {self->registers.a = GB_mmu_read_byte(&self->memory, GB_register_get_HL(self)); PC_INC(self, 1);  return 8; }

Byte ins_ld_b_a(GB_cpu *self) { self->registers.b = self->registers.a; PC_INC(self, 1); return 4; }
Byte ins_ld_b_c(GB_cpu *self) { self->registers.b = self->registers.c; PC_INC(self, 1); return 4; }
Byte ins_ld_b_d(GB_cpu *self) { self->registers.b = self->registers.d; PC_INC(self, 1); return 4; }
Byte ins_ld_b_e(GB_cpu *self) { self->registers.b = self->registers.e; PC_INC(self, 1); return 4; }
Byte ins_ld_b_h(GB_cpu *self) { self->registers.b = self->registers.h; PC_INC(self, 1); return 4; }
Byte ins_ld_b_l(GB_cpu *self) { self->registers.b = self->registers.l; PC_INC(self, 1); return 4; }
Byte ins_ld_b_hl(GB_cpu *self) {self->registers.b = GB_mmu_read_byte(&self->memory, GB_register_get_HL(self)); PC_INC(self, 1);  return 8; }

Byte ins_ld_c_a(GB_cpu *self) { self->registers.c = self->registers.a; PC_INC(self, 1); return 4; }
Byte ins_ld_c_b(GB_cpu *self) { self->registers.c = self->registers.b; PC_INC(self, 1); return 4; }
Byte ins_ld_c_d(GB_cpu *self) { self->registers.c = self->registers.d; PC_INC(self, 1); return 4; }
Byte ins_ld_c_e(GB_cpu *self) { self->registers.c = self->registers.e; PC_INC(self, 1); return 4; }
Byte ins_ld_c_h(GB_cpu *self) { self->registers.c = self->registers.h; PC_INC(self, 1); return 4; }
Byte ins_ld_c_l(GB_cpu *self) { self->registers.c = self->registers.l; PC_INC(self, 1); return 4; }
Byte ins_ld_c_hl(GB_cpu *self) {self->registers.c = GB_mmu_read_byte(&self->memory, GB_register_get_HL(self)); PC_INC(self, 1);  return 8; }

Byte ins_ld_d_a(GB_cpu *self) { self->registers.d = self->registers.a; PC_INC(self, 1); return 4; }
Byte ins_ld_d_b(GB_cpu *self) { self->registers.d = self->registers.b; PC_INC(self, 1); return 4; }
Byte ins_ld_d_c(GB_cpu *self) { self->registers.d = self->registers.c; PC_INC(self, 1); return 4; }
Byte ins_ld_d_e(GB_cpu *self) { self->registers.d = self->registers.e; PC_INC(self, 1); return 4; }
Byte ins_ld_d_h(GB_cpu *self) { self->registers.d = self->registers.h; PC_INC(self, 1); return 4; }
Byte ins_ld_d_l(GB_cpu *self) { self->registers.d = self->registers.l; PC_INC(self, 1); return 4; }
Byte ins_ld_d_hl(GB_cpu *self) {self->registers.d = GB_mmu_read_byte(&self->memory, GB_register_get_HL(self)); PC_INC(self, 1);  return 8; }

Byte ins_ld_e_a(GB_cpu *self)  { self->registers.e = self->registers.a; PC_INC(self, 1); return 4; }
Byte ins_ld_e_b(GB_cpu *self)  { self->registers.e = self->registers.b; PC_INC(self, 1); return 4; }
Byte ins_ld_e_c(GB_cpu *self)  { self->registers.e = self->registers.c; PC_INC(self, 1); return 4; }
Byte ins_ld_e_d(GB_cpu *self)  { self->registers.e = self->registers.d; PC_INC(self, 1); return 4; }
Byte ins_ld_e_h(GB_cpu *self)  { self->registers.e = self->registers.h; PC_INC(self, 1); return 4; }
Byte ins_ld_e_l(GB_cpu *self)  { self->registers.e = self->registers.l; PC_INC(self, 1); return 4; }
Byte ins_ld_e_hl(GB_cpu *self) { self->registers.e = GB_mmu_read_byte(&self->memory, GB_register_get_HL(self)); PC_INC(self, 1);  return 8; }

Byte ins_ld_h_a(GB_cpu *self) { self->registers.h = self->registers.a; PC_INC(self, 1); return 4; }
Byte ins_ld_h_b(GB_cpu *self) { self->registers.h = self->registers.b; PC_INC(self, 1); return 4; }
Byte ins_ld_h_c(GB_cpu *self) { self->registers.h = self->registers.c; PC_INC(self, 1); return 4; }
Byte ins_ld_h_d(GB_cpu *self) { self->registers.h = self->registers.d; PC_INC(self, 1); return 4; }
Byte ins_ld_h_e(GB_cpu *self) { self->registers.h = self->registers.e; PC_INC(self, 1); return 4; }
Byte ins_ld_h_l(GB_cpu *self) { self->registers.h = self->registers.l; PC_INC(self, 1); return 4; }
Byte ins_ld_h_hl(GB_cpu *self) {self->registers.h = GB_mmu_read_byte(&self->memory, GB_register_get_HL(self)); PC_INC(self, 1);  return 8; }

Byte ins_ld_l_a(GB_cpu *self) { self->registers.l = self->registers.a; PC_INC(self, 1); return 4; }
Byte ins_ld_l_b(GB_cpu *self) { self->registers.l = self->registers.b; PC_INC(self, 1); return 4; }
Byte ins_ld_l_c(GB_cpu *self) { self->registers.l = self->registers.c; PC_INC(self, 1); return 4; }
Byte ins_ld_l_d(GB_cpu *self) { self->registers.l = self->registers.d; PC_INC(self, 1); return 4; }
Byte ins_ld_l_e(GB_cpu *self) { self->registers.l = self->registers.e; PC_INC(self, 1); return 4; }
Byte ins_ld_l_h(GB_cpu *self) { self->registers.l = self->registers.h; PC_INC(self, 1); return 4; }
Byte ins_ld_l_hl(GB_cpu *self) {self->registers.l = GB_mmu_read_byte(&self->memory, GB_register_get_HL(self)); PC_INC(self, 1);  return 8; }

Byte ins_ld_xx_sp(GB_cpu *self) { Word xx = GB_cpu_fetch_word(self, 1); GB_mmu_write_word(&self->memory, xx, self->registers.sp); PC_INC(self, 3); return 20; }
Byte ins_ld_xx_a(GB_cpu *self) { Word xx = GB_cpu_fetch_word(self, 1); GB_mmu_write_byte(&self->memory, xx, self->registers.a); PC_INC(self, 3); return 16; }

Byte ins_ld_ff00x_a(GB_cpu *self) { Byte delta = GB_cpu_fetch_byte(self, 1); GB_mmu_write_byte(&self->memory,0xff00 + delta, self->registers.a); PC_INC(self, 2); return 12; }
Byte ins_ld_ff00c_a(GB_cpu * self) { GB_mmu_write_byte(&self->memory,0xff00 + self->registers.c, self->registers.a); PC_INC(self, 1); return 8; }

Byte ins_ld_a_ff00x(GB_cpu *self) { Byte delta = GB_cpu_fetch_byte(self, 1); self->registers.a = GB_mmu_read_byte(&self->memory, 0xff00 + delta); PC_INC(self, 2); return 12; }
Byte ins_ld_a_ff00c(GB_cpu *self) { self->registers.a = GB_mmu_read_byte(&self->memory, 0xff00 + self->registers.c); PC_INC(self, 1); return 8; }

Byte ins_ld_a_bc(GB_cpu *self) { self->registers.a = GB_mmu_read_byte(&self->memory, GB_register_get_BC(self)); PC_INC(self, 1); return 8; }
Byte ins_ld_a_de(GB_cpu *self) { self->registers.a = GB_mmu_read_byte(&self->memory, GB_register_get_DE(self)); PC_INC(self, 1); return 8; }


Byte ins_inc_bc(GB_cpu *self) { GB_register_set_BC(self, GB_register_get_BC(self) + 1); PC_INC(self, 1); return 8; }
Byte ins_inc_de(GB_cpu *self) { GB_register_set_DE(self, GB_register_get_DE(self) + 1); PC_INC(self, 1); return 8; }
Byte ins_inc_hl(GB_cpu *self) { GB_register_set_HL(self, GB_register_get_HL(self) + 1); PC_INC(self, 1); return 8; }
Byte ins_inc_sp(GB_cpu *self) { self->registers.sp++; PC_INC(self, 1); return 8; }
Byte ins_inc_hl_ptr(GB_cpu *self) { Byte value = GB_mmu_read_byte(&self->memory, GB_register_get_HL(self)); value++; GB_mmu_write_byte(&self->memory,GB_register_get_HL(self), value); self->registers.f = ZeroFlagValue(value) | ((value & 0xf) == 0) << 5  | GB_cpu_get_carry_flag(self); PC_INC(self, 1); return 12; }

Byte ins_inc_a(GB_cpu *self) { self->registers.a++; self->registers.f = ZeroFlagValue(self->registers.a) | ((self->registers.a & 0xf) == 0) << 5  | GB_cpu_get_carry_flag(self); PC_INC(self, 1); return 4; }
Byte ins_inc_b(GB_cpu *self) { self->registers.b++; self->registers.f = ZeroFlagValue(self->registers.b) | ((self->registers.b & 0xf) == 0) << 5  | GB_cpu_get_carry_flag(self); PC_INC(self, 1); return 4; }
Byte ins_inc_c(GB_cpu *self) { self->registers.c++; self->registers.f = ZeroFlagValue(self->registers.c) | ((self->registers.c & 0xf) == 0) << 5  | GB_cpu_get_carry_flag(self); PC_INC(self, 1); return 4; }
Byte ins_inc_d(GB_cpu *self) { self->registers.d++; self->registers.f = ZeroFlagValue(self->registers.d) | ((self->registers.d & 0xf) == 0) << 5  | GB_cpu_get_carry_flag(self); PC_INC(self, 1); return 4; }
Byte ins_inc_e(GB_cpu *self) { self->registers.e++; self->registers.f = ZeroFlagValue(self->registers.e) | ((self->registers.e & 0xf) == 0) << 5  | GB_cpu_get_carry_flag(self); PC_INC(self, 1); return 4; }
Byte ins_inc_h(GB_cpu *self) { self->registers.h++; self->registers.f = ZeroFlagValue(self->registers.h) | ((self->registers.h & 0xf) == 0) << 5  | GB_cpu_get_carry_flag(self); PC_INC(self, 1); return 4; }
Byte ins_inc_l(GB_cpu *self) { self->registers.l++; self->registers.f = ZeroFlagValue(self->registers.l) | ((self->registers.l & 0xf) == 0) << 5  | GB_cpu_get_carry_flag(self); PC_INC(self, 1); return 4; }

Byte ins_dec_a(GB_cpu *self) { self->registers.a--;  self->registers.f = ZeroFlagValue(self->registers.a) | FLAG_SUB | ((self->registers.a & 0xf) == 0xf) << 5 | GB_cpu_get_carry_flag(self); PC_INC(self, 1); return 4; }
Byte ins_dec_b(GB_cpu *self) { self->registers.b--;  self->registers.f = ZeroFlagValue(self->registers.b) | FLAG_SUB | ((self->registers.b & 0xf) == 0xf) << 5 | GB_cpu_get_carry_flag(self); PC_INC(self, 1); return 4; }
Byte ins_dec_c(GB_cpu *self) { self->registers.c--;  self->registers.f = ZeroFlagValue(self->registers.c) | FLAG_SUB | ((self->registers.c & 0xf) == 0xf) << 5 | GB_cpu_get_carry_flag(self); PC_INC(self, 1); return 4; }
Byte ins_dec_d(GB_cpu *self) { self->registers.d--;  self->registers.f = ZeroFlagValue(self->registers.d) | FLAG_SUB | ((self->registers.d & 0xf) == 0xf) << 5 | GB_cpu_get_carry_flag(self); PC_INC(self, 1); return 4; }
Byte ins_dec_e(GB_cpu *self) { self->registers.e--;  self->registers.f = ZeroFlagValue(self->registers.e) | FLAG_SUB | ((self->registers.e & 0xf) == 0xf) << 5 | GB_cpu_get_carry_flag(self); PC_INC(self, 1); return 4; }
Byte ins_dec_h(GB_cpu *self) { self->registers.h--;  self->registers.f = ZeroFlagValue(self->registers.h) | FLAG_SUB | ((self->registers.h & 0xf) == 0xf) << 5 | GB_cpu_get_carry_flag(self); PC_INC(self, 1); return 4; }
Byte ins_dec_l(GB_cpu *self) { self->registers.l--;  self->registers.f = ZeroFlagValue(self->registers.l) | FLAG_SUB | ((self->registers.l & 0xf) == 0xf) << 5 | GB_cpu_get_carry_flag(self); PC_INC(self, 1); return 4; }

Byte ins_dec_bc(GB_cpu *self) { GB_register_set_BC(self, GB_register_get_BC(self) - 1); PC_INC(self, 1); return 8; }
Byte ins_dec_de(GB_cpu *self) { GB_register_set_BC(self, GB_register_get_DE(self) - 1); PC_INC(self, 1); return 8; }
Byte ins_dec_hl(GB_cpu *self) { GB_register_set_HL(self, GB_register_get_HL(self) - 1); PC_INC(self, 1); return 8; }
Byte ins_dec_sp(GB_cpu *self) { self->registers.sp--; PC_INC(self, 1); return 8; }

Byte ins_dec_hl_ptr(GB_cpu *self) { Byte value = GB_mmu_read_byte(&self->memory, GB_register_get_HL(self)); value--; GB_mmu_write_byte(&self->memory,GB_register_get_HL(self), value); self->registers.f = ZeroFlagValue(value) | FLAG_SUB | ((value & 0xf) == 0xf) << 5 | GB_cpu_get_carry_flag(self); PC_INC(self, 1); return 12; }

Byte ins_rlca(GB_cpu *self) { Byte c = (self->registers.a >> 7) & 0x01; self->registers.a = (self->registers.a << 1) | c; GB_cpu_set_carry(self, c); PC_INC(self, 1); return 4; }

Byte ins_rlc_a(GB_cpu *self)  { Byte c = (self->registers.a >> 7) & 0x01; self->registers.a = (self->registers.a << 1) | c; self->registers.f = ZeroFlagValue(self->registers.a) | (c << 4) ; PC_INC(self, 2); return 8; }
Byte ins_rlc_b(GB_cpu *self)  { Byte c = (self->registers.b >> 7) & 0x01; self->registers.b = (self->registers.b << 1) | c; self->registers.f = ZeroFlagValue(self->registers.b) | (c << 4) ; PC_INC(self, 2); return 8; }
Byte ins_rlc_c(GB_cpu *self)  { Byte c = (self->registers.c >> 7) & 0x01; self->registers.c = (self->registers.c << 1) | c; self->registers.f = ZeroFlagValue(self->registers.c) | (c << 4) ; PC_INC(self, 2); return 8; }
Byte ins_rlc_d(GB_cpu *self)  { Byte c = (self->registers.d >> 7) & 0x01; self->registers.d = (self->registers.d << 1) | c; self->registers.f = ZeroFlagValue(self->registers.d) | (c << 4) ; PC_INC(self, 2); return 8; }
Byte ins_rlc_e(GB_cpu *self)  { Byte c = (self->registers.e >> 7) & 0x01; self->registers.e = (self->registers.e << 1) | c; self->registers.f = ZeroFlagValue(self->registers.e) | (c << 4) ; PC_INC(self, 2); return 8; }
Byte ins_rlc_h(GB_cpu *self)  { Byte c = (self->registers.h >> 7) & 0x01; self->registers.h = (self->registers.h << 1) | c; self->registers.f = ZeroFlagValue(self->registers.h) | (c << 4) ; PC_INC(self, 2); return 8; }
Byte ins_rlc_l(GB_cpu *self)  { Byte c = (self->registers.l >> 7) & 0x01; self->registers.l = (self->registers.l << 1) | c; self->registers.f = ZeroFlagValue(self->registers.l) | (c << 4) ; PC_INC(self, 2); return 8; }
Byte ins_rlc_hl(GB_cpu *self)  { Word hl = GB_register_get_HL(self); Byte value = GB_mmu_read_byte(&self->memory, hl); Byte c = (value >> 7) & 0x01; value = (value << 1) | c; GB_mmu_write_byte(&self->memory,hl, value); self->registers.f = ZeroFlagValue(value) | (c << 4) ; PC_INC(self, 2); return 16; }

Byte ins_rrca(GB_cpu *self) { Byte c = self->registers.a & 0x01; self->registers.a = (self->registers.a >> 1) | c << 7; GB_cpu_set_carry(self, c); PC_INC(self, 1); return 4; }

Byte ins_rrc_a(GB_cpu *self) { Byte c = self->registers.a & 0x01; self->registers.a = (self->registers.a >> 1) | c << 7; self->registers.f = ZeroFlagValue(self->registers.a) | (c << 4); PC_INC(self, 2); return 8; }
Byte ins_rrc_b(GB_cpu *self) { Byte c = self->registers.b & 0x01; self->registers.b = (self->registers.b >> 1) | c << 7; self->registers.f = ZeroFlagValue(self->registers.b) | (c << 4); PC_INC(self, 2); return 8; }
Byte ins_rrc_c(GB_cpu *self) { Byte c = self->registers.c & 0x01; self->registers.c = (self->registers.c >> 1) | c << 7; self->registers.f = ZeroFlagValue(self->registers.c) | (c << 4); PC_INC(self, 2); return 8; }
Byte ins_rrc_d(GB_cpu *self) { Byte c = self->registers.d & 0x01; self->registers.d = (self->registers.d >> 1) | c << 7; self->registers.f = ZeroFlagValue(self->registers.d) | (c << 4); PC_INC(self, 2); return 8; }
Byte ins_rrc_e(GB_cpu *self) { Byte c = self->registers.e & 0x01; self->registers.e = (self->registers.e >> 1) | c << 7; self->registers.f = ZeroFlagValue(self->registers.e) | (c << 4); PC_INC(self, 2); return 8; }
Byte ins_rrc_h(GB_cpu *self) { Byte c = self->registers.h & 0x01; self->registers.h = (self->registers.h >> 1) | c << 7; self->registers.f = ZeroFlagValue(self->registers.h) | (c << 4); PC_INC(self, 2); return 8; }
Byte ins_rrc_l(GB_cpu *self) { Byte c = self->registers.l & 0x01; self->registers.l = (self->registers.l >> 1) | c << 7; self->registers.f = ZeroFlagValue(self->registers.l) | (c << 4); PC_INC(self, 2); return 8; }
Byte ins_rrc_hl(GB_cpu *self)  {  Word hl = GB_register_get_HL(self); Byte value = GB_mmu_read_byte(&self->memory, hl); Byte c = value & 0x01; value = (value >> 1) | c << 7; GB_mmu_write_byte(&self->memory,hl, value); self->registers.f = ZeroFlagValue(self->registers.l) | (c << 4); PC_INC(self, 2); return 16; }


Byte ins_rla (GB_cpu *self) { Byte oldC = GB_cpu_get_carry_flag_bit(self); GB_cpu_set_carry(self, self->registers.a >> 7); self->registers.a = (self->registers.a << 1) | oldC; PC_INC(self, 1); return 4; }

Byte ins_rl_a (GB_cpu *self) { 
    Byte oldC = GB_cpu_get_carry_flag_bit(self);
    Byte co = (self->registers.a & 0x80) ? 0x10 : 0;
    self->registers.a = (self->registers.a << 1) + oldC; 
    self->registers.f = ZeroFlagValue(self->registers.a) + co; 
    PC_INC(self, 2); 
    return 8; 
}
Byte ins_rl_b (GB_cpu *self) { 
    Byte oldC = GB_cpu_get_carry_flag_bit(self);
    Byte co = (self->registers.b & 0x80) ? 0x10 : 0;
    self->registers.b = (self->registers.b << 1) + oldC; 
    self->registers.f = ZeroFlagValue(self->registers.b) + co; 
    PC_INC(self, 2); 
    return 8; 
}
Byte ins_rl_c (GB_cpu *self) { 
    Byte oldC = GB_cpu_get_carry_flag_bit(self);
    Byte co = (self->registers.c & 0x80) ? 0x10 : 0;
    self->registers.c = (self->registers.c << 1) + oldC; 
    self->registers.f = ZeroFlagValue(self->registers.c) + co; 
    PC_INC(self, 2); 
    return 8; 
}
Byte ins_rl_d (GB_cpu *self) { 
    Byte oldC = GB_cpu_get_carry_flag_bit(self);
    Byte co = (self->registers.d & 0x80) ? 0x10 : 0;
    self->registers.d = (self->registers.d << 1) + oldC; 
    self->registers.f = ZeroFlagValue(self->registers.d) + co; 
    PC_INC(self, 2); 
    return 8; 
}
Byte ins_rl_e (GB_cpu *self) { 
    Byte oldC = GB_cpu_get_carry_flag_bit(self);
    Byte co = (self->registers.e & 0x80) ? 0x10 : 0;
    self->registers.e = (self->registers.e << 1) + oldC; 
    self->registers.f = ZeroFlagValue(self->registers.e) + co; 
    PC_INC(self, 2); 
    return 8; 
}
Byte ins_rl_h (GB_cpu *self) { 
    Byte oldC = GB_cpu_get_carry_flag_bit(self);
    Byte co = (self->registers.h & 0x80) ? 0x10 : 0;
    self->registers.h = (self->registers.h << 1) + oldC; 
    self->registers.f = ZeroFlagValue(self->registers.h) + co; 
    PC_INC(self, 2); 
    return 8; 
}
Byte ins_rl_l (GB_cpu *self) { 
    Byte oldC = GB_cpu_get_carry_flag_bit(self);
    Byte co = (self->registers.l & 0x80) ? 0x10 : 0;
    self->registers.l = (self->registers.l << 1) + oldC; 
    self->registers.f = ZeroFlagValue(self->registers.l) + co; 
    PC_INC(self, 2); 
    return 8; 
}
Byte ins_rl_hl (GB_cpu *self) { 
    Word hl = GB_register_get_HL(self); 
    Byte value = GB_mmu_read_byte(&self->memory, hl); 
    Byte oldC = GB_cpu_get_carry_flag_bit(self);
    Byte co = (value & 0x80) ? 0x10 : 0;
    value = (value << 1) + oldC; 
    GB_mmu_write_byte(&self->memory,hl, value); 
    self->registers.f = ZeroFlagValue(value) + co; 
    PC_INC(self, 2); 
    return 16; 
}

Byte ins_rra (GB_cpu *self)  { 
    Byte oldC = GB_cpu_get_carry_flag_bit(self);
    Byte lbit = (self->registers.a & 0x01);
    
    self->registers.a = self->registers.a >> 1;
    self->registers.f = 0;
    if (oldC) {
        self->registers.a |= 0x80;
    }
    if (lbit) {
        self->registers.f |= FLAG_CARRY;
    }
    PC_INC(self, 1); 
    return 4; 
}

Byte ins_rr_a (GB_cpu *self) { bool cary = GB_cpu_get_carry_flag_bit(self); bool lbit = (self->registers.a & 0x01) != 0; self->registers.a = (self->registers.a >> 1) | (cary << 7); self->registers.f = ZeroFlagValue(self->registers.a) | (lbit << 4); PC_INC(self, 2); return 8; }
Byte ins_rr_b (GB_cpu *self) { bool cary = GB_cpu_get_carry_flag_bit(self); bool lbit = (self->registers.b & 0x01) != 0; self->registers.b = (self->registers.b >> 1) | (cary << 7); self->registers.f = ZeroFlagValue(self->registers.b) | (lbit << 4); PC_INC(self, 2); return 8; }
Byte ins_rr_c (GB_cpu *self) { 
    bool lbit = (self->registers.c & 0x01) != 0;
    bool cary = GB_cpu_get_carry_flag_bit(self);
    self->registers.c = (self->registers.c >> 1) | (cary << 7); 
    self->registers.f = ZeroFlagValue(self->registers.c) | (lbit << 4); 
    PC_INC(self, 2); 
    return 8;
}
Byte ins_rr_d (GB_cpu *self) 
{
    bool cary = GB_cpu_get_carry_flag_bit(self);
    bool lbit = (self->registers.d & 0x01) != 0; 
    self->registers.d = (self->registers.d >> 1) | (cary << 7); 
    self->registers.f = ZeroFlagValue(self->registers.d) | (lbit << 4); 
    PC_INC(self, 2); 
    return 8; 
}
Byte ins_rr_e (GB_cpu *self) { bool cary = GB_cpu_get_carry_flag_bit(self); bool lbit = (self->registers.e & 0x01) != 0; self->registers.e = (self->registers.e >> 1) | (cary << 7); self->registers.f = ZeroFlagValue(self->registers.e) | (lbit << 4); PC_INC(self, 2); return 8; }
Byte ins_rr_h (GB_cpu *self) { bool cary = GB_cpu_get_carry_flag_bit(self); bool lbit = (self->registers.h & 0x01) != 0; self->registers.h = (self->registers.h >> 1) | (cary << 7); self->registers.f = ZeroFlagValue(self->registers.h) | (lbit << 4); PC_INC(self, 2); return 8; }
Byte ins_rr_l (GB_cpu *self) { bool cary = GB_cpu_get_carry_flag_bit(self); bool lbit = (self->registers.l & 0x01) != 0; self->registers.l = (self->registers.l >> 1) | (cary << 7); self->registers.f = ZeroFlagValue(self->registers.l) | (lbit << 4); PC_INC(self, 2); return 8; }
Byte ins_rr_hl (GB_cpu *self) { 
    Word hl = GB_register_get_HL(self); 
    Byte value = GB_mmu_read_byte(&self->memory, hl); 
    bool cary = GB_cpu_get_carry_flag_bit(self);
    bool lbit = (value & 0x01) != 0; 
    value = (value >> 1) | (cary << 7); 
    self->registers.f = ZeroFlagValue(value) | (lbit << 4); 
    GB_mmu_write_byte(&self->memory,hl, value);
    PC_INC(self, 2); 
    return 12; 
}

Byte ins_sla_a (GB_cpu *self) { Byte hBit = self->registers.a >> 7; self->registers.a = self->registers.a << 1; self->registers.f = ZeroFlagValue(self->registers.a) | (hBit << 4); PC_INC(self, 2); return 8; }
Byte ins_sla_b (GB_cpu *self) { Byte hBit = self->registers.b >> 7; self->registers.b = self->registers.b << 1; self->registers.f = ZeroFlagValue(self->registers.b) | (hBit << 4); PC_INC(self, 2); return 8; }
Byte ins_sla_c (GB_cpu *self) { Byte hBit = self->registers.c >> 7; self->registers.c = self->registers.c << 1; self->registers.f = ZeroFlagValue(self->registers.c) | (hBit << 4); PC_INC(self, 2); return 8; }
Byte ins_sla_d (GB_cpu *self) { Byte hBit = self->registers.d >> 7; self->registers.d = self->registers.d << 1; self->registers.f = ZeroFlagValue(self->registers.d) | (hBit << 4); PC_INC(self, 2); return 8; }
Byte ins_sla_e (GB_cpu *self) { Byte hBit = self->registers.e >> 7; self->registers.e = self->registers.e << 1; self->registers.f = ZeroFlagValue(self->registers.e) | (hBit << 4); PC_INC(self, 2); return 8; }
Byte ins_sla_h (GB_cpu *self) { Byte hBit = self->registers.h >> 7; self->registers.h = self->registers.h << 1; self->registers.f = ZeroFlagValue(self->registers.h) | (hBit << 4); PC_INC(self, 2); return 8; }
Byte ins_sla_l (GB_cpu *self) { Byte hBit = self->registers.l >> 7; self->registers.l = self->registers.l << 1; self->registers.f = ZeroFlagValue(self->registers.l) | (hBit << 4); PC_INC(self, 2); return 8; }
Byte ins_sla_hl (GB_cpu *self) { Word hl = GB_register_get_HL(self); Byte value = GB_mmu_read_byte(&self->memory, hl); Byte hBit = value >> 7; value = value << 1; self->registers.f = ZeroFlagValue(value) | (hBit << 4); GB_mmu_write_byte(&self->memory,hl, value); PC_INC(self, 2); return 16; }

Byte ins_sra_a (GB_cpu *self) { Byte hBit = self->registers.a & 0x1; self->registers.a = self->registers.a >> 1; self->registers.f = ZeroFlagValue(self->registers.a) | (hBit << 4); PC_INC(self, 2); return 8; }
Byte ins_sra_b (GB_cpu *self) { Byte hBit = self->registers.b & 0x1; self->registers.b = self->registers.b >> 1; self->registers.f = ZeroFlagValue(self->registers.b) | (hBit << 4); PC_INC(self, 2); return 8; }
Byte ins_sra_c (GB_cpu *self) { Byte hBit = self->registers.c & 0x1; self->registers.c = self->registers.c >> 1; self->registers.f = ZeroFlagValue(self->registers.c) | (hBit << 4); PC_INC(self, 2); return 8; }
Byte ins_sra_d (GB_cpu *self) { Byte hBit = self->registers.d & 0x1; self->registers.d = self->registers.d >> 1; self->registers.f = ZeroFlagValue(self->registers.d) | (hBit << 4); PC_INC(self, 2); return 8; }
Byte ins_sra_e (GB_cpu *self) { Byte hBit = self->registers.e & 0x1; self->registers.e = self->registers.e >> 1; self->registers.f = ZeroFlagValue(self->registers.e) | (hBit << 4); PC_INC(self, 2); return 8; }
Byte ins_sra_h (GB_cpu *self) { Byte hBit = self->registers.h & 0x1; self->registers.h = self->registers.h >> 1; self->registers.f = ZeroFlagValue(self->registers.h) | (hBit << 4); PC_INC(self, 2); return 8; }
Byte ins_sra_l (GB_cpu *self) { Byte hBit = self->registers.l & 0x1; self->registers.l = self->registers.l >> 1; self->registers.f = ZeroFlagValue(self->registers.l) | (hBit << 4); PC_INC(self, 2); return 8; }
Byte ins_sra_hl (GB_cpu *self) { Word hl = GB_register_get_HL(self); Byte value = GB_mmu_read_byte(&self->memory, hl); Byte hBit = value & 0x1; value = value >> 1; self->registers.f = ZeroFlagValue(value) | (hBit << 4); GB_mmu_write_byte(&self->memory,hl, value); PC_INC(self, 2); return 16; }

Byte ins_srl_a (GB_cpu *self) { Byte lBit = self->registers.a & 0x01; self->registers.a = self->registers.a >> 1; self->registers.f = ZeroFlagValue(self->registers.a) | (lBit << 4); PC_INC(self, 2); return 8; }
Byte ins_srl_b (GB_cpu *self) { 
    Byte lBit = self->registers.b & 0x01; 
    self->registers.b = self->registers.b >> 1; 
    self->registers.f = ZeroFlagValue(self->registers.b) | (lBit << 4);
    PC_INC(self, 2); 
    return 8; 
}
Byte ins_srl_c (GB_cpu *self) { Byte lBit = self->registers.c & 0x01; self->registers.c = self->registers.c >> 1; self->registers.f = ZeroFlagValue(self->registers.c) | (lBit << 4); PC_INC(self, 2); return 8; }
Byte ins_srl_d (GB_cpu *self) { Byte lBit = self->registers.d & 0x01; self->registers.d = self->registers.d >> 1; self->registers.f = ZeroFlagValue(self->registers.d) | (lBit << 4); PC_INC(self, 2); return 8; }
Byte ins_srl_e (GB_cpu *self) { Byte lBit = self->registers.e & 0x01; self->registers.e = self->registers.e >> 1; self->registers.f = ZeroFlagValue(self->registers.e) | (lBit << 4); PC_INC(self, 2); return 8; }
Byte ins_srl_h (GB_cpu *self) { Byte lBit = self->registers.h & 0x01; self->registers.h = self->registers.h >> 1; self->registers.f = ZeroFlagValue(self->registers.h) | (lBit << 4); PC_INC(self, 2); return 8; }
Byte ins_srl_l (GB_cpu *self) { Byte lBit = self->registers.l & 0x01; self->registers.l = self->registers.l >> 1; self->registers.f = ZeroFlagValue(self->registers.l) | (lBit << 4); PC_INC(self, 2); return 8; }
Byte ins_srl_hl (GB_cpu *self) { Word hl = GB_register_get_HL(self); Byte value = GB_mmu_read_byte(&self->memory, hl); Byte lBit = value & 0x01; value = value >> 1; self->registers.f = ZeroFlagValue(value) | (lBit << 4); GB_mmu_write_byte(&self->memory,hl, value); PC_INC(self, 2); return 16; }


Byte ins_swap_a(GB_cpu *self) { self->registers.a = ((self->registers.a & 0xf0) >> 4) | ((self->registers.a & 0x0f)) << 4; self->registers.f = ZeroFlagValue(self->registers.a); PC_INC(self, 2); return 8; }
Byte ins_swap_b(GB_cpu *self) { self->registers.b = ((self->registers.b & 0xf0) >> 4) | ((self->registers.b & 0x0f)) << 4; self->registers.f = ZeroFlagValue(self->registers.b); PC_INC(self, 2); return 8; }
Byte ins_swap_c(GB_cpu *self) { self->registers.c = ((self->registers.c & 0xf0) >> 4) | ((self->registers.c & 0x0f)) << 4; self->registers.f = ZeroFlagValue(self->registers.c); PC_INC(self, 2); return 8; }
Byte ins_swap_d(GB_cpu *self) { self->registers.d = ((self->registers.d & 0xf0) >> 4) | ((self->registers.d & 0x0f)) << 4; self->registers.f = ZeroFlagValue(self->registers.d); PC_INC(self, 2); return 8; }
Byte ins_swap_e(GB_cpu *self) { self->registers.e = ((self->registers.e & 0xf0) >> 4) | ((self->registers.e & 0x0f)) << 4; self->registers.f = ZeroFlagValue(self->registers.e); PC_INC(self, 2); return 8; }
Byte ins_swap_h(GB_cpu *self) { self->registers.h = ((self->registers.h & 0xf0) >> 4) | ((self->registers.h & 0x0f)) << 4; self->registers.f = ZeroFlagValue(self->registers.h); PC_INC(self, 2); return 8; }
Byte ins_swap_l(GB_cpu *self) { self->registers.l = ((self->registers.l & 0xf0) >> 4) | ((self->registers.l & 0x0f)) << 4; self->registers.f = ZeroFlagValue(self->registers.l); PC_INC(self, 2); return 8; }
Byte ins_swap_hl(GB_cpu *self) { Word hl = GB_register_get_HL(self); Byte value = GB_mmu_read_byte(&self->memory, hl); value = ((value & 0xf0) >> 4) | ((value & 0x0f)) << 4; self->registers.f = ZeroFlagValue(value); GB_mmu_write_byte(&self->memory,hl, value); PC_INC(self, 2); return 16; }

Byte ins_daa2 (GB_cpu *self) { 
    Byte ajustment = 0;
    if (GB_cpu_get_half_carry_flag_bit(self) || (GB_cpu_get_subtraction_flag_bit(self) == 0 && ((self->registers.a & 0x0f) > 0x09))) {
        ajustment = 6;
    }
    if (GB_cpu_get_carry_flag_bit(self) || (GB_cpu_get_subtraction_flag_bit(self) == 0 && self->registers.a > 0x99)) {
         ajustment |= 0x60;
    }
    Byte value = GB_cpu_get_subtraction_flag_bit(self) ? self->registers.a - ajustment : self->registers.a + ajustment;

    self->registers.a = value;
    self->registers.f = ZeroFlagValue(value) | GB_cpu_get_subtraction_flag_bit(self) << 6 | ((ajustment > 6) ? FLAG_CARRY : 0);
    PC_INC(self, 1); 
    return 4;
}

Byte ins_daa3 (GB_cpu *self) { 
    int result = self->registers.a;
    unsigned short mask = 0xFF00;
    GB_register_set_AF(self, ~(mask | FLAG_ZERO));
    if(self->registers.f & FLAG_SUB) {
        if (self->registers.f & FLAG_HALF) {
            result = (result - 0x06);
        }
        if (self->registers.f & FLAG_CARRY) {
            result -= 0x60;
        }
    } else {
        if ((self->registers.f & FLAG_HALF) || (result & 0x0F) > 0x09) {
            result += 0x06;
        }
        if ((self->registers.f & FLAG_CARRY) || result > 0x9F) {
            result += 0x60;
        }
    }
    if ((result & 0xFF) == 0) {
        self->registers.f |= FLAG_ZERO;
    }

    if ((result & 0x100) == 0x100) {
        self->registers.f |= FLAG_CARRY;
    }

    self->registers.a |= result;
    self->registers.f &= ~FLAG_HALF;
    PC_INC(self, 1);
    return 4;
}

Byte ins_daa (GB_cpu *self) { 
    int result = self->registers.a;
    if(self->registers.f & FLAG_SUB) {
        if (self->registers.f & FLAG_HALF) {
            result -= 0x06;
            if (! (self->registers.f & FLAG_CARRY)) {
                result &= 0xff;
            }
        }
        if (self->registers.f & FLAG_CARRY) {
            result -= 0x60;
        }
    } else {
        if ((self->registers.f & FLAG_HALF) || (result & 0x0F) > 0x09) {
            result += 0x06;
        }
        if ((self->registers.f & FLAG_CARRY) || result > 0x9F) {
            result += 0x60;
        }
    }

    self->registers.f &= ~ (FLAG_HALF | FLAG_ZERO);

    if (result & 0x100) {
        self->registers.f |= FLAG_CARRY;
    }
    self->registers.a = result & 0xff;

    if (! self->registers.a) {
        self->registers.f |= FLAG_ZERO;
    }

    PC_INC(self, 1);
    return 4;
}

Byte ins_cpl(GB_cpu *self) {
    self->registers.a = ~self->registers.a;
    self->registers.f = GB_cpu_zero_flag(self) | FLAG_SUB | FLAG_HALF | GB_cpu_get_carry_flag(self);
    PC_INC(self, 1);
    return 4;
}


Byte ins_jr_x    (GB_cpu *self) { self->registers.pc += 2 + ((char)GB_mmu_read_byte(&self->memory, self->registers.pc + 1)); return 12; }
Byte ins_jr_nz_x (GB_cpu *self) { 
    if(GB_cpu_zero_flag(self) == 0) { 
        self->registers.pc += 2 + ((char)GB_cpu_fetch_byte(self, 1)); 
        return 12;
    }
    PC_INC(self, 2);
    return 8;
 }
Byte ins_jr_nc_x (GB_cpu *self) { 
    if(GB_cpu_get_carry_flag_bit(self) == 0) { 
        self->registers.pc += 2 +((char) GB_cpu_fetch_byte(self, 1)); 
        return 12;
    }
    PC_INC(self, 2);
    return 8;
}
Byte ins_jr_c_x (GB_cpu *self) { 
    if(GB_cpu_get_carry_flag_bit(self) != 0) { 
        self->registers.pc += 2 + ((char) GB_cpu_fetch_byte(self, 1)); 
        return 12;
    }
    PC_INC(self, 2);
    return 8;
}
Byte ins_jr_z_x  (GB_cpu *self) { 
    if(GB_cpu_zero_flag(self) != 0) { 
        self->registers.pc += 2 + ((char)GB_cpu_fetch_byte(self, 1)); 
        return 12;
    }
    PC_INC(self, 2);
    return 8;
 }

Byte ins_jp_xx    (GB_cpu *self) { self->registers.pc = GB_cpu_fetch_word(self, 1); return 16; }
Byte ins_jp_nz_xx (GB_cpu *self) { 
    if(GB_cpu_zero_flag(self) == 0) { 
        self->registers.pc = GB_cpu_fetch_word(self, 1); 
        return 16;
    }
    PC_INC(self, 3);
    return 12; 
}
Byte ins_jp_z_xx (GB_cpu *self) { if(GB_cpu_zero_flag(self) != 0) { self->registers.pc = GB_cpu_fetch_word(self, 1); return 16;}  PC_INC(self, 3); return 12; }
Byte ins_jp_nc_xx (GB_cpu *self) { 
    if(GB_cpu_get_carry_flag(self) == 0) { 
        self->registers.pc = GB_cpu_fetch_word(self, 1);
        return 16;
    }
    PC_INC(self, 3); 
    return 12; 
}
Byte ins_jp_c_xx (GB_cpu *self) { if(GB_cpu_get_carry_flag(self) != 0) { self->registers.pc = GB_cpu_fetch_word(self, 1); return 16;} PC_INC(self, 3); return 12; }
Byte ins_jp_hl   (GB_cpu *self) { self->registers.pc = GB_register_get_HL(self); return 4; }

Byte ins_add_hl_bc(GB_cpu *self) { 
    Word x1 = GB_register_get_HL(self), x2 = GB_register_get_BC(self);
    Word value = x1 + x2;
    GB_register_set_HL(self, value);
    self->registers.f = (self->registers.f & FLAG_ZERO) | HalfCarryFlagValueW(x1, x2) | CarryFlagValueAdd(value, x1, x2);
    PC_INC(self, 1);
    return 8; 
}
Byte ins_add_hl_de(GB_cpu *self) { 
    Word x1 = GB_register_get_HL(self), x2 = GB_register_get_DE(self);
    Word value = x1 + x2;
    GB_register_set_HL(self, value);
    self->registers.f = (self->registers.f & FLAG_ZERO) | HalfCarryFlagValueW(x1, x2) | CarryFlagValueAdd(value, x1, x2);
    PC_INC(self, 1);
    return 8; 
}
Byte ins_add_hl_hl(GB_cpu *self) { 
    Word x1 = GB_register_get_HL(self);
    Word value = x1 + x1;
    GB_register_set_HL(self, value);
    self->registers.f = (self->registers.f & FLAG_ZERO) | HalfCarryFlagValueW(x1, x1) | CarryFlagValueAdd(value, x1, x1);
    PC_INC(self, 1);
    return 8;
}
Byte ins_add_hl_sp(GB_cpu *self) { 
    Word x1 = GB_register_get_HL(self), x2 = self->registers.sp;
    Word value = x1 + x2;
    GB_register_set_HL(self, value);
    self->registers.f = (self->registers.f & FLAG_ZERO) | HalfCarryFlagValueW(x1, x2) | CarryFlagValueAdd(value, x1, x2);
    PC_INC(self, 1);
    return 8;
}

Byte ins_add_a_a(GB_cpu *self) { 
    Byte value = self->registers.a + self->registers.a; 
    self->registers.f = (self->registers.f & FLAG_ZERO) | HalfCarryFlagValue(self->registers.a, self->registers.a) | CarryFlagValueAdd(value, self->registers.a, self->registers.a); 
    self->registers.a = value; 
    PC_INC(self, 1); 
    return 4; 
}
Byte ins_add_a_b(GB_cpu *self) { 
    Byte value = self->registers.a + self->registers.b; 
    self->registers.f = (self->registers.f & FLAG_ZERO) | HalfCarryFlagValue(self->registers.a, self->registers.b) | CarryFlagValueAdd(value, self->registers.a, self->registers.b); 
    self->registers.a = value; 
    PC_INC(self, 1); 
    return 4; 
}
Byte ins_add_a_c(GB_cpu *self) { 
    Byte value = self->registers.a + self->registers.c; 
    self->registers.f = (self->registers.f & FLAG_ZERO) | HalfCarryFlagValue(self->registers.a, self->registers.c) | CarryFlagValueAdd(value, self->registers.a, self->registers.c); 
    self->registers.a = value;
    PC_INC(self, 1);
    return 4; 
}
Byte ins_add_a_d(GB_cpu *self) { 
    Byte value = self->registers.a + self->registers.d; 
    self->registers.f = (self->registers.f & FLAG_ZERO) | HalfCarryFlagValue(self->registers.a, self->registers.d) | CarryFlagValueAdd(value, self->registers.a, self->registers.d); 
    self->registers.a = value;
    PC_INC(self, 1);
    return 4; 
}
Byte ins_add_a_e(GB_cpu *self) { 
    Byte value = self->registers.a + self->registers.e; 
    self->registers.f = (self->registers.f & FLAG_ZERO) | HalfCarryFlagValue(self->registers.a, self->registers.e) | CarryFlagValueAdd(value, self->registers.a, self->registers.e); 
    self->registers.a = value;
    PC_INC(self, 1);
    return 4; 
}
Byte ins_add_a_h(GB_cpu *self) { 
    Byte value = self->registers.a + self->registers.h; 
    self->registers.f = (self->registers.f & FLAG_ZERO) | HalfCarryFlagValue(self->registers.a, self->registers.h) | CarryFlagValueAdd(value, self->registers.a, self->registers.h); 
    self->registers.a = value;
    PC_INC(self, 1);
    return 4; 
}
Byte ins_add_a_l(GB_cpu *self) { 
    Byte value = self->registers.a + self->registers.l; 
    self->registers.f = (self->registers.f & FLAG_ZERO) | HalfCarryFlagValue(self->registers.a, self->registers.l) | CarryFlagValueAdd(value, self->registers.a, self->registers.l); 
    self->registers.a = value;
    PC_INC(self, 1);
    return 4; 
}
Byte ins_add_a_hl(GB_cpu *self) { 
    Byte hlValue =  GB_mmu_read_byte(&self->memory, GB_register_get_HL(self));
    Byte value = self->registers.a + hlValue; 
    self->registers.f = (self->registers.f & FLAG_ZERO) | HalfCarryFlagValue(self->registers.a, hlValue) | CarryFlagValueAdd(value, self->registers.a, hlValue); 
    self->registers.a = value;
    PC_INC(self, 1);
    return 4; 
}
Byte ins_add_a_x(GB_cpu *self) { 
    Byte x = GB_cpu_fetch_byte(self, 1);
    Byte value = self->registers.a + x; 
    self->registers.f = (self->registers.f & FLAG_ZERO) | HalfCarryFlagValue(self->registers.a, x) | CarryFlagValueAdd(value, self->registers.a, x); 
    self->registers.a = value;
    PC_INC(self, 2);
    return 4; 
}
Byte ins_add_sp_x(GB_cpu *self) { 
    int16_t offset = (int8_t) GB_cpu_fetch_byte(self, 1);
    Word sp = self->registers.sp;
    self->registers.sp += offset;

    self->registers.f = 0;

    /* A new instruction, a new meaning for Half Carry! Thanks Sameboy */
    if ((sp & 0xF) + (offset & 0xF) > 0xF) {
       self->registers.f |= FLAG_HALF;
    }
    if ((sp & 0xFF) + (offset & 0xFF) > 0xFF)  {
        self->registers.f |= FLAG_CARRY;
    }
    PC_INC(self, 2);
    return 16; 
}

Byte ins_sub_a_a(GB_cpu *self) { 
    Byte value = self->registers.a - self->registers.a; 
    self->registers.f = ZeroFlagValue(value) | FLAG_SUB | HalfCarrySubFlagValue(self->registers.a, self->registers.a) | CarrySubFlagValueAdd(self->registers.a, self->registers.a); 
    self->registers.a = value; 
    PC_INC(self, 1); 
    return 4; 
}
Byte ins_sub_a_b(GB_cpu *self) { 
    Byte value = self->registers.a - self->registers.b; 
    self->registers.f = ZeroFlagValue(value) | FLAG_SUB | HalfCarrySubFlagValue(self->registers.a, self->registers.b) | CarrySubFlagValueAdd(self->registers.a, self->registers.b); 
    self->registers.a = value; 
    PC_INC(self, 1); 
    return 4; 
}
Byte ins_sub_a_c(GB_cpu *self) {
    Byte value = self->registers.a - self->registers.c; 
    self->registers.f = ZeroFlagValue(value) | FLAG_SUB | HalfCarrySubFlagValue(self->registers.a, self->registers.c) | CarrySubFlagValueAdd(self->registers.a, self->registers.c); 
    self->registers.a = value; 
    PC_INC(self, 1); 
    return 4; 
}
Byte ins_sub_a_d(GB_cpu *self) {
    Byte value = self->registers.a - self->registers.d; 
    self->registers.f = ZeroFlagValue(value) | FLAG_SUB | HalfCarrySubFlagValue(self->registers.a, self->registers.d) | CarrySubFlagValueAdd(self->registers.a, self->registers.d); 
    self->registers.a = value; 
    PC_INC(self, 1);
    return 4; 
}
Byte ins_sub_a_e(GB_cpu *self) {
    Byte value = self->registers.a - self->registers.e; 
    self->registers.f = ZeroFlagValue(value) | FLAG_SUB | HalfCarrySubFlagValue(self->registers.a, self->registers.e) | CarrySubFlagValueAdd(self->registers.a, self->registers.e); 
    self->registers.a = value; 
    PC_INC(self, 1);
    return 4; 
}
Byte ins_sub_a_h(GB_cpu *self) {
    Byte value = self->registers.a - self->registers.h; 
    self->registers.f = ZeroFlagValue(value) | FLAG_SUB | HalfCarrySubFlagValue(self->registers.a, self->registers.h) | CarrySubFlagValueAdd(self->registers.a, self->registers.h); 
    self->registers.a = value; 
    PC_INC(self, 1);
    return 4; 
}
Byte ins_sub_a_l(GB_cpu *self) {
    Byte value = self->registers.a - self->registers.l; 
    self->registers.f = ZeroFlagValue(value) | FLAG_SUB | HalfCarrySubFlagValue(self->registers.a, self->registers.l) | CarrySubFlagValueAdd(self->registers.a, self->registers.l); 
    self->registers.a = value; 
    PC_INC(self, 1);
    return 4; 
}
Byte ins_sub_a_hl(GB_cpu *self) {
    Byte hlValue =  GB_mmu_read_byte(&self->memory, GB_register_get_HL(self));
    Byte value = self->registers.a - hlValue; 
    self->registers.f = ZeroFlagValue(value) | FLAG_SUB | HalfCarrySubFlagValue(self->registers.a, hlValue) | CarrySubFlagValueAdd(self->registers.a, hlValue); 
    self->registers.a = value; 
    PC_INC(self, 1);
    return 4; 
}
Byte ins_sub_a_x(GB_cpu *self) {
    Byte x = GB_cpu_fetch_byte(self, 1);
    Byte value = self->registers.a - x; 
    self->registers.f = ZeroFlagValue(value) | FLAG_SUB | HalfCarrySubFlagValue(self->registers.a, x) | CarrySubFlagValueAdd(self->registers.a, x); 
    self->registers.a = value; 
    PC_INC(self, 2);
    return 4; 
}

Byte ins_adc_a_a(GB_cpu *self) { Byte c = GB_cpu_get_carry_flag_bit(self); Byte v = self->registers.a + self->registers.a + c; self->registers.f = ZeroFlagValue(v) | HalfCarryValueC(self->registers.a, self->registers.a, c) |CarryFlagValueAdd(v, self->registers.a, self->registers.a); self->registers.a = v;PC_INC(self, 1); return 4; }
Byte ins_adc_a_b(GB_cpu *self) { Byte c = GB_cpu_get_carry_flag_bit(self); Byte v = self->registers.a + self->registers.b + c; self->registers.f = ZeroFlagValue(v) | HalfCarryValueC(self->registers.a, self->registers.b, c) |CarryFlagValueAdd(v, self->registers.a, self->registers.b); self->registers.a = v;PC_INC(self, 1); return 4; }
Byte ins_adc_a_c(GB_cpu *self) { Byte c = GB_cpu_get_carry_flag_bit(self); Byte v = self->registers.a + self->registers.c + c; self->registers.f = ZeroFlagValue(v) | HalfCarryValueC(self->registers.a, self->registers.c, c) |CarryFlagValueAdd(v, self->registers.a, self->registers.c); self->registers.a = v;PC_INC(self, 1); return 4; }
Byte ins_adc_a_d(GB_cpu *self) { Byte c = GB_cpu_get_carry_flag_bit(self); Byte v = self->registers.a + self->registers.d + c; self->registers.f = ZeroFlagValue(v) | HalfCarryValueC(self->registers.a, self->registers.d, c) |CarryFlagValueAdd(v, self->registers.a, self->registers.d); self->registers.a = v;PC_INC(self, 1); return 4; }
Byte ins_adc_a_e(GB_cpu *self) { Byte c = GB_cpu_get_carry_flag_bit(self); Byte v = self->registers.a + self->registers.e + c; self->registers.f = ZeroFlagValue(v) | HalfCarryValueC(self->registers.a, self->registers.e, c) |CarryFlagValueAdd(v, self->registers.a, self->registers.e); self->registers.a = v;PC_INC(self, 1); return 4; }
Byte ins_adc_a_h(GB_cpu *self) { Byte c = GB_cpu_get_carry_flag_bit(self); Byte v = self->registers.a + self->registers.h + c; self->registers.f = ZeroFlagValue(v) | HalfCarryValueC(self->registers.a, self->registers.h, c) |CarryFlagValueAdd(v, self->registers.a, self->registers.h); self->registers.a = v;PC_INC(self, 1); return 4; }
Byte ins_adc_a_l(GB_cpu *self) { Byte c = GB_cpu_get_carry_flag_bit(self); Byte v = self->registers.a + self->registers.l + c; self->registers.f = ZeroFlagValue(v) | HalfCarryValueC(self->registers.a, self->registers.l, c) |CarryFlagValueAdd(v, self->registers.a, self->registers.l); self->registers.a = v;PC_INC(self, 1); return 4; }
Byte ins_adc_a_x(GB_cpu *self) { Byte c = GB_cpu_get_carry_flag_bit(self), x = GB_cpu_fetch_byte(self, 1); Byte v = self->registers.a + x + c; self->registers.f = ZeroFlagValue(v) | HalfCarryValueC(self->registers.a, x, c) |CarryFlagValueAdd(v, self->registers.a, x); self->registers.a = v;PC_INC(self, 2); return 8; }
Byte ins_adc_a_hl(GB_cpu *self){ Byte c = GB_cpu_get_carry_flag_bit(self), x2 = GB_mmu_read_byte(&self->memory, GB_register_get_HL(self)); Byte v = self->registers.a + x2 + c; self->registers.f = ZeroFlagValue(v) | HalfCarryValueC(self->registers.a, x2, c) |CarryFlagValueAdd(v, self->registers.a, x2); self->registers.a = v; PC_INC(self, 1); return 8; }

Byte ins_sdc_a_a(GB_cpu *self) { Byte c = GB_cpu_get_carry_flag_bit(self); Byte v = self->registers.a - self->registers.a - c; self->registers.f = ZeroFlagValue(v) | FLAG_ZERO | HalfCarrySubFlagValueC(self->registers.a, self->registers.a, c) | CarrySubFlagValueAddC(self->registers.a, self->registers.a, c); self->registers.a = v; PC_INC(self, 1); return 4; }
Byte ins_sdc_a_b(GB_cpu *self) { Byte c = GB_cpu_get_carry_flag_bit(self); Byte v = self->registers.a - self->registers.b - c; self->registers.f = ZeroFlagValue(v) | FLAG_ZERO | HalfCarrySubFlagValueC(self->registers.a, self->registers.b, c) | CarrySubFlagValueAddC(self->registers.a, self->registers.b, c); self->registers.a = v; PC_INC(self, 1); return 4; }
Byte ins_sdc_a_c(GB_cpu *self) { Byte c = GB_cpu_get_carry_flag_bit(self); Byte v = self->registers.a - self->registers.c - c; self->registers.f = ZeroFlagValue(v) | FLAG_ZERO | HalfCarrySubFlagValueC(self->registers.a, self->registers.c, c) | CarrySubFlagValueAddC(self->registers.a, self->registers.c, c); self->registers.a = v; PC_INC(self, 1); return 4; }
Byte ins_sdc_a_d(GB_cpu *self) { Byte c = GB_cpu_get_carry_flag_bit(self); Byte v = self->registers.a - self->registers.d - c; self->registers.f = ZeroFlagValue(v) | FLAG_ZERO | HalfCarrySubFlagValueC(self->registers.a, self->registers.d, c) | CarrySubFlagValueAddC(self->registers.a, self->registers.d, c); self->registers.a = v; PC_INC(self, 1); return 4; }
Byte ins_sdc_a_e(GB_cpu *self) { Byte c = GB_cpu_get_carry_flag_bit(self); Byte v = self->registers.a - self->registers.e - c; self->registers.f = ZeroFlagValue(v) | FLAG_ZERO | HalfCarrySubFlagValueC(self->registers.a, self->registers.e, c) | CarrySubFlagValueAddC(self->registers.a, self->registers.e, c); self->registers.a = v; PC_INC(self, 1); return 4; }
Byte ins_sdc_a_h(GB_cpu *self) { Byte c = GB_cpu_get_carry_flag_bit(self); Byte v = self->registers.a - self->registers.h - c; self->registers.f = ZeroFlagValue(v) | FLAG_ZERO | HalfCarrySubFlagValueC(self->registers.a, self->registers.h, c) | CarrySubFlagValueAddC(self->registers.a, self->registers.h, c); self->registers.a = v; PC_INC(self, 1); return 4; }
Byte ins_sdc_a_l(GB_cpu *self) { Byte c = GB_cpu_get_carry_flag_bit(self); Byte v = self->registers.a - self->registers.l - c; self->registers.f = ZeroFlagValue(v) | FLAG_ZERO | HalfCarrySubFlagValueC(self->registers.a, self->registers.l, c) | CarrySubFlagValueAddC(self->registers.a, self->registers.l, c); self->registers.a = v; PC_INC(self, 1); return 4; }
Byte ins_sdc_a_x(GB_cpu *self) { Byte c = GB_cpu_get_carry_flag_bit(self), x = GB_cpu_fetch_byte(self, 1); Byte v = self->registers.a - x - c; self->registers.f = ZeroFlagValue(v) | FLAG_ZERO | HalfCarrySubFlagValueC(self->registers.a, x, c) | CarrySubFlagValueAddC(self->registers.a, x, c); self->registers.a = v; PC_INC(self, 2); return 8; }
Byte ins_sdc_a_hl(GB_cpu *self){ Byte c = GB_cpu_get_carry_flag_bit(self), x2 = GB_mmu_read_byte(&self->memory, GB_register_get_HL(self)); Byte v = self->registers.a - x2 - c; self->registers.f = ZeroFlagValue(v) | FLAG_ZERO | HalfCarrySubFlagValueC(self->registers.a, x2, c) | CarrySubFlagValueAddC(self->registers.a, x2, c); self->registers.a = v; PC_INC(self, 1); return 8; }

Byte ins_and_a_a(GB_cpu *self) { self->registers.a = self->registers.a & self->registers.a; self->registers.f = ZeroFlagValue(self->registers.a); PC_INC(self, 1); return 4; }
Byte ins_and_a_b(GB_cpu *self) { self->registers.a = self->registers.a & self->registers.b; self->registers.f = ZeroFlagValue(self->registers.a); PC_INC(self, 1); return 4; }
Byte ins_and_a_c(GB_cpu *self) { self->registers.a = self->registers.a & self->registers.c; self->registers.f = ZeroFlagValue(self->registers.a); PC_INC(self, 1); return 4; }
Byte ins_and_a_d(GB_cpu *self) { self->registers.a = self->registers.a & self->registers.d; self->registers.f = ZeroFlagValue(self->registers.a); PC_INC(self, 1); return 4; }
Byte ins_and_a_e(GB_cpu *self) { self->registers.a = self->registers.a & self->registers.e; self->registers.f = ZeroFlagValue(self->registers.a); PC_INC(self, 1); return 4; }
Byte ins_and_a_h(GB_cpu *self) { self->registers.a = self->registers.a & self->registers.h; self->registers.f = ZeroFlagValue(self->registers.a); PC_INC(self, 1); return 4; }
Byte ins_and_a_l(GB_cpu *self) { self->registers.a = self->registers.a & self->registers.l; self->registers.f = ZeroFlagValue(self->registers.a); PC_INC(self, 1); return 4; }
Byte ins_and_a_hl(GB_cpu *self) { Byte hlv =  GB_mmu_read_byte(&self->memory, GB_register_get_HL(self)); self->registers.a = self->registers.a & hlv; self->registers.f = ZeroFlagValue(self->registers.a) | FLAG_HALF; PC_INC(self, 1); return 8; }
Byte ins_and_a_x(GB_cpu *self) { 
    Byte val = GB_cpu_fetch_byte(self, 1);
    self->registers.a = self->registers.a & val; 
    self->registers.f = ZeroFlagValue(self->registers.a); 
    PC_INC(self, 2); 
    return 8; 
}

Byte ins_or_a_a(GB_cpu *self) { self->registers.a = self->registers.a | self->registers.a; self->registers.f = ZeroFlagValue(self->registers.a); PC_INC(self, 1); return 4; }
Byte ins_or_a_b(GB_cpu *self) { self->registers.a = self->registers.a | self->registers.b; self->registers.f = ZeroFlagValue(self->registers.a); PC_INC(self, 1); return 4; }
Byte ins_or_a_c(GB_cpu *self) { self->registers.a = self->registers.a | self->registers.c; self->registers.f = ZeroFlagValue(self->registers.a); PC_INC(self, 1); return 4; }
Byte ins_or_a_d(GB_cpu *self) { self->registers.a = self->registers.a | self->registers.d; self->registers.f = ZeroFlagValue(self->registers.a); PC_INC(self, 1); return 4; }
Byte ins_or_a_e(GB_cpu *self) { self->registers.a = self->registers.a | self->registers.e; self->registers.f = ZeroFlagValue(self->registers.a); PC_INC(self, 1); return 4; }
Byte ins_or_a_h(GB_cpu *self) { self->registers.a = self->registers.a | self->registers.h; self->registers.f = ZeroFlagValue(self->registers.a); PC_INC(self, 1); return 4; }
Byte ins_or_a_l(GB_cpu *self) { self->registers.a = self->registers.a | self->registers.l; self->registers.f = ZeroFlagValue(self->registers.a); PC_INC(self, 1); return 4; }
Byte ins_or_a_x(GB_cpu *self) { self->registers.a = self->registers.a | GB_cpu_fetch_byte(self, 1); self->registers.f = ZeroFlagValue(self->registers.a); PC_INC(self, 2); return 8; }
Byte ins_or_a_hl(GB_cpu *self) { Byte hlv =  GB_mmu_read_byte(&self->memory, GB_register_get_HL(self)); self->registers.a = self->registers.a | hlv; self->registers.f = ZeroFlagValue(self->registers.a); PC_INC(self, 1); return 8; }

Byte ins_xor_a_a(GB_cpu *self) { self->registers.a = self->registers.a ^ self->registers.a; self->registers.f = ZeroFlagValue(self->registers.a); PC_INC(self, 1); return 4; }
Byte ins_xor_a_b(GB_cpu *self) { self->registers.a = self->registers.a ^ self->registers.b; self->registers.f = ZeroFlagValue(self->registers.a); PC_INC(self, 1); return 4; }
Byte ins_xor_a_c(GB_cpu *self) { self->registers.a = self->registers.a ^ self->registers.c; self->registers.f = ZeroFlagValue(self->registers.a); PC_INC(self, 1); return 4; }
Byte ins_xor_a_d(GB_cpu *self) { self->registers.a = self->registers.a ^ self->registers.d; self->registers.f = ZeroFlagValue(self->registers.a); PC_INC(self, 1); return 4; }
Byte ins_xor_a_e(GB_cpu *self) { self->registers.a = self->registers.a ^ self->registers.e; self->registers.f = ZeroFlagValue(self->registers.a); PC_INC(self, 1); return 4; }
Byte ins_xor_a_h(GB_cpu *self) { self->registers.a = self->registers.a ^ self->registers.h; self->registers.f = ZeroFlagValue(self->registers.a); PC_INC(self, 1); return 4; }
Byte ins_xor_a_l(GB_cpu *self) { self->registers.a = self->registers.a ^ self->registers.l; self->registers.f = ZeroFlagValue(self->registers.a); PC_INC(self, 1); return 4; }
Byte ins_xor_a_x(GB_cpu *self) { self->registers.a = self->registers.a ^ GB_cpu_fetch_byte(self, 1); self->registers.f = ZeroFlagValue(self->registers.a); PC_INC(self, 2); return 8; }
Byte ins_xor_a_hl(GB_cpu *self) {
    Byte hlv =  GB_mmu_read_byte(&self->memory, GB_register_get_HL(self)); 
    self->registers.a = self->registers.a ^ hlv; 
    self->registers.f = ZeroFlagValue(self->registers.a); 
    PC_INC(self, 1); 
    return 8;
}

Byte ins_cp_a_a(GB_cpu *self) { Byte value = self->registers.a - self->registers.a;  self->registers.f = ZeroFlagValue(value) | FLAG_SUB | HalfCarrySubFlagValue(self->registers.a, self->registers.a) | CarrySubFlagValueAdd(self->registers.a, self->registers.a); PC_INC(self, 1); return 4;  }
Byte ins_cp_a_b(GB_cpu *self) { 
    Byte value = self->registers.a - self->registers.b; 
    self->registers.f = ZeroFlagValue(value) | FLAG_SUB | HalfCarrySubFlagValue(self->registers.a, self->registers.b) | CarrySubFlagValueAdd(self->registers.a, self->registers.b); 
    PC_INC(self, 1); 
    return 4; 
}
Byte ins_cp_a_c(GB_cpu *self) {
    Byte value = self->registers.a - self->registers.c; 
    self->registers.f = ZeroFlagValue(value) | FLAG_SUB | HalfCarrySubFlagValue(self->registers.a, self->registers.c) | CarrySubFlagValueAdd(self->registers.a, self->registers.c); 
    PC_INC(self, 1); 
    return 4; 
}
Byte ins_cp_a_d(GB_cpu *self) {
    Byte value = self->registers.a - self->registers.d; 
    self->registers.f = ZeroFlagValue(value) | FLAG_SUB | HalfCarrySubFlagValue(self->registers.a, self->registers.d) | CarrySubFlagValueAdd(self->registers.a, self->registers.d); 
    PC_INC(self, 1);
    return 4; 
}
Byte ins_cp_a_e(GB_cpu *self) {
    Byte value = self->registers.a - self->registers.e; 
    self->registers.f = ZeroFlagValue(value) | FLAG_SUB | HalfCarrySubFlagValue(self->registers.a, self->registers.e) | CarrySubFlagValueAdd(self->registers.a, self->registers.e); 
    PC_INC(self, 1);
    return 4; 
}
Byte ins_cp_a_h(GB_cpu *self) {
    Byte value = self->registers.a - self->registers.h; 
    self->registers.f = ZeroFlagValue(value) | FLAG_SUB | HalfCarrySubFlagValue(self->registers.a, self->registers.h) | CarrySubFlagValueAdd(self->registers.a, self->registers.h); 
    PC_INC(self, 1);
    return 4; 
}
Byte ins_cp_a_l(GB_cpu *self) {
    Byte value = self->registers.a - self->registers.l; 
    self->registers.f = ZeroFlagValue(value) | FLAG_SUB | HalfCarrySubFlagValue(self->registers.a, self->registers.l) | CarrySubFlagValueAdd(self->registers.a, self->registers.l); 
    PC_INC(self, 1);
    return 4; 
}
Byte ins_cp_a_hl(GB_cpu *self) {
    Byte hlValue =  GB_mmu_read_byte(&self->memory, GB_register_get_HL(self));
    Byte value = self->registers.a - hlValue; 
    self->registers.f = ZeroFlagValue(value) | FLAG_SUB | HalfCarrySubFlagValue(self->registers.a, hlValue) | CarrySubFlagValueAdd(self->registers.a, hlValue); 
    PC_INC(self, 1);
    return 4; 
}
Byte ins_cp_a_x(GB_cpu *self) {
    Byte x = GB_cpu_fetch_byte(self, 1);
    Byte value = self->registers.a - x; 
    self->registers.f = ZeroFlagValue(value) | FLAG_SUB | HalfCarrySubFlagValue(self->registers.a, x) | CarrySubFlagValueAdd(self->registers.a, x); 
    PC_INC(self, 2);
    return 4; 
}

Byte ins_ret_nz(GB_cpu *self) { if(GB_cpu_zero_flag(self) == 0) { self->registers.pc = GB_cpu_pop_stack(self); return 20; } PC_INC(self, 1); return 8; }
Byte ins_ret_z(GB_cpu *self)  { if(GB_cpu_zero_flag(self) != 0) { self->registers.pc = GB_cpu_pop_stack(self); return 20; } PC_INC(self, 1); return 8; }
Byte ins_ret_nc(GB_cpu *self) { if(GB_cpu_get_carry_flag(self) == 0) { self->registers.pc = GB_cpu_pop_stack(self); return 20; } PC_INC(self, 1); return 8; }
Byte ins_ret_c(GB_cpu *self)  { if(GB_cpu_get_carry_flag(self) != 0) { self->registers.pc = GB_cpu_pop_stack(self); return 20; } PC_INC(self, 1); return 8; }
Byte ins_ret(GB_cpu *self)    { self->registers.pc = GB_cpu_pop_stack(self); return 16; }
Byte ins_reti(GB_cpu *self)   { self->registers.pc = GB_cpu_pop_stack(self); self->IME = true; return 16; }

Byte ins_pop_bc(GB_cpu *self) { GB_register_set_BC(self, GB_cpu_pop_stack(self)); PC_INC(self, 1); return 12; }
Byte ins_pop_de(GB_cpu *self) { GB_register_set_DE(self, GB_cpu_pop_stack(self)); PC_INC(self, 1); return 12; }
Byte ins_pop_hl(GB_cpu *self) { GB_register_set_HL(self, GB_cpu_pop_stack(self)); PC_INC(self, 1); return 12; }
Byte ins_pop_af(GB_cpu *self) { GB_register_set_AF(self, GB_cpu_pop_stack(self) & 0xFFF0); PC_INC(self, 1); return 12; }

Byte ins_push_bc(GB_cpu *self) {
    GB_cpu_push_stack(self, GB_register_get_BC(self)); 
    PC_INC(self, 1); 
    return 16; 
}
Byte ins_push_de(GB_cpu *self) { GB_cpu_push_stack(self, GB_register_get_DE(self)); PC_INC(self, 1); return 16; }
Byte ins_push_hl(GB_cpu *self) { GB_cpu_push_stack(self, GB_register_get_HL(self)); PC_INC(self, 1); return 16; }
Byte ins_push_af(GB_cpu *self) { GB_cpu_push_stack(self, GB_register_get_AF(self)); PC_INC(self, 1); return 16; }

Byte ins_call_nz_xx(GB_cpu *self) { if(GB_cpu_zero_flag(self) == 0) { GB_cpu_push_stack(self, self->registers.pc + 3); self->registers.pc = GB_cpu_fetch_word(self, 1); return 24; } PC_INC(self, 3); return 12; }
Byte ins_call_z_xx(GB_cpu *self) { if(GB_cpu_zero_flag(self) != 0) { GB_cpu_push_stack(self, self->registers.pc + 3); self->registers.pc = GB_cpu_fetch_word(self, 1); return 24; } PC_INC(self, 3); return 12; }
Byte ins_call_nc_xx(GB_cpu *self) { if(GB_cpu_get_carry_flag(self) == 0) { GB_cpu_push_stack(self, self->registers.pc + 3); self->registers.pc = GB_cpu_fetch_word(self, 1); return 24; } PC_INC(self, 3); return 12; }
Byte ins_call_c_xx(GB_cpu *self) { if(GB_cpu_get_carry_flag(self) != 0) { GB_cpu_push_stack(self, self->registers.pc + 3); self->registers.pc = GB_cpu_fetch_word(self, 1); return 24; } PC_INC(self, 3); return 12; }
Byte ins_call_xx(GB_cpu *self) { GB_cpu_push_stack(self, self->registers.pc + 3); self->registers.pc = GB_cpu_fetch_word(self, 1); return 24; } 

Byte ins_rst_00(GB_cpu *self) { GB_cpu_push_stack(self, self->registers.pc + 1); self->registers.pc = 0x00; return 16; }
Byte ins_rst_08(GB_cpu *self) { GB_cpu_push_stack(self, self->registers.pc + 1); self->registers.pc = 0x08; return 16; }
Byte ins_rst_10(GB_cpu *self) { GB_cpu_push_stack(self, self->registers.pc + 1); self->registers.pc = 0x10; return 16; }
Byte ins_rst_18(GB_cpu *self) { GB_cpu_push_stack(self, self->registers.pc + 1); self->registers.pc = 0x18; return 16; }
Byte ins_rst_20(GB_cpu *self) { GB_cpu_push_stack(self, self->registers.pc + 1); self->registers.pc = 0x20; return 16; }
Byte ins_rst_28(GB_cpu *self) { GB_cpu_push_stack(self, self->registers.pc + 1); self->registers.pc = 0x28; return 16; }
Byte ins_rst_30(GB_cpu *self) { GB_cpu_push_stack(self, self->registers.pc + 1); self->registers.pc = 0x30; return 16; }
Byte ins_rst_38(GB_cpu *self) { GB_cpu_push_stack(self, self->registers.pc + 1); self->registers.pc = 0x38; return 16; }

Byte ins_halt(GB_cpu *self) { 
    self->is_halted = true; 
    PC_INC(self, 1); 
    return 4; 
}

Byte ins_bit_a_0(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.a & 0x01) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_b_0(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.b & 0x01) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_c_0(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.c & 0x01) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_d_0(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.d & 0x01) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_e_0(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.e & 0x01) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_h_0(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.h & 0x01) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_l_0(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.l & 0x01) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_hl_0(GB_cpu *self) { self->registers.f = ZeroFlagValue(GB_mmu_read_byte(&self->memory, GB_register_get_HL(self)) & 0x01) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 16; }

Byte ins_bit_a_1(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.a & 0x02) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_b_1(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.b & 0x02) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_c_1(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.c & 0x02) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_d_1(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.d & 0x02) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_e_1(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.e & 0x02) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_h_1(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.h & 0x02) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_l_1(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.h & 0x02) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_hl_1(GB_cpu *self) { self->registers.f = ZeroFlagValue(GB_mmu_read_byte(&self->memory, GB_register_get_HL(self)) & 0x02) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 16; }

Byte ins_bit_a_2(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.a & 0x04) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_b_2(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.b & 0x04) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_c_2(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.c & 0x04) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_d_2(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.d & 0x04) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_e_2(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.e & 0x04) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_h_2(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.h & 0x04) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_l_2(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.h & 0x04) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_hl_2(GB_cpu *self) { self->registers.f = ZeroFlagValue(GB_mmu_read_byte(&self->memory, GB_register_get_HL(self)) & 0x04) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 16; }

Byte ins_bit_a_3(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.a & 0x08) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_b_3(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.b & 0x08) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_c_3(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.c & 0x08) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_d_3(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.d & 0x08) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_e_3(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.e & 0x08) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_h_3(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.h & 0x08) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_l_3(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.h & 0x08) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_hl_3(GB_cpu *self) { self->registers.f = ZeroFlagValue(GB_mmu_read_byte(&self->memory, GB_register_get_HL(self)) & 0x08) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 16; }

Byte ins_bit_a_4(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.a & 0x10) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_b_4(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.b & 0x10) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_c_4(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.c & 0x10) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_d_4(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.d & 0x10) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_e_4(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.e & 0x10) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_h_4(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.h & 0x10) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_l_4(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.l & 0x10) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_hl_4(GB_cpu *self) { self->registers.f = ZeroFlagValue(GB_mmu_read_byte(&self->memory, GB_register_get_HL(self)) & 0x10) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 16; }

Byte ins_bit_a_5(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.a & 0x20) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_b_5(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.b & 0x20) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_c_5(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.c & 0x20) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_d_5(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.d & 0x20) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_e_5(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.e & 0x20) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_h_5(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.h & 0x20) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_l_5(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.h & 0x20) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_hl_5(GB_cpu *self) { self->registers.f = ZeroFlagValue(GB_mmu_read_byte(&self->memory, GB_register_get_HL(self)) & 0x20) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 16; }

Byte ins_bit_a_6(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.a & 0x40) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_b_6(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.b & 0x40) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_c_6(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.c & 0x40) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_d_6(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.d & 0x40) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_e_6(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.e & 0x40) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_h_6(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.h & 0x40) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_l_6(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.l & 0x40) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_hl_6(GB_cpu *self) { self->registers.f = ZeroFlagValue(GB_mmu_read_byte(&self->memory, GB_register_get_HL(self)) & 0x40) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 16; }

Byte ins_bit_a_7(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.a & 0x80) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_b_7(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.b & 0x80) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_c_7(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.c & 0x80) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_d_7(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.d & 0x80) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_e_7(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.e & 0x80) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_h_7(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.h & 0x80) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_l_7(GB_cpu *self) { self->registers.f = ZeroFlagValue(self->registers.h & 0x80) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 8; }
Byte ins_bit_hl_7(GB_cpu *self) { self->registers.f = ZeroFlagValue(GB_mmu_read_byte(&self->memory, GB_register_get_HL(self) & 0x80)) | GB_cpu_get_carry_flag(self);  PC_INC(self, 2); return 16; }

Byte ins_res_a_0(GB_cpu *self) { self->registers.a = self->registers.a & 0xfe; PC_INC(self, 2); return 8; }
Byte ins_res_b_0(GB_cpu *self) { self->registers.b = self->registers.b & 0xfe; PC_INC(self, 2); return 8; }
Byte ins_res_c_0(GB_cpu *self) { self->registers.c = self->registers.c & 0xfe; PC_INC(self, 2); return 8; }
Byte ins_res_d_0(GB_cpu *self) { self->registers.d = self->registers.d & 0xfe; PC_INC(self, 2); return 8; }
Byte ins_res_e_0(GB_cpu *self) { self->registers.e = self->registers.e & 0xfe; PC_INC(self, 2); return 8; }
Byte ins_res_h_0(GB_cpu *self) { self->registers.h = self->registers.h & 0xfe; PC_INC(self, 2); return 8; }
Byte ins_res_l_0(GB_cpu *self) { self->registers.l = self->registers.l & 0xfe; PC_INC(self, 2); return 8; }
Byte ins_res_hl_0(GB_cpu *self) { GB_mmu_write_byte(&self->memory,GB_register_get_HL(self), GB_mmu_read_byte(&self->memory, GB_register_get_HL(self)) & 0xfe); PC_INC(self, 2); return 16; }

Byte ins_res_a_1(GB_cpu *self) { self->registers.a = self->registers.a & 0xfd; PC_INC(self, 2); return 8; }
Byte ins_res_b_1(GB_cpu *self) { self->registers.b = self->registers.b & 0xfd; PC_INC(self, 2); return 8; }
Byte ins_res_c_1(GB_cpu *self) { self->registers.c = self->registers.c & 0xfd; PC_INC(self, 2); return 8; }
Byte ins_res_d_1(GB_cpu *self) { self->registers.d = self->registers.d & 0xfd; PC_INC(self, 2); return 8; }
Byte ins_res_e_1(GB_cpu *self) { self->registers.e = self->registers.e & 0xfd; PC_INC(self, 2); return 8; }
Byte ins_res_h_1(GB_cpu *self) { self->registers.h = self->registers.h & 0xfd; PC_INC(self, 2); return 8; }
Byte ins_res_l_1(GB_cpu *self) { self->registers.l = self->registers.l & 0xfd; PC_INC(self, 2); return 8; }
Byte ins_res_hl_1(GB_cpu *self) { GB_mmu_write_byte(&self->memory,GB_register_get_HL(self), GB_mmu_read_byte(&self->memory, GB_register_get_HL(self)) & 0xfd); PC_INC(self, 2); return 16; }

Byte ins_res_a_2(GB_cpu *self) { self->registers.a = self->registers.a & 0xfb; PC_INC(self, 2); return 8; }
Byte ins_res_b_2(GB_cpu *self) { self->registers.b = self->registers.b & 0xfb; PC_INC(self, 2); return 8; }
Byte ins_res_c_2(GB_cpu *self) { self->registers.c = self->registers.c & 0xfb; PC_INC(self, 2); return 8; }
Byte ins_res_d_2(GB_cpu *self) { self->registers.d = self->registers.d & 0xfb; PC_INC(self, 2); return 8; }
Byte ins_res_e_2(GB_cpu *self) { self->registers.e = self->registers.e & 0xfb; PC_INC(self, 2); return 8; }
Byte ins_res_h_2(GB_cpu *self) { self->registers.h = self->registers.h & 0xfb; PC_INC(self, 2); return 8; }
Byte ins_res_l_2(GB_cpu *self) { self->registers.l = self->registers.l & 0xfb; PC_INC(self, 2); return 8; }
Byte ins_res_hl_2(GB_cpu *self) { GB_mmu_write_byte(&self->memory,GB_register_get_HL(self), GB_mmu_read_byte(&self->memory, GB_register_get_HL(self)) & 0xfb); PC_INC(self, 2); return 16; }

Byte ins_res_a_3(GB_cpu *self) { self->registers.a = self->registers.a & 0xf7; PC_INC(self, 2); return 8; }
Byte ins_res_b_3(GB_cpu *self) { self->registers.b = self->registers.b & 0xf7; PC_INC(self, 2); return 8; }
Byte ins_res_c_3(GB_cpu *self) { self->registers.c = self->registers.c & 0xf7; PC_INC(self, 2); return 8; }
Byte ins_res_d_3(GB_cpu *self) { self->registers.d = self->registers.d & 0xf7; PC_INC(self, 2); return 8; }
Byte ins_res_e_3(GB_cpu *self) { self->registers.e = self->registers.e & 0xf7; PC_INC(self, 2); return 8; }
Byte ins_res_h_3(GB_cpu *self) { self->registers.h = self->registers.h & 0xf7; PC_INC(self, 2); return 8; }
Byte ins_res_l_3(GB_cpu *self) { self->registers.l = self->registers.l & 0xf7; PC_INC(self, 2); return 8; }
Byte ins_res_hl_3(GB_cpu *self) { GB_mmu_write_byte(&self->memory,GB_register_get_HL(self), GB_mmu_read_byte(&self->memory, GB_register_get_HL(self)) & 0xf7); PC_INC(self, 2); return 16; }

Byte ins_res_a_4(GB_cpu *self) { self->registers.a = self->registers.a & 0xef; PC_INC(self, 2); return 8; }
Byte ins_res_b_4(GB_cpu *self) { self->registers.b = self->registers.b & 0xef; PC_INC(self, 2); return 8; }
Byte ins_res_c_4(GB_cpu *self) { self->registers.c = self->registers.c & 0xef; PC_INC(self, 2); return 8; }
Byte ins_res_d_4(GB_cpu *self) { self->registers.d = self->registers.d & 0xef; PC_INC(self, 2); return 8; }
Byte ins_res_e_4(GB_cpu *self) { self->registers.e = self->registers.e & 0xef; PC_INC(self, 2); return 8; }
Byte ins_res_h_4(GB_cpu *self) { self->registers.h = self->registers.h & 0xef; PC_INC(self, 2); return 8; }
Byte ins_res_l_4(GB_cpu *self) { self->registers.l = self->registers.l & 0xef; PC_INC(self, 2); return 8; }
Byte ins_res_hl_4(GB_cpu *self) { GB_mmu_write_byte(&self->memory,GB_register_get_HL(self), GB_mmu_read_byte(&self->memory, GB_register_get_HL(self)) & 0xef); PC_INC(self, 2); return 16; }

Byte ins_res_a_5(GB_cpu *self) { self->registers.a = self->registers.a & 0xdf; PC_INC(self, 2); return 8; }
Byte ins_res_b_5(GB_cpu *self) { self->registers.b = self->registers.b & 0xdf; PC_INC(self, 2); return 8; }
Byte ins_res_c_5(GB_cpu *self) { self->registers.c = self->registers.c & 0xdf; PC_INC(self, 2); return 8; }
Byte ins_res_d_5(GB_cpu *self) { self->registers.d = self->registers.d & 0xdf; PC_INC(self, 2); return 8; }
Byte ins_res_e_5(GB_cpu *self) { self->registers.e = self->registers.e & 0xdf; PC_INC(self, 2); return 8; }
Byte ins_res_h_5(GB_cpu *self) { self->registers.h = self->registers.h & 0xdf; PC_INC(self, 2); return 8; }
Byte ins_res_l_5(GB_cpu *self) { self->registers.l = self->registers.l & 0xdf; PC_INC(self, 2); return 8; }
Byte ins_res_hl_5(GB_cpu *self) { GB_mmu_write_byte(&self->memory,GB_register_get_HL(self), GB_mmu_read_byte(&self->memory, GB_register_get_HL(self)) & 0xdf); PC_INC(self, 2); return 16; }

Byte ins_res_a_6(GB_cpu *self) { self->registers.a = self->registers.a & 0xbf; PC_INC(self, 2); return 8; }
Byte ins_res_b_6(GB_cpu *self) { self->registers.b = self->registers.b & 0xbf; PC_INC(self, 2); return 8; }
Byte ins_res_c_6(GB_cpu *self) { self->registers.c = self->registers.c & 0xbf; PC_INC(self, 2); return 8; }
Byte ins_res_d_6(GB_cpu *self) { self->registers.d = self->registers.d & 0xbf; PC_INC(self, 2); return 8; }
Byte ins_res_e_6(GB_cpu *self) { self->registers.e = self->registers.e & 0xbf; PC_INC(self, 2); return 8; }
Byte ins_res_h_6(GB_cpu *self) { self->registers.h = self->registers.h & 0xbf; PC_INC(self, 2); return 8; }
Byte ins_res_l_6(GB_cpu *self) { self->registers.l = self->registers.l & 0xbf; PC_INC(self, 2); return 8; }
Byte ins_res_hl_6(GB_cpu *self) { GB_mmu_write_byte(&self->memory,GB_register_get_HL(self), GB_mmu_read_byte(&self->memory, GB_register_get_HL(self)) & 0xbf); PC_INC(self, 2); return 16; }

Byte ins_res_a_7(GB_cpu *self) { self->registers.a = self->registers.a & 0x7f; PC_INC(self, 2); return 8; }
Byte ins_res_b_7(GB_cpu *self) { self->registers.b = self->registers.b & 0x7f; PC_INC(self, 2); return 8; }
Byte ins_res_c_7(GB_cpu *self) { self->registers.c = self->registers.c & 0x7f; PC_INC(self, 2); return 8; }
Byte ins_res_d_7(GB_cpu *self) { self->registers.d = self->registers.d & 0x7f; PC_INC(self, 2); return 8; }
Byte ins_res_e_7(GB_cpu *self) { self->registers.e = self->registers.e & 0x7f; PC_INC(self, 2); return 8; }
Byte ins_res_h_7(GB_cpu *self) { self->registers.h = self->registers.h & 0x7f; PC_INC(self, 2); return 8; }
Byte ins_res_l_7(GB_cpu *self) { self->registers.l = self->registers.l & 0x7f; PC_INC(self, 2); return 8; }
Byte ins_res_hl_7(GB_cpu *self) { GB_mmu_write_byte(&self->memory,GB_register_get_HL(self), GB_mmu_read_byte(&self->memory, GB_register_get_HL(self)) & 0x7f); PC_INC(self, 2); return 16; }

Byte ins_set_a_0(GB_cpu *self) { self->registers.a = self->registers.a | 0x01; PC_INC(self, 2); return 8; }
Byte ins_set_b_0(GB_cpu *self) { self->registers.b = self->registers.b | 0x01; PC_INC(self, 2); return 8; }
Byte ins_set_c_0(GB_cpu *self) { self->registers.c = self->registers.c | 0x01; PC_INC(self, 2); return 8; }
Byte ins_set_d_0(GB_cpu *self) { self->registers.d = self->registers.d | 0x01; PC_INC(self, 2); return 8; }
Byte ins_set_e_0(GB_cpu *self) { self->registers.e = self->registers.e | 0x01; PC_INC(self, 2); return 8; }
Byte ins_set_h_0(GB_cpu *self) { self->registers.h = self->registers.h | 0x01; PC_INC(self, 2); return 8; }
Byte ins_set_l_0(GB_cpu *self) { self->registers.l = self->registers.l | 0x01; PC_INC(self, 2); return 8; }
Byte ins_set_hl_0(GB_cpu *self) { GB_mmu_write_byte(&self->memory,GB_register_get_HL(self), GB_mmu_read_byte(&self->memory, GB_register_get_HL(self)) | 0x01); PC_INC(self, 2); return 8; }

Byte ins_set_a_1(GB_cpu *self) { self->registers.a = self->registers.a | 0x02; PC_INC(self, 2); return 8; }
Byte ins_set_b_1(GB_cpu *self) { self->registers.b = self->registers.b | 0x02; PC_INC(self, 2); return 8; }
Byte ins_set_c_1(GB_cpu *self) { self->registers.c = self->registers.c | 0x02; PC_INC(self, 2); return 8; }
Byte ins_set_d_1(GB_cpu *self) { self->registers.d = self->registers.d | 0x02; PC_INC(self, 2); return 8; }
Byte ins_set_e_1(GB_cpu *self) { self->registers.e = self->registers.e | 0x02; PC_INC(self, 2); return 8; }
Byte ins_set_h_1(GB_cpu *self) { self->registers.h = self->registers.h | 0x02; PC_INC(self, 2); return 8; }
Byte ins_set_l_1(GB_cpu *self) { self->registers.l = self->registers.l | 0x02; PC_INC(self, 2); return 8; }
Byte ins_set_hl_1(GB_cpu *self) { GB_mmu_write_byte(&self->memory,GB_register_get_HL(self), GB_mmu_read_byte(&self->memory, GB_register_get_HL(self)) | 0x02); PC_INC(self, 2); return 8; }

Byte ins_set_a_2(GB_cpu *self) { self->registers.a = self->registers.a | 0x04; PC_INC(self, 2); return 8; }
Byte ins_set_b_2(GB_cpu *self) { self->registers.b = self->registers.b | 0x04; PC_INC(self, 2); return 8; }
Byte ins_set_c_2(GB_cpu *self) { self->registers.c = self->registers.c | 0x04; PC_INC(self, 2); return 8; }
Byte ins_set_d_2(GB_cpu *self) { self->registers.d = self->registers.d | 0x04; PC_INC(self, 2); return 8; }
Byte ins_set_e_2(GB_cpu *self) { self->registers.e = self->registers.e | 0x04; PC_INC(self, 2); return 8; }
Byte ins_set_h_2(GB_cpu *self) { self->registers.h = self->registers.h | 0x04; PC_INC(self, 2); return 8; }
Byte ins_set_l_2(GB_cpu *self) { self->registers.l = self->registers.l | 0x04; PC_INC(self, 2); return 8; }
Byte ins_set_hl_2(GB_cpu *self) { GB_mmu_write_byte(&self->memory,GB_register_get_HL(self), GB_mmu_read_byte(&self->memory, GB_register_get_HL(self)) | 0x04); PC_INC(self, 2); return 8; }

Byte ins_set_a_3(GB_cpu *self) { self->registers.a = self->registers.a | 0x08; PC_INC(self, 2); return 8; }
Byte ins_set_b_3(GB_cpu *self) { self->registers.b = self->registers.b | 0x08; PC_INC(self, 2); return 8; }
Byte ins_set_c_3(GB_cpu *self) { self->registers.c = self->registers.c | 0x08; PC_INC(self, 2); return 8; }
Byte ins_set_d_3(GB_cpu *self) { self->registers.d = self->registers.d | 0x08; PC_INC(self, 2); return 8; }
Byte ins_set_e_3(GB_cpu *self) { self->registers.e = self->registers.e | 0x08; PC_INC(self, 2); return 8; }
Byte ins_set_h_3(GB_cpu *self) { self->registers.h = self->registers.h | 0x08; PC_INC(self, 2); return 8; }
Byte ins_set_l_3(GB_cpu *self) { self->registers.l = self->registers.l | 0x08; PC_INC(self, 2); return 8; }
Byte ins_set_hl_3(GB_cpu *self) { GB_mmu_write_byte(&self->memory,GB_register_get_HL(self), GB_mmu_read_byte(&self->memory, GB_register_get_HL(self)) | 0x08); PC_INC(self, 2); return 8; }

Byte ins_set_a_4(GB_cpu *self) { self->registers.a = self->registers.a | 0x10; PC_INC(self, 2); return 8; }
Byte ins_set_b_4(GB_cpu *self) { self->registers.b = self->registers.b | 0x10; PC_INC(self, 2); return 8; }
Byte ins_set_c_4(GB_cpu *self) { self->registers.c = self->registers.c | 0x10; PC_INC(self, 2); return 8; }
Byte ins_set_d_4(GB_cpu *self) { self->registers.d = self->registers.d | 0x10; PC_INC(self, 2); return 8; }
Byte ins_set_e_4(GB_cpu *self) { self->registers.e = self->registers.e | 0x10; PC_INC(self, 2); return 8; }
Byte ins_set_h_4(GB_cpu *self) { self->registers.h = self->registers.h | 0x10; PC_INC(self, 2); return 8; }
Byte ins_set_l_4(GB_cpu *self) { self->registers.l = self->registers.l | 0x10; PC_INC(self, 2); return 8; }
Byte ins_set_hl_4(GB_cpu *self) { GB_mmu_write_byte(&self->memory,GB_register_get_HL(self), GB_mmu_read_byte(&self->memory, GB_register_get_HL(self)) | 0x10); PC_INC(self, 2); return 8; }

Byte ins_set_a_5(GB_cpu *self) { self->registers.a = self->registers.a | 0x20; PC_INC(self, 2); return 8; }
Byte ins_set_b_5(GB_cpu *self) { self->registers.b = self->registers.b | 0x20; PC_INC(self, 2); return 8; }
Byte ins_set_c_5(GB_cpu *self) { self->registers.c = self->registers.c | 0x20; PC_INC(self, 2); return 8; }
Byte ins_set_d_5(GB_cpu *self) { self->registers.d = self->registers.d | 0x20; PC_INC(self, 2); return 8; }
Byte ins_set_e_5(GB_cpu *self) { self->registers.e = self->registers.e | 0x20; PC_INC(self, 2); return 8; }
Byte ins_set_h_5(GB_cpu *self) { self->registers.h = self->registers.h | 0x20; PC_INC(self, 2); return 8; }
Byte ins_set_l_5(GB_cpu *self) { self->registers.l = self->registers.l | 0x20; PC_INC(self, 2); return 8; }
Byte ins_set_hl_5(GB_cpu *self) { GB_mmu_write_byte(&self->memory,GB_register_get_HL(self), GB_mmu_read_byte(&self->memory, GB_register_get_HL(self)) | 0x20); PC_INC(self, 2); return 8; }

Byte ins_set_a_6(GB_cpu *self) { self->registers.a = self->registers.a | 0x40; PC_INC(self, 2); return 8; }
Byte ins_set_b_6(GB_cpu *self) { self->registers.b = self->registers.b | 0x40; PC_INC(self, 2); return 8; }
Byte ins_set_c_6(GB_cpu *self) { self->registers.c = self->registers.c | 0x40; PC_INC(self, 2); return 8; }
Byte ins_set_d_6(GB_cpu *self) { self->registers.d = self->registers.d | 0x40; PC_INC(self, 2); return 8; }
Byte ins_set_e_6(GB_cpu *self) { self->registers.e = self->registers.e | 0x40; PC_INC(self, 2); return 8; }
Byte ins_set_h_6(GB_cpu *self) { self->registers.h = self->registers.h | 0x40; PC_INC(self, 2); return 8; }
Byte ins_set_l_6(GB_cpu *self) { self->registers.l = self->registers.l | 0x40; PC_INC(self, 2); return 8; }
Byte ins_set_hl_6(GB_cpu *self) { GB_mmu_write_byte(&self->memory,GB_register_get_HL(self), GB_mmu_read_byte(&self->memory, GB_register_get_HL(self)) | 0x40); PC_INC(self, 2); return 8; }

Byte ins_set_a_7(GB_cpu *self) { self->registers.a = self->registers.a | 0x80; PC_INC(self, 2); return 8; }
Byte ins_set_b_7(GB_cpu *self) { self->registers.b = self->registers.b | 0x80; PC_INC(self, 2); return 8; }
Byte ins_set_c_7(GB_cpu *self) { self->registers.c = self->registers.c | 0x80; PC_INC(self, 2); return 8; }
Byte ins_set_d_7(GB_cpu *self) { self->registers.d = self->registers.d | 0x80; PC_INC(self, 2); return 8; }
Byte ins_set_e_7(GB_cpu *self) { self->registers.e = self->registers.e | 0x80; PC_INC(self, 2); return 8; }
Byte ins_set_h_7(GB_cpu *self) { self->registers.h = self->registers.h | 0x80; PC_INC(self, 2); return 8; }
Byte ins_set_l_7(GB_cpu *self) { self->registers.l = self->registers.l | 0x80; PC_INC(self, 2); return 8; }
Byte ins_set_hl_7(GB_cpu *self) { GB_mmu_write_byte(&self->memory,GB_register_get_HL(self), GB_mmu_read_byte(&self->memory, GB_register_get_HL(self)) | 0x80); PC_INC(self, 2); return 8; }

typedef Byte (*ins_func)(GB_cpu*);

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

Byte ins_prefixCB(GB_cpu *self) { 
    Byte insCode = GB_cpu_fetch_byte(self, 1);
    //printf("executing instruction CB => 0x%02x; PC=0x%04x \n", insCode, self->registers.pc);
    Byte ticks = ins_CB_table[insCode](self); 
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

Byte GB_cpu_step(GB_cpu* cpu) {
    // TODO: Handle interrups
    if(cpu->is_halted == true) {
        // if the cpu is halted no operation can be performed exept interups
        if(cpu->memory.KEY1 == 0) {
            return 0;
        } else {
            cpu->is_halted = false;
            cpu->memory.KEY1 = 0;
        }
        
    }

    Byte ins_code = GB_cpu_fetch_byte(cpu, 0);
    ins_func insToExec = ins_table[ins_code];
    //printf("executing instruction 0x%02x; PC=0x%04x \n", ins_code, cpu->registers.pc);

    Byte cycles = (*insToExec)(cpu);

    // update DIV register
    cpu->divCounter += cycles;
    if(cpu->divCounter >= DIV_CLOCK_INC) { // TODO: Handle double speed
        cpu->divCounter = 0;
        cpu->memory.div++;
    }
    // Update TIMER counter if enabled
    if (cpu->memory.isTimaEnabled == true) {
        cpu->timaCounter += cycles;
        if (cpu->timaCounter >= GB_TIMA_clock_inc_value(cpu->memory.timaClockCycles)) {
            Byte oldTima = cpu->memory.tima;
            cpu->memory.tima++;
            if (oldTima > cpu->memory.tima) {
                // overflow
                cpu->memory.tima = cpu->memory.tma;
                //TODO: Request interrupt
            }
        }
    }

    return cycles;
}
