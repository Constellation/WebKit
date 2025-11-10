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
        // ===== Bug 1: Logical Immediate Field Extraction =====
        { 0xb24003fa, "ORR/MOV x26, #0x1 (Bug 1: was #0x3)", "   mov      x26, #0x1", "Logical Imm" },
        { 0xb2400020, "ORR x0, x1, #0x1", "   orr      x0, x1, #0x1", "Logical Imm" },

        // ===== Bug 2: 64-bit Rotation in Logical Immediate =====
        { 0xb27c03e0, "ORR/MOV x0, #0x10 (Bug 2: was #0x0)", "   mov      x0, #0x10", "Logical Imm" },
        { 0xb27c0000, "ORR x0, x0, #0x10", "   orr      x0, x0, #0x10", "Logical Imm" },
        { 0xb24007e0, "ORR x0, xzr, #0x3", "   mov      x0, #0x3", "Logical Imm" },

        // ===== Bug 3: MOVN Immediate Display =====
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

        // ===== Bug 4: ADD/SUB/CMP Hex Formatting =====
        { 0x7100281f, "CMP w0, #10 (Bug 4: hex format)", "   cmp      w0, #0xa", "CMP" },
        { 0x7100fc1f, "CMP w0, #63", "   cmp      w0, #0x3f", "CMP" },
        { 0x710ffc1f, "CMP w0, #1023", "   cmp      w0, #0x3ff", "CMP" },
        { 0xf100281f, "CMP x0, #10", "   cmp      x0, #0xa", "CMP" },

        // ===== ADD immediate =====
        { 0x91000400, "ADD x0, x0, #1", "   add      x0, x0, #0x1", "ADD" },
        { 0x91002800, "ADD x0, x0, #10", "   add      x0, x0, #0xa", "ADD" },
        { 0x9103fc00, "ADD x0, x0, #255", "   add      x0, x0, #0xff", "ADD" },
        { 0x11000400, "ADD w0, w0, #1", "   add      w0, w0, #0x1", "ADD" },

        // ===== SUB immediate =====
        { 0xd1002800, "SUB x0, x0, #10", "   sub      x0, x0, #0xa", "SUB" },
        { 0xd1000400, "SUB x0, x0, #1", "   sub      x0, x0, #0x1", "SUB" },
        { 0x51000400, "SUB w0, w0, #1", "   sub      w0, w0, #0x1", "SUB" },

        // ===== ADD/SUB with shift =====
        { 0x91400000, "ADD x0, x0, #0, lsl #12", "   add      x0, x0, #0x0, lsl #12", "ADD Shift" },
        { 0x91400400, "ADD x0, x0, #1, lsl #12", "   add      x0, x0, #0x1, lsl #12", "ADD Shift" },
        { 0xd1400000, "SUB x0, x0, #0, lsl #12", "   sub      x0, x0, #0x0, lsl #12", "SUB Shift" },

        // ===== CMP with register and extend =====
        { 0xeb00001f, "CMP x0, x0", "   cmp      x0, x0", "CMP Reg" },
        { 0xeb01001f, "CMP x0, x1", "   cmp      x0, x1", "CMP Reg" },
        { 0x6b00001f, "CMP w0, w0", "   cmp      w0, w0", "CMP Reg" },
        { 0x6b01001f, "CMP w0, w1", "   cmp      w0, w1", "CMP Reg" },
        { 0xeb21001f, "CMP x0, w1, uxtb", "   cmp      x0, w1, uxtb", "CMP Reg" },
        { 0xeb21c01f, "CMP x0, w1, sxtw", "   cmp      x0, w1, sxtw", "CMP Reg" },

        // ===== Load/Store Addressing Modes =====
        { 0xf9400000, "LDR x0, [x0]", "   ldr      x0, [x0]", "Memory" },
        { 0xf9400020, "LDR x0, [x1]", "   ldr      x0, [x1]", "Memory" },
        { 0xf8408000, "LDUR x0, [x0, #8]", "   ldur     x0, [x0, #8]", "Memory" },
        { 0xf8450318, "LDUR x24, [x24, #80]", "   ldur     x24, [x24, #80]", "Memory" },
        { 0xf84003e0, "LDUR x0, [sp]", "   ldur     x0, [sp]", "Memory" },
        { 0xa9bf7bfd, "STP fp, lr, [sp, #-16]!", "   stp      fp, lr, [sp, #-16]!", "Memory" },
        { 0xa9bf7fff, "STP xzr, xzr, [sp, #-16]!", "   stp      xzr, xzr, [sp, #-16]!", "Memory" },

        // ===== SIMD Arrangements =====
        { 0x2e218400, "SUB v0.8b, v0.8b, v1.8b (Q=0)", "   sub      v0.8b, v0.8b, v1.8b", "SIMD" },
        { 0x6e218400, "SUB v0.16b, v0.16b, v1.16b (Q=1)", "   sub      v0.16b, v0.16b, v1.16b", "SIMD" },
        { 0x0ea1dc00, "FAMAX v0.2s, v0.2s, v1.2s", "   famax    v0.2s, v0.2s, v1.2s", "SIMD" },
        { 0x4ea1dc00, "FAMAX v0.4s, v0.4s, v1.4s", "   famax    v0.4s, v0.4s, v1.4s", "SIMD" },
        { 0x4ee1dc00, "FAMAX v0.2d, v0.2d, v1.2d", "   famax    v0.2d, v0.2d, v1.2d", "SIMD" },
        { 0x0ea21c20, "ORR v0.8b, v1.8b, v2.8b (Q=0)", "   orr      v0.8b, v1.8b, v2.8b", "SIMD" },
        { 0x4ea21c20, "ORR v0.16b, v1.16b, v2.16b (Q=1)", "   orr      v0.16b, v1.16b, v2.16b", "SIMD" },
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

        bool match = strcmp(buffer, tests[i].expected) == 0;

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
