#include "A64InstructionTable.h"
#include <cstdio>
#include <cstring>

using namespace JSC;

int main() {
    char buffer[256];
    uint32_t pc = 0;
    
    // ADD w4, w1, w0, lsl #2
    // Encoding: shifted register
    // Rd=4, Rn=1, Rm=0, shift=00 (LSL), imm6=000010 (amount 2)
    uint32_t opcode = 0x0B000824;  // ADD (shifted register, 32-bit)
    
    memset(buffer, 0, sizeof(buffer));
    auto* entry = ARM64Disassembler::findInstruction(opcode);
    if (entry) {
        printf("Opcode 0x%08x:\n", opcode);
        printf("Name: %s, Mnemonic: %s\n", entry->name, entry->mnemonic);
        printf("Operand count: %u\n\n", entry->operandCount);
        
        for (unsigned i = 0; i < entry->operandCount; i++) {
            const auto& op = ARM64Disassembler::g_operandTable[entry->operandOffset + i];
            printf("Operand %u: type=%u, f1[%u:%u], f2[%u:%u]\n",
                   i, op.type, op.field1_start, op.field1_width,
                   op.field2_start, op.field2_width);
        }
        
        ARM64Disassembler::formatInstruction(entry, opcode, &pc, &pc, &pc, buffer, sizeof(buffer));
        printf("\nFormatted: %s\n", buffer);
        printf("Expected:     add      w4, w1, w0, lsl #2\n");
    } else {
        printf("Instruction NOT FOUND\n");
    }
    
    return 0;
}
