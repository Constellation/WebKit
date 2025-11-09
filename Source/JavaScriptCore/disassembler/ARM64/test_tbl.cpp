#include "A64InstructionTable.h"
#include <cstdio>
#include <cstring>

using namespace JSC;

int main() {
    // TBL single register: tbl v1.8b, { v0.16b }, v1.8b
    // Q=0, len=00, Rd=1, Rn=0, Rm=1
    uint32_t opcode = 0x0E000001;

    char buffer[256];
    uint32_t pc = 0;
    memset(buffer, 0, sizeof(buffer));

    auto* entry = ARM64Disassembler::findInstruction(opcode);
    if (entry) {
        printf("Found: %s (mnemonic: %s)\n", entry->name, entry->mnemonic);
        printf("Operand count: %u\n", entry->operandCount);

        ARM64Disassembler::formatInstruction(entry, opcode, &pc, &pc, &pc, buffer, sizeof(buffer));
        printf("Output: %s\n", buffer);

        // Print operand details
        for (unsigned i = 0; i < entry->operandCount; i++) {
            const auto& op = ARM64Disassembler::g_operandTable[entry->operandOffset + i];
            printf("  Operand %u: type=%u, f1[%u:%u], f2[%u:%u]\n",
                   i, op.type, op.field1_start, op.field1_width,
                   op.field2_start, op.field2_width);
        }
    } else {
        printf("Instruction not found!\n");
    }

    return 0;
}
