#include "A64InstructionTable.h"
#include <cstdio>
#include <cstring>

using namespace JSC;

int main() {
    char buffer[256];
    uint32_t pc = 0;

    printf("Testing opcode 0x0f10a401\n\n");

    uint32_t opcode = 0x0F10A401;
    memset(buffer, 0, sizeof(buffer));

    auto* entry = ARM64Disassembler::findInstruction(opcode);
    if (entry) {
        printf("Found instruction: %s\n", entry->name);
        printf("Mnemonic: %s\n", entry->mnemonic);
        printf("Operand count: %u\n", entry->operandCount);
        printf("Operand offset: %u\n\n", entry->operandOffset);

        ARM64Disassembler::formatInstruction(entry, opcode, &pc, &pc, &pc, buffer, sizeof(buffer));
        printf("Formatted: %s\n\n", buffer);

        // Show operands
        const ARM64Disassembler::OperandDesc* operands = &ARM64Disassembler::g_operandTable[entry->operandOffset];
        for (int i = 0; i < entry->operandCount; i++) {
            printf("Operand %d: type=%u, f1_start=%u, f1_width=%u, f2_start=%u, f2_width=%u\n",
                   i, operands[i].type, operands[i].field1_start, operands[i].field1_width,
                   operands[i].field2_start, operands[i].field2_width);
        }

        // Decode bit fields
        printf("\nBit field analysis:\n");
        printf("Bits 31-30 (op0): %u\n", (opcode >> 30) & 0x3);
        printf("Bits 29-25: %u\n", (opcode >> 25) & 0x1F);
        printf("Bits 24-21 (op1): %u\n", (opcode >> 21) & 0xF);
        printf("Bits 20-16 (Rm): %u\n", (opcode >> 16) & 0x1F);
        printf("Bits 15-10 (op2): %u\n", (opcode >> 10) & 0x3F);
        printf("Bits 9-5 (Rn): %u\n", (opcode >> 5) & 0x1F);
        printf("Bits 4-0 (Rd): %u\n", opcode & 0x1F);

    } else {
        printf("Instruction NOT FOUND\n");
    }

    return 0;
}
