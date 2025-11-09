#include "A64InstructionTable.h"
#include <cstdio>
#include <cstring>

using namespace JSC;

int main() {
    uint32_t opcode = 0xF140001F;  // CMP x0, #0, lsl #12
    uint32_t pc = 0;
    char buffer[256];
    memset(buffer, 0, sizeof(buffer));
    
    auto* entry = ARM64Disassembler::findInstruction(opcode);
    if (entry) {
        printf("Found: %s (mnemonic: %s)\n", entry->name, entry->mnemonic);
        printf("Operand count: %u\n", entry->operandCount);
        
        // Show operands
        for (unsigned i = 0; i < entry->operandCount; i++) {
            const auto& op = ARM64Disassembler::g_operandTable[entry->operandOffset + i];
            printf("  Operand %u: type=%u, subtype=%u, f1[%u:%u], f2[%u:%u]\n",
                   i, op.type, op.subtype,
                   op.field1_start, op.field1_width,
                   op.field2_start, op.field2_width);
        }
        
        ARM64Disassembler::formatInstruction(entry, opcode, &pc, &pc, &pc, buffer, sizeof(buffer));
        printf("\nOutput: %s\n", buffer);
    }
    
    return 0;
}
