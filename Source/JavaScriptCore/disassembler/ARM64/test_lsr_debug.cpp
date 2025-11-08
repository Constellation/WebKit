#include "A64InstructionTable.h"
#include <cstdio>

using namespace JSC;

int main() {
    uint32_t opcode = 0x53017C00;  // LSR w0, w0, #1
    
    auto* entry = ARM64Disassembler::findInstruction(opcode);
    if (entry) {
        printf("Opcode: 0x%08x\n", opcode);
        printf("Name: %s\n", entry->name);
        printf("Mnemonic: %s\n", entry->mnemonic);
        printf("Operand count: %u\n", entry->operandCount);
    }
    
    return 0;
}
