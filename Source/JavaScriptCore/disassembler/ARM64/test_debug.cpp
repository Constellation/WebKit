#include "A64InstructionTable.h"
#include <stdio.h>

using namespace JSC::ARM64Disassembler;

int main() {
    uint32_t opcode = 0x1e602000;
    const auto* entry = findInstruction(opcode);
    
    if (entry) {
        printf("Found instruction: %s\n", entry->mnemonic);
        printf("  Mask:    0x%08x\n", entry->mask);
        printf("  Pattern: 0x%08x\n", entry->pattern);
        printf("  Opcode:  0x%08x\n", opcode);
        printf("  Match:   0x%08x (should equal pattern)\n", opcode & entry->mask);
        printf("  Operand count: %d\n", entry->operandCount);
        printf("  Operand offset: %d\n", entry->operandOffset);
    } else {
        printf("No instruction found\!\n");
    }
    
    return 0;
}
