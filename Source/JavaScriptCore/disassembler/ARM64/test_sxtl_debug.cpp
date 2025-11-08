#include "A64InstructionTable.h"
#include <cstdio>

using namespace JSC;

int main() {
    // SXTL (Q=0)
    uint32_t opcode1 = 0x0f08a401;
    auto* entry1 = ARM64Disassembler::findInstruction(opcode1);
    if (entry1) {
        printf("SXTL (Q=0): 0x%08x\n", opcode1);
        printf("  Name: %s\n", entry1->name);
        printf("  Mnemonic: %s\n", entry1->mnemonic);
        printf("  Flags: 0x%02x (bit 0=%d, bit 1=%d)\n", 
               entry1->flags, 
               (entry1->flags & 1), 
               (entry1->flags & 2) >> 1);
    } else {
        printf("SXTL (Q=0) NOT FOUND\n");
    }
    
    // SXTL2 (Q=1)
    uint32_t opcode2 = 0x4f08a401;
    auto* entry2 = ARM64Disassembler::findInstruction(opcode2);
    if (entry2) {
        printf("\nSXTL2 (Q=1): 0x%08x\n", opcode2);
        printf("  Name: %s\n", entry2->name);
        printf("  Mnemonic: %s\n", entry2->mnemonic);
        printf("  Flags: 0x%02x (bit 0=%d, bit 1=%d)\n", 
               entry2->flags, 
               (entry2->flags & 1), 
               (entry2->flags & 2) >> 1);
    } else {
        printf("SXTL2 (Q=1) NOT FOUND\n");
    }
    
    // Test formatting
    char buffer[256];
    uint32_t pc = 0;
    
    if (entry1) {
        ARM64Disassembler::formatInstruction(entry1, opcode1, &pc, &pc, &pc, buffer, sizeof(buffer));
        printf("\nFormatted (Q=0): %s\n", buffer);
    }
    
    if (entry2) {
        ARM64Disassembler::formatInstruction(entry2, opcode2, &pc, &pc, &pc, buffer, sizeof(buffer));
        printf("Formatted (Q=1): %s\n", buffer);
    }
    
    return 0;
}
