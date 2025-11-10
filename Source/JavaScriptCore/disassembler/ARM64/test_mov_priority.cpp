#include "A64InstructionTable.h"
#include <stdio.h>

using namespace JSC::ARM64Disassembler;

// Helper to extract bits
static inline uint32_t extractBits(uint32_t value, unsigned start, unsigned width) {
    return (value >> start) & ((1U << width) - 1);
}

int main() {
    uint32_t opcode = 0xd280003a;  // mov x26, #0x1

    printf("Testing opcode: 0x%08x (mov x26, #0x1)\n\n", opcode);

    // Extract fields manually
    printf("Manual field extraction:\n");
    printf("  Rd (bits 0-4):     %u\n", extractBits(opcode, 0, 5));
    printf("  imm16 (bits 5-20): 0x%x\n", extractBits(opcode, 5, 16));
    printf("  hw (bits 21-22):   %u\n", extractBits(opcode, 21, 2));
    printf("  opc (bits 29-30):  %u\n", extractBits(opcode, 29, 2));
    printf("  sf (bit 31):       %u\n\n", extractBits(opcode, 31, 1));

    // Find instruction
    auto* entry = findInstruction(opcode);
    if (entry) {
        printf("Matched instruction: %s\n", entry->mnemonic);
        printf("  Mask:    0x%08x\n", entry->mask);
        printf("  Pattern: 0x%08x\n", entry->pattern);
        printf("  Operand offset: %u\n", entry->operandOffset);
        printf("  Operand count: %u\n\n", entry->operandCount);

        // Disassemble
        char buffer[256];
        uint32_t pc = 0;
        formatInstruction(entry, opcode, &pc, nullptr, nullptr, buffer, sizeof(buffer));
        printf("Disassembly: %s\n", buffer);
        printf("Expected:    mov      x26, #0x1\n");
    } else {
        printf("No match found!\n");
    }

    return 0;
}
