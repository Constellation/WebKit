#include "A64InstructionTable.h"
#include <cstdio>
#include <cstring>

using namespace JSC;

void test_ldur_vector() {
    char buffer[256];

    printf("Testing LDUR vector (128-bit) instructions...\n\n");

    // LDUR (SIMD&FP) - 128-bit (Q register)
    // ldur q0, [sp, #192]
    uint32_t opcode1 = 0x3CCC03E0;  // LDUR Q0, [SP, #192]
    memset(buffer, 0, sizeof(buffer));
    auto* entry1 = ARM64Disassembler::findInstruction(opcode1);
    if (entry1) {
        printf("Instruction: %s\n", entry1->name);
        printf("Operand count: %u\n", entry1->operandCount);
        uint32_t pc = 0;
        ARM64Disassembler::formatInstruction(entry1, opcode1, &pc, &pc, &pc, buffer, sizeof(buffer));
        printf("0x%08x: %s\n", opcode1, buffer);
        printf("Expected: ldur q0, [sp, #192]\n\n");

        // Show operands
        const ARM64Disassembler::OperandDesc* operands = &ARM64Disassembler::g_operandTable[entry1->operandOffset];
        for (int i = 0; i < entry1->operandCount; i++) {
            printf("Operand %d: type=%u, f1_start=%u, f1_width=%u, f2_start=%u, f2_width=%u\n",
                   i, operands[i].type, operands[i].field1_start, operands[i].field1_width,
                   operands[i].field2_start, operands[i].field2_width);
        }
    } else {
        printf("0x%08x: NOT FOUND\n\n", opcode1);
    }

    printf("\n");

    // LDUR (SIMD&FP) - 64-bit (D register)
    // ldur d1, [sp, #16]
    uint32_t opcode2 = 0xFC4103E1;  // LDUR D1, [SP, #16]
    memset(buffer, 0, sizeof(buffer));
    auto* entry2 = ARM64Disassembler::findInstruction(opcode2);
    if (entry2) {
        printf("Instruction: %s\n", entry2->name);
        printf("Operand count: %u\n", entry2->operandCount);
        uint32_t pc = 0;
        ARM64Disassembler::formatInstruction(entry2, opcode2, &pc, &pc, &pc, buffer, sizeof(buffer));
        printf("0x%08x: %s\n", opcode2, buffer);
        printf("Expected: ldur d1, [sp, #16]\n\n");

        // Show operands
        const ARM64Disassembler::OperandDesc* operands = &ARM64Disassembler::g_operandTable[entry2->operandOffset];
        for (int i = 0; i < entry2->operandCount; i++) {
            printf("Operand %d: type=%u, f1_start=%u, f1_width=%u, f2_start=%u, f2_width=%u\n",
                   i, operands[i].type, operands[i].field1_start, operands[i].field1_width,
                   operands[i].field2_start, operands[i].field2_width);
        }
    } else {
        printf("0x%08x: NOT FOUND\n\n", opcode2);
    }
}

int main() {
    test_ldur_vector();
    return 0;
}
