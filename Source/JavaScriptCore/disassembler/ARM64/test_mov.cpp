#include "A64InstructionTable.h"
#include <stdio.h>
#include <stdint.h>

using namespace JSC::ARM64Disassembler;

int main() {
    printf("Testing MOV/MOVZ instruction formatting:\n\n");

    // MOVZ x8, #0x8103
    // Format: sf=1, opc=10, hw=00, imm16=0x8103
    // Bits: 31(sf), 30-29(opc), 28-23(100101), 22-21(hw), 20-5(imm16), 4-0(Rd)
    
    uint32_t opcode = 0;
    opcode |= (1 << 31);      // sf=1 (64-bit)
    opcode |= (0b10 << 29);   // opc=10 (MOVZ)
    opcode |= (0b100101 << 23); // Fixed bits
    opcode |= (0 << 21);      // hw=00 (no shift)
    opcode |= (0x8103 << 5);  // imm16=0x8103
    opcode |= (8 << 0);       // Rd=8 (x8)

    printf("Testing MOVZ x8, #0x8103:\n");
    printf("Opcode: 0x%08x\n\n", opcode);

    uint32_t* pc = &opcode;
    const InstructionEntry* entry = findInstruction(opcode);

    if (entry) {
        printf("Found instruction: %s\n", entry->name);
        printf("Mnemonic: %s\n", entry->mnemonic);
        printf("Operand count: %u\n\n", entry->operandCount);

        // Show operand details
        for (unsigned i = 0; i < entry->operandCount; i++) {
            const auto& op = g_operandTable[entry->operandOffset + i];
            printf("Operand %u: type=%u, start=%u, width=%u, start2=%u, width2=%u\n",
                   i, op.type, op.field1_start, op.field1_width, op.field2_start, op.field2_width);
        }
    } else {
        printf("Instruction not found\!\n");
    }

    char buffer[256];
    formatInstruction(entry, opcode, pc, nullptr, nullptr, buffer, sizeof(buffer));

    printf("\nFormatted: %s\n", buffer);
    printf("Expected:  movz     x8, #0x8103\n");

    // Extract fields manually
    printf("\nManual field extraction:\n");
    printf("Rd (bits 0-4):    %u (x%u)\n", (opcode >> 0) & 0x1f, (opcode >> 0) & 0x1f);
    printf("imm16 (bits 5-20): 0x%x (#0x%x)\n", (opcode >> 5) & 0xffff, (opcode >> 5) & 0xffff);
    printf("hw (bits 21-22):   %u (shift: %u)\n", (opcode >> 21) & 0x3, ((opcode >> 21) & 0x3) * 16);
    printf("opc (bits 29-30):  %u\n", (opcode >> 29) & 0x3);
    printf("sf (bit 31):       %u\n", (opcode >> 31) & 0x1);

    return 0;
}
