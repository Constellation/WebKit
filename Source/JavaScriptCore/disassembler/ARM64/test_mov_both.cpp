#include "A64InstructionTable.h"
#include <stdio.h>

using namespace JSC::ARM64Disassembler;

static inline uint32_t extractBits(uint32_t value, unsigned start, unsigned width) {
    return (value >> start) & ((1U << width) - 1);
}

int main() {
    uint32_t opcodes[] = {
        0xd280003a,  // mov x26, #0x1
        0xd280014a   // mov x10, #0xa
    };

    const char* expected[] = {
        "mov x26, #0x1",
        "mov x10, #0xa"
    };

    for (int i = 0; i < 2; i++) {
        uint32_t opcode = opcodes[i];
        printf("Testing opcode: 0x%08x\n", opcode);
        printf("  imm16 (bits 5-20): 0x%x\n", extractBits(opcode, 5, 16));
        printf("  hw (bits 21-22):   %u\n", extractBits(opcode, 21, 2));

        auto* entry = findInstruction(opcode);
        if (entry) {
            char buffer[256];
            uint32_t pc = 0;
            formatInstruction(entry, opcode, &pc, nullptr, nullptr, buffer, sizeof(buffer));
            printf("  Matched: %s\n", entry->mnemonic);
            printf("  Output:   %s\n", buffer);
            printf("  Expected: %s\n", expected[i]);
        }
        printf("\n");
    }

    return 0;
}
