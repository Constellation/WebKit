#include "A64InstructionTable.h"
#include <stdio.h>

using namespace JSC::ARM64Disassembler;

void testMovImm(uint32_t opcode, const char* expected) {
    char buffer[256];
    uint32_t pc = 0;

    auto* entry = findInstruction(opcode);
    if (entry) {
        formatInstruction(entry, opcode, &pc, nullptr, nullptr, buffer, sizeof(buffer));
        printf("0x%08x: %s  (expected: %s)\n", opcode, buffer, expected);
    } else {
        printf("0x%08x: NOT FOUND (expected: %s)\n", opcode, expected);
    }
}

int main() {
    printf("Testing MOV immediate value extraction:\n\n");

    // Test various MOV immediate values
    testMovImm(0xd280014a, "mov x10, #0xa");      // mov x10, #10
    testMovImm(0xd2800060, "mov x0, #0x3");       // mov x0, #3
    testMovImm(0xd2800140, "mov x0, #0xa");       // mov x0, #10
    testMovImm(0xd2800020, "mov x0, #0x1");       // mov x0, #1
    testMovImm(0xd2800000, "mov x0, #0x0");       // mov x0, #0
    testMovImm(0xd28001e0, "mov x0, #0xf");       // mov x0, #15
    testMovImm(0xd2800400, "mov x0, #0x20");      // mov x0, #32

    printf("\n--- Manual bit extraction test ---\n");
    uint32_t test_opcode = 0xd280014a;
    printf("Opcode: 0x%08x\n", test_opcode);
    printf("Bits 5-20 (imm16): 0x%x\n", (test_opcode >> 5) & 0xFFFF);
    printf("Bits 0-4 (Rd): %u\n", test_opcode & 0x1F);
    printf("Bits 21-22 (hw): %u\n", (test_opcode >> 21) & 0x3);

    return 0;
}
