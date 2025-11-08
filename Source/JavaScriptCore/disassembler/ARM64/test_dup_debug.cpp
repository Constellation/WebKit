#include "A64InstructionTable.h"
#include <cstdio>
#include <cstring>

using namespace JSC;

int main() {
    char buffer[256];
    uint32_t pc = 0;

    printf("Testing DUP instructions:\n\n");

    // Correct opcodes for DUP:
    // dup v0.8b, v1.b[0]
    uint32_t opcode1 = 0x0E010420;
    memset(buffer, 0, sizeof(buffer));
    auto* entry1 = ARM64Disassembler::findInstruction(opcode1);
    if (entry1) {
        printf("Opcode 0x%08x (dup v0.8b, v1.b[0]):\n", opcode1);
        printf("  Name: %s\n", entry1->name);
        printf("  Mnemonic: %s\n", entry1->mnemonic);
        printf("  Operand count: %u\n\n", entry1->operandCount);
        
        // Show operands
        for (unsigned i = 0; i < entry1->operandCount; i++) {
            const auto& op = ARM64Disassembler::g_operandTable[entry1->operandOffset + i];
            printf("  Operand %u: type=%u, subtype=%u, f1[%u:%u], f2[%u:%u]\n",
                   i, op.type, op.subtype, 
                   op.field1_start, op.field1_width,
                   op.field2_start, op.field2_width);
        }
        
        ARM64Disassembler::formatInstruction(entry1, opcode1, &pc, &pc, &pc, buffer, sizeof(buffer));
        printf("\n  Formatted: %s\n\n", buffer);
    }

    // dup v0.16b, v1.b[0]
    uint32_t opcode2 = 0x4E010420;
    memset(buffer, 0, sizeof(buffer));
    auto* entry2 = ARM64Disassembler::findInstruction(opcode2);
    if (entry2) {
        printf("Opcode 0x%08x (dup v0.16b, v1.b[0]):\n", opcode2);
        printf("  Name: %s\n", entry2->name);
        printf("  Operand count: %u\n\n", entry2->operandCount);
        
        // Show operands
        for (unsigned i = 0; i < entry2->operandCount; i++) {
            const auto& op = ARM64Disassembler::g_operandTable[entry2->operandOffset + i];
            printf("  Operand %u: type=%u, subtype=%u, f1[%u:%u], f2[%u:%u]\n",
                   i, op.type, op.subtype, 
                   op.field1_start, op.field1_width,
                   op.field2_start, op.field2_width);
        }
        
        ARM64Disassembler::formatInstruction(entry2, opcode2, &pc, &pc, &pc, buffer, sizeof(buffer));
        printf("\n  Formatted: %s\n\n", buffer);
    }

    return 0;
}
