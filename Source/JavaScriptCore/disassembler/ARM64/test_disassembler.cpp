#include "A64InstructionTable.h"
#include <stdio.h>
#include <string.h>

using namespace JSC::ARM64Disassembler;

struct TestCase {
    uint32_t opcode;
    const char* description;
    const char* expected;
    const char* category;
};

int main() {
    TestCase tests[] = {
        // ===== Logical Immediate (Bug 1 & 2) =====
        { 0xb24003fa, "ORR/MOV x26, #0x1 (Bug 1: was #0x3)", "   mov      x26, #0x1", "Logical Imm" },
        { 0xb2400020, "ORR x0, x1, #0x1", "   orr      x0, x1, #0x1", "Logical Imm" },
        { 0xb27c03e0, "ORR/MOV x0, #0x10 (Bug 2: was #0x0)", "   mov      x0, #0x10", "Logical Imm" },
        { 0xb27c0000, "ORR x0, x0, #0x10", "   orr      x0, x0, #0x10", "Logical Imm" },
        { 0xb24007e0, "ORR x0, xzr, #0x3", "   mov      x0, #0x3", "Logical Imm" },

        // ===== MOVN (Bug 3) =====
        { 0x92800002, "MOVN x2, #0 -> #-1 (Bug 3: was #0x0)", "   mov      x2, #-1", "MOVN" },
        { 0x92800020, "MOVN x0, #1 -> #-2", "   mov      x0, #-2", "MOVN" },
        { 0x92800040, "MOVN x0, #2 -> #-3", "   mov      x0, #-3", "MOVN" },
        { 0x928001e0, "MOVN x0, #15 -> #-16", "   mov      x0, #-16", "MOVN" },
        { 0x12800000, "MOVN w0, #0 -> #-1", "   mov      w0, #-1", "MOVN" },

        // ===== MOVZ/MOV (wide immediate) =====
        { 0xd2800020, "MOVZ x0, #1", "   mov      x0, #0x1", "MOVZ" },
        { 0xd280003a, "MOVZ x26, #1", "   mov      x26, #0x1", "MOVZ" },
        { 0xd280014a, "MOVZ x10, #10", "   mov      x10, #0xa", "MOVZ" },
        { 0xd2800060, "MOVZ x0, #3", "   mov      x0, #0x3", "MOVZ" },
        { 0xd28001e0, "MOVZ x0, #15", "   mov      x0, #0xf", "MOVZ" },
        { 0x52800060, "MOVZ w0, #3", "   mov      w0, #0x3", "MOVZ" },
        { 0x52800140, "MOVZ w0, #10", "   mov      w0, #0xa", "MOVZ" },

        // ===== MOVZ with shift =====
        { 0xd2a0014a, "MOVZ x10, #10, lsl #16", "   mov      x10, #0xa, lsl #16", "MOVZ Shift" },
        { 0xd2c0016b, "MOVZ x11, #11, lsl #32", "   mov      x11, #0xb, lsl #32", "MOVZ Shift" },
        { 0xd2e0018c, "MOVZ x12, #12, lsl #48", "   mov      x12, #0xc, lsl #48", "MOVZ Shift" },

        // ===== CMP (Bug 4: Hex formatting) =====
        { 0x7100281f, "CMP w0, #10 (Bug 4: hex format)", "   cmp      w0, #0xa", "CMP" },
        { 0x7100fc1f, "CMP w0, #63", "   cmp      w0, #0x3f", "CMP" },
        { 0x710ffc1f, "CMP w0, #1023", "   cmp      w0, #0x3ff", "CMP" },
        { 0xf100281f, "CMP x0, #10", "   cmp      x0, #0xa", "CMP" },
        { 0xf100001f, "CMP x0, #0 (no shift)", "   cmp      x0, #0x0", "CMP" },
        { 0xf100041f, "CMP x0, #1", "   cmp      x0, #0x1", "CMP" },
        { 0x7100001f, "CMP w0, #0", "   cmp      w0, #0x0", "CMP" },
        { 0xf140001f, "CMP x0, #0, lsl #12", "   cmp      x0, #0x0, lsl #12", "CMP" },
        { 0xf140041f, "CMP x0, #1, lsl #12", "   cmp      x0, #0x1, lsl #12", "CMP" },

        // ===== CMP (Register) =====
        { 0xeb01001f, "CMP x0, x1", "   cmp      x0, x1", "CMP Reg" },
        { 0x6b01001f, "CMP w0, w1", "   cmp      w0, w1", "CMP Reg" },
        { 0xeb01101f, "CMP x0, x1, lsl #4", "   cmp      x0, x1, lsl #4", "CMP Reg" },
        { 0xeb41101f, "CMP x0, x1, lsr #4", "   cmp      x0, x1, lsr #4", "CMP Reg" },
        { 0xeb81101f, "CMP x0, x1, asr #4", "   cmp      x0, x1, asr #4", "CMP Reg" },
        { 0x6b01101f, "CMP w0, w1, lsl #4", "   cmp      w0, w1, lsl #4", "CMP Reg" },
        { 0xeb21001f, "CMP x0, w1, uxtb", "   cmp      x0, w1, uxtb", "CMP Reg" },
        { 0xeb21c01f, "CMP x0, w1, sxtw", "   cmp      x0, w1, sxtw", "CMP Reg" },
        { 0xeb21c81f, "CMP x0, w1, sxtw #2", "   cmp      x0, w1, sxtw #2", "CMP Reg" },

        // ===== ADD (Immediate) =====
        { 0x91000400, "ADD x0, x0, #1", "   add      x0, x0, #0x1", "ADD" },
        { 0x91002800, "ADD x0, x0, #10", "   add      x0, x0, #0xa", "ADD" },
        { 0x9103fc00, "ADD x0, x0, #255", "   add      x0, x0, #0xff", "ADD" },
        { 0x11000400, "ADD w0, w0, #1", "   add      w0, w0, #0x1", "ADD" },
        { 0x91000020, "ADD x0, x1, #0 (aliased to MOV)", "   mov      x0, x1", "ADD" },
        { 0x91000420, "ADD x0, x1, #1", "   add      x0, x1, #0x1", "ADD" },

        // ===== ADD (with shift) =====
        { 0x91400000, "ADD x0, x0, #0, lsl #12", "   add      x0, x0, #0x0, lsl #12", "ADD Shift" },
        { 0x91400400, "ADD x0, x0, #1, lsl #12", "   add      x0, x0, #0x1, lsl #12", "ADD Shift" },
        { 0x91400020, "ADD x0, x1, #0, lsl #12", "   add      x0, x1, #0x0, lsl #12", "ADD Shift" },
        { 0x91400420, "ADD x0, x1, #1, lsl #12", "   add      x0, x1, #0x1, lsl #12", "ADD Shift" },

        // ===== ADD (with extend) =====
        { 0x8b020ac2, "ADD x2, x22, x2, lsl #2", "   add      x2, x22, x2, lsl #2", "ADD Extend" },
        { 0x0b000824, "ADD w4, w1, w0, lsl #2", "   add      w4, w1, w0, lsl #2", "ADD Extend" },

        // ===== ADDS (set flags) =====
        { 0xb1000020, "ADDS x0, x1, #0", "   adds     x0, x1, #0x0", "ADD Flags" },
        { 0xb1000420, "ADDS x0, x1, #1", "   adds     x0, x1, #0x1", "ADD Flags" },
        { 0xb1400020, "ADDS x0, x1, #0, lsl #12", "   adds     x0, x1, #0x0, lsl #12", "ADD Flags" },
        { 0xb1400420, "ADDS x0, x1, #1, lsl #12", "   adds     x0, x1, #0x1, lsl #12", "ADD Flags" },

        // ===== SUB (Immediate) =====
        { 0xd1002800, "SUB x0, x0, #10", "   sub      x0, x0, #0xa", "SUB" },
        { 0xd1000400, "SUB x0, x0, #1", "   sub      x0, x0, #0x1", "SUB" },
        { 0x51000400, "SUB w0, w0, #1", "   sub      w0, w0, #0x1", "SUB" },
        { 0xd1000020, "SUB x0, x1, #0", "   sub      x0, x1, #0x0", "SUB" },
        { 0xd1000420, "SUB x0, x1, #1", "   sub      x0, x1, #0x1", "SUB" },

        // ===== SUB (with shift) =====
        { 0xd1400000, "SUB x0, x0, #0, lsl #12", "   sub      x0, x0, #0x0, lsl #12", "SUB Shift" },
        { 0xd1400020, "SUB x0, x1, #0, lsl #12", "   sub      x0, x1, #0x0, lsl #12", "SUB Shift" },
        { 0xd1400420, "SUB x0, x1, #1, lsl #12", "   sub      x0, x1, #0x1, lsl #12", "SUB Shift" },

        // ===== SUBS (set flags) =====
        { 0xf1000020, "SUBS x0, x1, #0", "   subs     x0, x1, #0x0", "SUB Flags" },
        { 0xf1000420, "SUBS x0, x1, #1", "   subs     x0, x1, #0x1", "SUB Flags" },
        { 0xf1400020, "SUBS x0, x1, #0, lsl #12", "   subs     x0, x1, #0x0, lsl #12", "SUB Flags" },
        { 0xf1400420, "SUBS x0, x1, #1, lsl #12", "   subs     x0, x1, #0x1, lsl #12", "SUB Flags" },

        // ===== Shift Operations =====
        { 0xd37af400, "LSL x0, x0, #6", "   lsl      x0, x0, #6", "Shift" },
        { 0x531e7400, "LSL w0, w0, #2", "   lsl      w0, w0, #2", "Shift" },
        { 0xd35ffc00, "LSR x0, x0, #31", "   lsr      x0, x0, #31", "Shift" },
        { 0x13017c00, "ASR w0, w0, #1", "   asr      w0, w0, #1", "Shift" },

        // ===== TST (Test bits) =====
        { 0x72000c5f, "TST w2, #0xf", "   tst      w2, #0xf", "TST" },

        // ===== TBNZ (Test and branch) =====
        { 0x37280042, "TBNZ w2, #5", "   tbnz     w2, #5, ", "TBNZ" },

        // ===== CASAL (Atomic) =====
        { 0xc8e3fc41, "CASAL x3, x1, [x2]", "   casal    x3, x1, [x2]", "Atomic" },

        // ===== Memory Operations =====
        { 0xf9400000, "LDR x0, [x0]", "   ldr      x0, [x0]", "Memory" },
        { 0xf9400020, "LDR x0, [x1]", "   ldr      x0, [x1]", "Memory" },
        { 0xf8408000, "LDUR x0, [x0, #8]", "   ldur     x0, [x0, #8]", "Memory" },
        { 0xf8450318, "LDUR x24, [x24, #80]", "   ldur     x24, [x24, #80]", "Memory" },
        { 0xf84003e0, "LDUR x0, [sp]", "   ldur     x0, [sp]", "Memory" },
        { 0xa9bf7bfd, "STP fp, lr, [sp, #-16]!", "   stp      fp, lr, [sp, #-16]!", "Memory" },
        { 0xa9bf7fff, "STP xzr, xzr, [sp, #-16]!", "   stp      xzr, xzr, [sp, #-16]!", "Memory" },
        { 0xa8c17bfd, "LDP fp, lr, [sp], #16", "   ldp      fp, lr, [sp], #16", "Memory" },
        { 0xa9007fe0, "STP x0, xzr, [sp]", "   stp      x0, xzr, [sp]", "Memory" },
        { 0xa9017fe0, "STP x0, xzr, [sp, #16]", "   stp      x0, xzr, [sp, #16]", "Memory" },
        { 0xa9407fe0, "LDP x0, xzr, [sp]", "   ldp      x0, xzr, [sp]", "Memory" },
        { 0xa9417fe0, "LDP x0, xzr, [sp, #16]", "   ldp      x0, xzr, [sp, #16]", "Memory" },
        { 0x29007fe0, "STP w0, wzr, [sp]", "   stp      w0, wzr, [sp]", "Memory" },
        { 0x29017fe0, "STP w0, wzr, [sp, #8]", "   stp      w0, wzr, [sp, #8]", "Memory" },

        // ===== LDR/STR with register offset =====
        { 0xb8626820, "LDR w0, [x1, x2]", "   ldr      w0, [x1, x2]", "Memory Reg" },
        { 0xf8626820, "LDR x0, [x1, x2]", "   ldr      x0, [x1, x2]", "Memory Reg" },
        { 0xb8627820, "LDR w0, [x1, x2, lsl #2]", "   ldr      w0, [x1, x2, lsl #2]", "Memory Reg" },
        { 0xf8627820, "LDR x0, [x1, x2, lsl #3]", "   ldr      x0, [x1, x2, lsl #3]", "Memory Reg" },
        { 0xb8226820, "STR w0, [x1, x2]", "   str      w0, [x1, x2]", "Memory Reg" },
        { 0xf8226820, "STR x0, [x1, x2]", "   str      x0, [x1, x2]", "Memory Reg" },
        { 0xb8227820, "STR w0, [x1, x2, lsl #2]", "   str      w0, [x1, x2, lsl #2]", "Memory Reg" },
        { 0xf8227820, "STR x0, [x1, x2, lsl #3]", "   str      x0, [x1, x2, lsl #3]", "Memory Reg" },
        { 0xb8624820, "LDR w0, [x1, w2, uxtw]", "   ldr      w0, [x1, w2, uxtw]", "Memory Reg" },
        { 0xb862c820, "LDR w0, [x1, w2, sxtw]", "   ldr      w0, [x1, w2, sxtw]", "Memory Reg" },
        { 0xb8224820, "STR w0, [x1, w2, uxtw]", "   str      w0, [x1, w2, uxtw]", "Memory Reg" },
        { 0xb822d820, "STR w0, [x1, w2, sxtw #2]", "   str      w0, [x1, w2, sxtw #2]", "Memory Reg" },

        // ===== Floating Point =====
        { 0x1e240020, "FCVTAS w0, s1", "   fcvtas   w0, s1", "FP Convert" },
        { 0x9e640062, "FCVTAS x2, d3", "   fcvtas   x2, d3", "FP Convert" },
        { 0x1e201008, "FMOV s8, #2.0", "   fmov     s8, #2.0", "FP Move" },
        { 0x1e6e1000, "FMOV d0, #1.0", "   fmov     d0, #1.0", "FP Move" },

        // ===== SIMD ADD (All arrangements) =====
        { 0x0e228420, "ADD v0.8b, v1.8b, v2.8b", "   add      v0.8b, v1.8b, v2.8b", "SIMD Add" },
        { 0x4e228420, "ADD v0.16b, v1.16b, v2.16b", "   add      v0.16b, v1.16b, v2.16b", "SIMD Add" },
        { 0x0e628420, "ADD v0.4h, v1.4h, v2.4h", "   add      v0.4h, v1.4h, v2.4h", "SIMD Add" },
        { 0x4e628420, "ADD v0.8h, v1.8h, v2.8h", "   add      v0.8h, v1.8h, v2.8h", "SIMD Add" },
        { 0x0ea28420, "ADD v0.2s, v1.2s, v2.2s", "   add      v0.2s, v1.2s, v2.2s", "SIMD Add" },
        { 0x4ea28420, "ADD v0.4s, v1.4s, v2.4s", "   add      v0.4s, v1.4s, v2.4s", "SIMD Add" },
        { 0x4ee28420, "ADD v0.2d, v1.2d, v2.2d", "   add      v0.2d, v1.2d, v2.2d", "SIMD Add" },

        // ===== SIMD SUB =====
        { 0x2e218400, "SUB v0.8b, v0.8b, v1.8b", "   sub      v0.8b, v0.8b, v1.8b", "SIMD Sub" },
        { 0x6e218400, "SUB v0.16b, v0.16b, v1.16b", "   sub      v0.16b, v0.16b, v1.16b", "SIMD Sub" },

        // ===== SIMD MUL =====
        { 0x0e229c20, "MUL v0.8b, v1.8b, v2.8b", "   mul      v0.8b, v1.8b, v2.8b", "SIMD Mul" },
        { 0x4e229c20, "MUL v0.16b, v1.16b, v2.16b", "   mul      v0.16b, v1.16b, v2.16b", "SIMD Mul" },
        { 0x0e629c20, "MUL v0.4h, v1.4h, v2.4h", "   mul      v0.4h, v1.4h, v2.4h", "SIMD Mul" },
        { 0x4e629c20, "MUL v0.8h, v1.8h, v2.8h", "   mul      v0.8h, v1.8h, v2.8h", "SIMD Mul" },
        { 0x0ea29c20, "MUL v0.2s, v1.2s, v2.2s", "   mul      v0.2s, v1.2s, v2.2s", "SIMD Mul" },
        { 0x4ea29c20, "MUL v0.4s, v1.4s, v2.4s", "   mul      v0.4s, v1.4s, v2.4s", "SIMD Mul" },

        // ===== SIMD Logical =====
        { 0x0e201c00, "AND v0.8b, v0.8b, v0.8b", "   and      v0.8b, v0.8b, v0.8b", "SIMD Logic" },
        { 0x4e201c00, "AND v0.16b, v0.16b, v0.16b", "   and      v0.16b, v0.16b, v0.16b", "SIMD Logic" },
        { 0x0ea21c20, "ORR v0.8b, v1.8b, v2.8b", "   orr      v0.8b, v1.8b, v2.8b", "SIMD Logic" },
        { 0x4ea21c20, "ORR v0.16b, v1.16b, v2.16b", "   orr      v0.16b, v1.16b, v2.16b", "SIMD Logic" },
        { 0x2e201c00, "EOR v0.8b, v0.8b, v0.8b", "   eor      v0.8b, v0.8b, v0.8b", "SIMD Logic" },
        { 0x6e201c00, "EOR v0.16b, v0.16b, v0.16b", "   eor      v0.16b, v0.16b, v0.16b", "SIMD Logic" },

        // ===== SIMD FP Operations =====
        { 0x0ea1dc00, "FAMAX v0.2s, v0.2s, v1.2s", "   famax    v0.2s, v0.2s, v1.2s", "SIMD FP" },
        { 0x4ea1dc00, "FAMAX v0.4s, v0.4s, v1.4s", "   famax    v0.4s, v0.4s, v1.4s", "SIMD FP" },
        { 0x4ee1dc00, "FAMAX v0.2d, v0.2d, v1.2d", "   famax    v0.2d, v0.2d, v1.2d", "SIMD FP" },
        { 0x0ec01c00, "FAMAX v0.4h, v0.4h, v0.4h", "   famax    v0.4h, v0.4h, v0.4h", "SIMD FP" },
        { 0x4ec01c00, "FAMAX v0.8h, v0.8h, v0.8h", "   famax    v0.8h, v0.8h, v0.8h", "SIMD FP" },
        { 0x2e401c00, "FMUL v0.4h, v0.4h, v0.4h", "   fmul     v0.4h, v0.4h, v0.4h", "SIMD FP" },
        { 0x6e401c00, "FMUL v0.8h, v0.8h, v0.8h", "   fmul     v0.8h, v0.8h, v0.8h", "SIMD FP" },
        { 0x2e20dc00, "FMUL v0.2s, v0.2s, v0.2s", "   fmul     v0.2s, v0.2s, v0.2s", "SIMD FP" },
        { 0x6e20dc00, "FMUL v0.4s, v0.4s, v0.4s", "   fmul     v0.4s, v0.4s, v0.4s", "SIMD FP" },
        { 0x6e60dc00, "FMUL v0.2d, v0.2d, v0.2d", "   fmul     v0.2d, v0.2d, v0.2d", "SIMD FP" },
        { 0x6e23dc41, "FMUL v1.4s, v2.4s, v3.4s", "   fmul     v1.4s, v2.4s, v3.4s", "SIMD FP" },

        // ===== DUP (Duplicate Element) =====
        { 0x0e010420, "DUP v0.8b, v1.b[0]", "   dup      v0.8b, v1.b[0]", "DUP" },
        { 0x4e010420, "DUP v0.16b, v1.b[0]", "   dup      v0.16b, v1.b[0]", "DUP" },
        { 0x0e020420, "DUP v0.4h, v1.h[0]", "   dup      v0.4h, v1.h[0]", "DUP" },
        { 0x4e020420, "DUP v0.8h, v1.h[0]", "   dup      v0.8h, v1.h[0]", "DUP" },
        { 0x0e040420, "DUP v0.2s, v1.s[0]", "   dup      v0.2s, v1.s[0]", "DUP" },
        { 0x4e040420, "DUP v0.4s, v1.s[0]", "   dup      v0.4s, v1.s[0]", "DUP" },
        { 0x0e080420, "DUP v0.1d, v1.d[0]", "   dup      v0.1d, v1.d[0]", "DUP" },
        { 0x4e080420, "DUP v0.2d, v1.d[0]", "   dup      v0.2d, v1.d[0]", "DUP" },

        // ===== UMOV (Unsigned Move) =====
        { 0x0e013c00, "UMOV w0, v0.b[0]", "   umov     w0, v0.b[0]", "UMOV" },
        { 0x0e023c00, "UMOV w0, v0.h[0]", "   umov     w0, v0.h[0]", "UMOV" },
        { 0x0e043c00, "UMOV w0, v0.s[0] (aliased to MOV)", "   mov      w0, v0.s[0]", "UMOV" },
        { 0x4e083c00, "UMOV x0, v0.d[0] (aliased to MOV)", "   mov      x0, v0.d[0]", "UMOV" },
        { 0x4e183c00, "UMOV x0, v0.d[1] (aliased to MOV)", "   mov      x0, v0.d[1]", "UMOV" },

        // ===== INS (Insert Element) =====
        { 0x4e011c01, "INS v1.b[0], w0", "   ins      v1.b[0], w0", "INS" },
        { 0x4e021c01, "INS v1.h[0], w0", "   ins      v1.h[0], w0", "INS" },
        { 0x4e041c01, "INS v1.s[0], w0", "   ins      v1.s[0], w0", "INS" },
        { 0x4e081c01, "INS v1.d[0], x0", "   ins      v1.d[0], x0", "INS" },

        // ===== MOV (Element) - These are INS aliases =====
        { 0x6e044401, "MOV v1.s[0], v0.s[0] (alias of INS)", "   ins      v1.s[0], v0.s[0]", "MOV Elem" },
        { 0x6e084401, "MOV v1.d[0], v0.d[0] (alias of INS)", "   ins      v1.d[0], v0.d[0]", "MOV Elem" },
        { 0x6e014401, "MOV v1.b[0], v0.b[0] (alias of INS)", "   ins      v1.b[0], v0.b[0]", "MOV Elem" },
        { 0x6e024401, "MOV v1.h[0], v0.h[0] (alias of INS)", "   ins      v1.h[0], v0.h[0]", "MOV Elem" },
        { 0x6e184c21, "MOV v1.d[1], v1.d[1] (alias of INS)", "   ins      v1.d[1], v1.d[1]", "MOV Elem" },

        // ===== SXTL/SXTL2 (Sign Extend Long) =====
        { 0x0f08a401, "SXTL v1.8h, v0.8b", "   sxtl     v1.8h, v0.8b", "SXTL" },
        { 0x4f08a401, "SXTL2 v1.8h, v0.16b", "   sxtl2    v1.8h, v0.16b", "SXTL" },
        { 0x0f10a401, "SXTL v1.4s, v0.4h", "   sxtl     v1.4s, v0.4h", "SXTL" },
        { 0x4f10a401, "SXTL2 v1.4s, v0.8h", "   sxtl2    v1.4s, v0.8h", "SXTL" },
        { 0x0f20a401, "SXTL v1.2d, v0.2s", "   sxtl     v1.2d, v0.2s", "SXTL" },
        { 0x4f20a401, "SXTL2 v1.2d, v0.4s", "   sxtl2    v1.2d, v0.4s", "SXTL" },

        // ===== UXTL/UXTL2 (Unsigned Extend Long) =====
        { 0x2f08a401, "UXTL v1.8h, v0.8b", "   uxtl     v1.8h, v0.8b", "UXTL" },
        { 0x6f08a401, "UXTL2 v1.8h, v0.16b", "   uxtl2    v1.8h, v0.16b", "UXTL" },
        { 0x2f10a401, "UXTL v1.4s, v0.4h", "   uxtl     v1.4s, v0.4h", "UXTL" },
        { 0x6f10a401, "UXTL2 v1.4s, v0.8h", "   uxtl2    v1.4s, v0.8h", "UXTL" },
        { 0x2f20a401, "UXTL v1.2d, v0.2s", "   uxtl     v1.2d, v0.2s", "UXTL" },
        { 0x6f20a401, "UXTL2 v1.2d, v0.4s", "   uxtl2    v1.2d, v0.4s", "UXTL" },

        // ===== TBL (Table Lookup) =====
        { 0x0e000000, "TBL v0.8b, {v0.16b}, v0.8b", "   tbl      v0.8b, { v0.16b }, v0.8b", "TBL" },
        { 0x4e000000, "TBL v0.16b, {v0.16b}, v0.16b", "   tbl      v0.16b, { v0.16b }, v0.16b", "TBL" },
        { 0x0e010001, "TBL v1.8b, {v0.16b}, v1.8b", "   tbl      v1.8b, { v0.16b }, v1.8b", "TBL" },

        // ===== TBX (Table Extension) =====
        { 0x0e001000, "TBX v0.8b, {v0.16b}, v0.8b", "   tbx      v0.8b, { v0.16b }, v0.8b", "TBX" },
        { 0x4e001000, "TBX v0.16b, {v0.16b}, v0.16b", "   tbx      v0.16b, { v0.16b }, v0.16b", "TBX" },

        // ===== EXT (Extract) =====
        { 0x2e000000, "EXT v0.8b, v0.8b, v0.8b, #0", "   ext      v0.8b, v0.8b, v0.8b, #0x0", "EXT" },
        { 0x6e000000, "EXT v0.16b, v0.16b, v0.16b, #0", "   ext      v0.16b, v0.16b, v0.16b, #0x0", "EXT" },

        // ===== LD1 (Load Multiple Structures) =====
        { 0x0c407020, "LD1 { v0.8b }, [x1]", "   ld1      { v0.8b }, [x1]", "LD1" },
        { 0x4c407020, "LD1 { v0.16b }, [x1]", "   ld1      { v0.16b }, [x1]", "LD1" },
        { 0x0c407462, "LD1 { v2.4h }, [x3]", "   ld1      { v2.4h }, [x3]", "LD1" },
        { 0x4c407462, "LD1 { v2.8h }, [x3]", "   ld1      { v2.8h }, [x3]", "LD1" },
        { 0x0c407945, "LD1 { v5.2s }, [x10]", "   ld1      { v5.2s }, [x10]", "LD1" },
        { 0x4c407945, "LD1 { v5.4s }, [x10]", "   ld1      { v5.4s }, [x10]", "LD1" },
        { 0x0c407e87, "LD1 { v7.1d }, [x20]", "   ld1      { v7.1d }, [x20]", "LD1" },
        { 0x4c407e87, "LD1 { v7.2d }, [x20]", "   ld1      { v7.2d }, [x20]", "LD1" },
    };

    int total = sizeof(tests) / sizeof(tests[0]);
    int passed = 0;
    int failed = 0;

    printf("ARM64 Disassembler Unified Test Suite\n");
    printf("======================================\n");
    printf("Total tests: %d\n\n", total);

    const char* lastCategory = "";

    for (int i = 0; i < total; i++) {
        if (strcmp(tests[i].category, lastCategory) != 0) {
            printf("\n--- %s Tests ---\n", tests[i].category);
            lastCategory = tests[i].category;
        }

        auto* entry = findInstruction(tests[i].opcode);
        if (!entry) {
            printf("✗ FAIL: %s\n", tests[i].description);
            printf("  ERROR: No instruction match for 0x%08x\n", tests[i].opcode);
            failed++;
            continue;
        }

        char buffer[256];
        uint32_t pc = 0;
        formatInstruction(entry, tests[i].opcode, &pc, nullptr, nullptr, buffer, sizeof(buffer));

        // For branch instructions, just check prefix (address varies)
        bool match;
        if (strstr(tests[i].category, "TBNZ") || strstr(tests[i].category, "Branch")) {
            match = strncmp(buffer, tests[i].expected, strlen(tests[i].expected)) == 0;
        } else {
            match = strcmp(buffer, tests[i].expected) == 0;
        }

        if (match) {
            printf("✓ PASS: %s\n", tests[i].description);
            passed++;
        } else {
            printf("✗ FAIL: %s\n", tests[i].description);
            printf("  Opcode:   0x%08x\n", tests[i].opcode);
            printf("  Expected: %s\n", tests[i].expected);
            printf("  Got:      %s\n", buffer);
            failed++;
        }
    }

    printf("\n======================================\n");
    printf("Results: %d/%d tests passed", passed, total);
    if (failed > 0) {
        printf(" (%d FAILED)", failed);
    }
    printf("\n");

    return failed == 0 ? 0 : 1;
}
