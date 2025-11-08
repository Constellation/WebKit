#include "A64InstructionTable.h"
#include <cstdio>
#include <cstring>

using namespace JSC;

int main() {
    char buffer[256];
    uint32_t pc = 0;
    
    // UMOV X0, V0.D[0]
    // Q=1, imm5=x1000 (bit 3 set = D), imm4=0111, Rn=0, Rd=0
    uint32_t opcode = 0x4E083C00;
    
    memset(buffer, 0, sizeof(buffer));
    auto* entry = ARM64Disassembler::findInstruction(opcode);
    if (entry) {
        printf("Opcode 0x%08x:\n", opcode);
        printf("Name: %s, Mnemonic: %s\n", entry->name, entry->mnemonic);
        printf("Operand count: %u\n\n", entry->operandCount);
        
        for (unsigned i = 0; i < entry->operandCount; i++) {
            const auto& op = ARM64Disassembler::g_operandTable[entry->operandOffset + i];
            printf("Operand %u: type=%u, subtype=%u, f1[%u:%u], f2[%u:%u]\n",
                   i, op.type, op.subtype, op.field1_start, op.field1_width,
                   op.field2_start, op.field2_width);
        }
        
        ARM64Disassembler::formatInstruction(entry, opcode, &pc, &pc, &pc, buffer, sizeof(buffer));
        printf("\nFormatted: %s\n", buffer);
        printf("Expected:  umov     x0, v0.d[0]\n");
    } else {
        printf("Instruction NOT FOUND\n");
    }
    
    return 0;
}
