#include "A64InstructionTable.h"
#include <stdio.h>

using namespace JSC::ARM64Disassembler;

// Helper to extract bits
static inline uint32_t extractBits(uint32_t value, unsigned start, unsigned width) {
    return (value >> start) & ((1U << width) - 1);
}

int main() {
    char buffer[256];
    uint32_t pc = 0;

    // Assemble: mov x26, #0x1
    // This should be: MOVZ x26, #1, lsl #0
    // sf=1, opc=10, hw=00, imm16=0x0001, Rd=26
    uint32_t opcode = 0xd280003a;  // From assembler

    printf("Testing: mov x26, #0x1\n");
    printf("Opcode: 0x%08x\n\n", opcode);

    printf("Bit extraction:\n");
    printf("  Rd (bits 0-4):     %u (expected: 26)\n", extractBits(opcode, 0, 5));
    printf("  imm16 (bits 5-20): 0x%x (expected: 0x1)\n", extractBits(opcode, 5, 16));
    printf("  hw (bits 21-22):   %u (expected: 0)\n", extractBits(opcode, 21, 2));
    printf("  opc (bits 29-30):  %u (expected: 2 for MOVZ)\n", extractBits(opcode, 29, 2));
    printf("  sf (bit 31):       %u (expected: 1 for 64-bit)\n", extractBits(opcode, 31, 1));

    printf("\nDisassembly:\n");
    auto* entry = findInstruction(opcode);
    if (entry) {
        printf("Matched: %s (offset=%u, count=%u)\n", entry->mnemonic, entry->operandOffset, entry->operandCount);
        formatInstruction(entry, opcode, &pc, nullptr, nullptr, buffer, sizeof(buffer));
        printf("Output:   %s\n", buffer);
        printf("Expected: mov      x26, #0x1\n");
    } else {
        printf("No match found\!\n");
    }

    return 0;
}
