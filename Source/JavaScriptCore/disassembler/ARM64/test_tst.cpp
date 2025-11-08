#include "A64InstructionTable.h"
#include <stdio.h>
#include <stdint.h>

using namespace JSC::ARM64Disassembler;

int main() {
    printf("Testing TST instruction with logical immediate:\n\n");

    // TST w2, #0xf (ANDS wzr, w2, #0xf)
    // This is an alias for ANDS with destination = wzr
    // Logical immediate #0xf = 0b1111 (4 ones)
    // For 32-bit: N=0, immr=0, imms=3 (4 bits set)
    uint32_t opcode1 = 0;
    opcode1 |= (0 << 31);       // sf=0 (32-bit)
    opcode1 |= (0b11 << 29);    // opc=11 (ANDS)
    opcode1 |= (0b100100 << 23); // Fixed bits
    opcode1 |= (0 << 22);       // N=0
    opcode1 |= (0 << 16);       // immr=0
    opcode1 |= (3 << 10);       // imms=3 (4 bits)
    opcode1 |= (2 << 5);        // Rn=2 (w2)
    opcode1 |= (31 << 0);       // Rd=31 (wzr - makes it TST)

    printf("Test 1: TST w2, #0xf\n");
    printf("Opcode: 0x%08x\n", opcode1);
    printf("N bit: %u\n", (opcode1 >> 22) & 1);
    printf("immr: %u\n", (opcode1 >> 16) & 0x3f);
    printf("imms: %u\n", (opcode1 >> 10) & 0x3f);
    printf("Rn: %u\n", (opcode1 >> 5) & 0x1f);
    printf("Rd: %u\n", (opcode1 >> 0) & 0x1f);

    char buffer1[256];
    uint32_t* pc1 = &opcode1;
    const InstructionEntry* entry1 = findInstruction(opcode1);

    if (entry1) {
        printf("\nFound instruction: %s\n", entry1->name);
        printf("Operand count: %u\n", entry1->operandCount);

        // Show operand details
        for (unsigned i = 0; i < entry1->operandCount; i++) {
            const auto& op = g_operandTable[entry1->operandOffset + i];
            printf("Operand %u: type=%u, start=%u, width=%u, start2=%u, width2=%u\n",
                   i, op.type, op.field1_start, op.field1_width, op.field2_start, op.field2_width);
        }
    }

    formatInstruction(entry1, opcode1, pc1, nullptr, nullptr, buffer1, sizeof(buffer1));
    printf("\nFormatted: %s\n", buffer1);
    printf("Expected:  tst      w2, #0xf\n\n");

    return 0;
}
