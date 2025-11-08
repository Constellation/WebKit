#include "A64InstructionTable.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

using namespace JSC::ARM64Disassembler;

int main() {
    // Test CASAL instruction
    // CASAL_C64_comswap: size=11, L=1, o0=1
    // Format: CASAL <Xs>, <Xt>, [<Xn|SP>]
    // Encoding: 11 001 0 0 0 1 1 1 Rs(16-20) 1 Rn(5-9) Rt(0-4)

    // Build opcode: CASAL x3, x1, [x2]
    // Rs=3 (bits 16-20), Rt=1 (bits 0-4), Rn=2 (bits 5-9)
    // size=11 (bits 30-31), L=1 (bit 22), o0=1 (bit 15)

    uint32_t opcode = 0;
    opcode |= (0b11 << 30);  // size=11
    opcode |= (0b00100 << 25); // Fixed bits
    opcode |= (1 << 23);     // Fixed bit
    opcode |= (1 << 22);     // L=1
    opcode |= (1 << 21);     // Fixed bit
    opcode |= (3 << 16);     // Rs=3 (x3)
    opcode |= (1 << 15);     // o0=1
    opcode |= (0b11111 << 10); // Fixed bits
    opcode |= (2 << 5);      // Rn=2 (x2)
    opcode |= (1 << 0);      // Rt=1 (x1)

    printf("Testing CASAL instruction:\n");
    printf("Opcode: 0x%08x\n\n", opcode);

    uint32_t* pc = &opcode;
    const InstructionEntry* entry = findInstruction(opcode);

    if (entry) {
        printf("Found instruction: %s\n", entry->name);
        printf("Mnemonic: %s\n", entry->mnemonic);
        printf("Operand count: %u\n", entry->operandCount);
        printf("Operand offset: %u\n\n", entry->operandOffset);

        // Show operand details
        for (unsigned i = 0; i < entry->operandCount; i++) {
            const auto& op = g_operandTable[entry->operandOffset + i];
            printf("Operand %u: type=%u, start=%u, width=%u, start2=%u, width2=%u\n",
                   i, op.type, op.field1_start, op.field1_width, op.field2_start, op.field2_width);
        }
    } else {
        printf("Instruction not found!\n");
    }

    char buffer[256];
    formatInstruction(entry, opcode, pc, nullptr, nullptr, buffer, sizeof(buffer));

    printf("\nFormatted: %s\n", buffer);
    printf("Expected:     casal    x3, x1, [x2]\n");

    // Extract fields manually
    printf("\nManual field extraction:\n");
    printf("Rs (bits 16-20): %u (x%u)\n", (opcode >> 16) & 0x1f, (opcode >> 16) & 0x1f);
    printf("Rt (bits 0-4):   %u (x%u)\n", (opcode >> 0) & 0x1f, (opcode >> 0) & 0x1f);
    printf("Rn (bits 5-9):   %u (x%u)\n", (opcode >> 5) & 0x1f, (opcode >> 5) & 0x1f);

    return 0;
}
