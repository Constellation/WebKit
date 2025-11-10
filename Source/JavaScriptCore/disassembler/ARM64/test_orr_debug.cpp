#include "A64InstructionTable.h"
#include <stdio.h>

using namespace JSC::ARM64Disassembler;

int main() {
    uint32_t opcode = 0x0ea21c20;  // ORR Q=0
    uint32_t pc = 0;
    char buffer[256];

    auto* entry = findInstruction(opcode);
    if (entry) {
        printf("Matched instruction: %s\n", entry->mnemonic);
        printf("Operand offset: %u\n", entry->operandOffset);
        printf("Operand count: %u\n", entry->operandCount);

        formatInstruction(entry, opcode, &pc, nullptr, nullptr, buffer, sizeof(buffer));
        printf("Formatted: %s\n", buffer);
    } else {
        printf("No match found!\n");
    }

    return 0;
}
