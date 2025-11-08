#include "A64InstructionTable.h"
#include <stdio.h>
#include <stdint.h>

using namespace JSC::ARM64Disassembler;

int main() {
    printf("Testing LDRB with register offset:\n\n");

    // LDRB w2, [x22, w20, UXTW]
    // Load byte with register offset and extend
    // Format: size=00 (8-bit), V=0, opc=01 (LDRB)
    // option=010 (UXTW), S=0 (no shift)

    uint32_t opcode1 = 0;
    opcode1 |= (0b00 << 30);     // size=00 (8-bit)
    opcode1 |= (0b111 << 27);    // Fixed bits
    opcode1 |= (0 << 26);        // V=0 (GP register)
    opcode1 |= (0b00 << 24);     // Fixed bits
    opcode1 |= (0b01 << 22);     // opc=01 (LDRB unsigned)
    opcode1 |= (1 << 21);        // Fixed bit
    opcode1 |= (20 << 16);       // Rm=20 (w20)
    opcode1 |= (0b010 << 13);    // option=010 (UXTW)
    opcode1 |= (0 << 12);        // S=0 (no shift)
    opcode1 |= (0b10 << 10);     // Fixed bits
    opcode1 |= (22 << 5);        // Rn=22 (x22)
    opcode1 |= (2 << 0);         // Rt=2 (w2)

    printf("Test 1: LDRB w2, [x22, w20, UXTW]\n");
    printf("Opcode: 0x%08x\n", opcode1);
    printf("Rt (bits 0-4): %u\n", (opcode1 >> 0) & 0x1f);
    printf("Rn (bits 5-9): %u\n", (opcode1 >> 5) & 0x1f);
    printf("Rm (bits 16-20): %u\n", (opcode1 >> 16) & 0x1f);
    printf("option (bits 13-15): %u (UXTW=010)\n", (opcode1 >> 13) & 0x7);
    printf("S (bit 12): %u\n", (opcode1 >> 12) & 1);

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
    } else {
        printf("\nInstruction not found!\n");
    }

    formatInstruction(entry1, opcode1, pc1, nullptr, nullptr, buffer1, sizeof(buffer1));
    printf("\nFormatted: %s\n", buffer1);
    printf("Expected:  ldrb     w2, [x22, w20, uxtw]\n");

    return 0;
}
