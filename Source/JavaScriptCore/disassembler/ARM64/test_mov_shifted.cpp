#include "A64InstructionTable.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

using namespace JSC::ARM64Disassembler;

int main() {
    printf("Testing MOV with shifted immediates:\n\n");

    // Test 1: MOVZ x8, #0x8103, LSL #0 (hw=0)
    uint32_t opcode1 = 0;
    opcode1 |= (1 << 31);      // sf=1
    opcode1 |= (0b10 << 29);   // opc=10 (MOVZ)
    opcode1 |= (0b100101 << 23);
    opcode1 |= (0 << 21);      // hw=00 (shift 0)
    opcode1 |= (0x8103 << 5);  // imm16=0x8103
    opcode1 |= (8 << 0);       // Rd=8

    // Test 2: MOVZ x9, #0x1234, LSL #16 (hw=1)
    uint32_t opcode2 = 0;
    opcode2 |= (1 << 31);      // sf=1
    opcode2 |= (0b10 << 29);   // opc=10 (MOVZ)
    opcode2 |= (0b100101 << 23);
    opcode2 |= (1 << 21);      // hw=01 (shift 16)
    opcode2 |= (0x1234 << 5);  // imm16=0x1234
    opcode2 |= (9 << 0);       // Rd=9

    // Test 3: MOVZ x10, #0xabcd, LSL #32 (hw=2)
    uint32_t opcode3 = 0;
    opcode3 |= (1 << 31);      // sf=1
    opcode3 |= (0b10 << 29);   // opc=10 (MOVZ)
    opcode3 |= (0b100101 << 23);
    opcode3 |= (2 << 21);      // hw=10 (shift 32)
    opcode3 |= (0xabcd << 5);  // imm16=0xabcd
    opcode3 |= (10 << 0);      // Rd=10

    struct TestCase {
        uint32_t opcode;
        const char* expected;
        const char* description;
    } tests[] = {
        { opcode1, "mov      x8, #0x8103", "MOV x8, #0x8103 (no shift)" },
        { opcode2, "mov      x9, #0x1234, lsl #16", "MOV x9, #0x1234, LSL #16" },
        { opcode3, "mov      x10, #0xabcd, lsl #32", "MOV x10, #0xabcd, LSL #32" },
    };

    int passed = 0;
    int failed = 0;

    for (const auto& test : tests) {
        uint32_t* pc = (uint32_t*)&test.opcode;
        const InstructionEntry* entry = findInstruction(test.opcode);

        char buffer[256];
        formatInstruction(entry, test.opcode, pc, nullptr, nullptr, buffer, sizeof(buffer));

        // Trim leading spaces for comparison
        const char* formatted = buffer;
        while (*formatted == ' ') formatted++;

        bool matches = (strcmp(formatted, test.expected) == 0);

        printf("%s\n", test.description);
        printf("  Formatted: %s\n", buffer);
        printf("  Expected:  %s\n", test.expected);
        printf("  Status: %s\n\n", matches ? "✅ PASS" : "❌ FAIL");

        if (matches) passed++;
        else failed++;
    }

    printf("==================\n");
    printf("Results: %d passed, %d failed\n", passed, failed);

    return (failed == 0) ? 0 : 1;
}
