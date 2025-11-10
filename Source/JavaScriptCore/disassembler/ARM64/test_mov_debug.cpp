#include "A64InstructionTable.h"
#include <stdio.h>

using namespace JSC::ARM64Disassembler;

extern const uint8_t g_operandIndices[];
extern const OperandDesc g_operandTable[];

static inline uint32_t extractBits(uint32_t value, unsigned start, unsigned width) {
    return (value >> start) & ((1U << width) - 1);
}

int main() {
    char buffer[256];
    uint32_t pc = 0;

    uint32_t opcode = 0xd280003a;  // mov x26, #0x1

    printf("Debugging opcode 0x%08x\n\n", opcode);

    auto* entry = findInstruction(opcode);
    if (\!entry) {
        printf("No match\!\n");
        return 1;
    }

    printf("Matched: %s\n", entry->mnemonic);
    printf("Operand offset: %u, count: %u\n\n", entry->operandOffset, entry->operandCount);

    printf("Operand descriptors:\n");
    for (unsigned i = 0; i < entry->operandCount; i++) {
        uint8_t opIdx = g_operandIndices[entry->operandOffset + i];
        const auto& op = g_operandTable[opIdx];
        printf("  [%u] type=%u, field1=%u (start=%u, width=%u), field2=%u (start=%u, width=%u)\n",
               i, op.type, op.subtype, op.field1Start, op.field1Width, op.field2Val, op.field2Start, op.field2Width);

        if (op.field1Width > 0 && op.field1Start < 32) {
            uint32_t val = extractBits(opcode, op.field1Start, op.field1Width);
            printf("      field1 extracted value: 0x%x (%u)\n", val, val);
        }
        if (op.field2Width > 0 && op.field2Start < 32) {
            uint32_t val = extractBits(opcode, op.field2Start, op.field2Width);
            printf("      field2 extracted value: 0x%x (%u)\n", val, val);
        }
    }

    printf("\nDisassembly:\n");
    formatInstruction(entry, opcode, &pc, nullptr, nullptr, buffer, sizeof(buffer));
    printf("%s\n", buffer);

    return 0;
}
