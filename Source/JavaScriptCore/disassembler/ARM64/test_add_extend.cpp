#include "A64InstructionTable.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

using namespace JSC::ARM64Disassembler;

int main() {
    // Test ADD instruction with extend
    // ADD_64_addsub_ext: ADD <Xd|SP>, <Xn|SP>, <Wm>{, <extend> {#<amount>}}
    // Format: ADD x2, x22, w2, uxtw

    // Build opcode: ADD x2, x22, w2, uxtw #0
    // Rd=2 (bits 0-4), Rn=22 (bits 5-9), Rm=2 (bits 16-20)
    // option=010 (UXTW, bits 13-15), imm3=0 (bits 10-12)
    // sf=1 (bit 31), op=0 (bit 30), S=0 (bit 29)

    uint32_t opcode = 0;
    opcode |= (1 << 31);     // sf=1 (64-bit)
    opcode |= (0 << 30);     // op=0 (ADD, not SUB)
    opcode |= (0 << 29);     // S=0 (no flags)
    opcode |= (0b01011 << 24); // Fixed bits
    opcode |= (0b00 << 22);  // Fixed bits
    opcode |= (1 << 21);     // Fixed bit
    opcode |= (2 << 16);     // Rm=2 (w2)
    opcode |= (0b010 << 13); // option=010 (UXTW)
    opcode |= (0 << 10);     // imm3=0 (shift amount)
    opcode |= (22 << 5);     // Rn=22 (x22)
    opcode |= (2 << 0);      // Rd=2 (x2)

    printf("Testing ADD with extend instruction:\n");
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
    printf("Expected:     add      x2, x22, w2, uxtw\n");

    // Extract fields manually
    printf("\nManual field extraction:\n");
    printf("Rd (bits 0-4):    %u (x%u)\n", (opcode >> 0) & 0x1f, (opcode >> 0) & 0x1f);
    printf("Rn (bits 5-9):    %u (x%u)\n", (opcode >> 5) & 0x1f, (opcode >> 5) & 0x1f);
    printf("Rm (bits 16-20):  %u (w%u)\n", (opcode >> 16) & 0x1f, (opcode >> 16) & 0x1f);
    printf("imm3 (bits 10-12): %u\n", (opcode >> 10) & 0x7);
    printf("option (bits 13-15): %u (", (opcode >> 13) & 0x7);

    const char* extendNames[] = { "uxtb", "uxth", "uxtw", "uxtx", "sxtb", "sxth", "sxtw", "sxtx" };
    printf("%s)\n", extendNames[(opcode >> 13) & 0x7]);

    return 0;
}
