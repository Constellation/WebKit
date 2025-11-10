#include "A64InstructionTable.h"
#include <stdio.h>

using namespace JSC::ARM64Disassembler;

int main() {
    uint32_t opcode = 0x4ee1dc00;
    
    printf("Testing opcode 0x%08x\n", opcode);
    printf("Looking for matching entries:\n\n");
    
    // Manually search through the table
    for (int i = 0; i < g_instructionTableSize; i++) {
        const auto& entry = g_instructionTable[i];
        if ((opcode & entry.mask) == entry.pattern) {
            printf("Match %d: %s (mask=0x%08x, pattern=0x%08x)\n",
                   i, entry.mnemonic, entry.mask, entry.pattern);
            
            char buf[256];
            uint32_t pc = 0;
            formatInstruction(&entry, opcode, &pc, nullptr, nullptr, buf, sizeof(buf));
            printf("  Result: %s\n\n", buf);
            
            if (i < 5) continue; // Show first few matches
            else break;  // Stop after showing some
        }
    }
    
    return 0;
}
