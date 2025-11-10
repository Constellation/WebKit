#include "A64InstructionTable.h"
#include <stdio.h>

using namespace JSC::ARM64Disassembler;

int main() {
    char buffer[256];
    uint32_t pc = 0;

    // MOV (immediate) is an alias of MOVZ or ORR
    // Test: mov x10, #0xa (MOVZ encoding)
    // MOVZ: sf=1, opc=10, hw=00, imm16=0x000a, Rd=10
    uint32_t movz_x10_a = 0b11010010100000000000000101001010;  // sf | opc | 100101 | hw | imm16 | Rd

    printf("Test opcode: 0x%08x\n", movz_x10_a);
    printf("Binary: ");
    for (int i = 31; i >= 0; i--) {
        printf("%d", (movz_x10_a >> i) & 1);
        if (i % 4 == 0) printf(" ");
    }
    printf("\n\n");

    auto* entry = findInstruction(movz_x10_a);
    if (entry) {
        printf("Matched: %s\n", entry->mnemonic);
        printf("Operand offset: %u, count: %u\n", entry->operandOffset, entry->operandCount);

        formatInstruction(entry, movz_x10_a, &pc, nullptr, nullptr, buffer, sizeof(buffer));
        printf("Disassembled: %s\n", buffer);
        printf("Expected:     mov      x10, #0xa\n");

        // Manual extraction to verify
        uint32_t imm16_manual = (movz_x10_a >> 5) & 0xFFFF;
        uint32_t hw_manual = (movz_x10_a >> 21) & 0x3;
        printf("\nManual extraction:\n");
        printf("  imm16: 0x%x (%u)\n", imm16_manual, imm16_manual);
        printf("  hw: %u (LSL #%u)\n", hw_manual, hw_manual * 16);
    } else {
        printf("No match found!\n");
    }

    // Also test the actual encoding
    printf("\n--- Bit field extraction ---\n");
    printf("imm16 field (bits 5-20): 0x%x\n", (movz_x10_a >> 5) & 0xFFFF);
    printf("Rd field (bits 0-4): %u\n", movz_x10_a & 0x1F);

    return 0;
}
