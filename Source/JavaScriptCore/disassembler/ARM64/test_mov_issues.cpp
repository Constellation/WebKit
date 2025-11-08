#include "A64InstructionTable.h"
#include <stdio.h>
#include <stdint.h>

using namespace JSC::ARM64Disassembler;

int main() {
    printf("Testing MOV immediate formatting issues:\n\n");

    // Test 1: MOVZ x8, #0x8103 (the reported issue)
    uint32_t opcode1 = 0;
    opcode1 |= (1 << 31);      // sf=1
    opcode1 |= (0b10 << 29);   // opc=10 (MOVZ)
    opcode1 |= (0b100101 << 23);
    opcode1 |= (0 << 21);      // hw=00 
    opcode1 |= (0x8103 << 5);  // imm16=0x8103
    opcode1 |= (8 << 0);       // Rd=8

    printf("Test 1: MOVZ x8, #0x8103\n");
    printf("Opcode: 0x%08x\n", opcode1);
    printf("imm16 extracted: 0x%x\n", (opcode1 >> 5) & 0xFFFF);
    printf("hw extracted: %u\n", (opcode1 >> 21) & 0x3);

    char buffer1[256];
    uint32_t* pc1 = &opcode1;
    const InstructionEntry* entry1 = findInstruction(opcode1);
    formatInstruction(entry1, opcode1, pc1, nullptr, nullptr, buffer1, sizeof(buffer1));
    printf("Formatted: %s\n", buffer1);
    printf("Expected:  movz     x8, #0x8103\n\n");

    // Test 2: MOVK x8, #0x43b4, LSL #16
    uint32_t opcode2 = 0;
    opcode2 |= (1 << 31);      // sf=1
    opcode2 |= (0b11 << 29);   // opc=11 (MOVK)
    opcode2 |= (0b100101 << 23);
    opcode2 |= (1 << 21);      // hw=01 (shift 16)
    opcode2 |= (0x43b4 << 5);  // imm16=0x43b4
    opcode2 |= (8 << 0);       // Rd=8

    printf("Test 2: MOVK x8, #0x43b4, LSL #16\n");
    printf("Opcode: 0x%08x\n", opcode2);
    printf("imm16 extracted: 0x%x\n", (opcode2 >> 5) & 0xFFFF);
    printf("hw extracted: %u\n", (opcode2 >> 21) & 0x3);

    char buffer2[256];
    uint32_t* pc2 = &opcode2;
    const InstructionEntry* entry2 = findInstruction(opcode2);
    formatInstruction(entry2, opcode2, pc2, nullptr, nullptr, buffer2, sizeof(buffer2));
    printf("Formatted: %s\n", buffer2);
    printf("Expected:  movk     x8, #0x43b4, lsl #16\n");

    return 0;
}
