#include "A64InstructionTable.h"
#include <cstdio>
#include <cstring>

using namespace JSC;

int main() {
    // LSL X0, X0, #6
    // This is an alias for UBFM X0, X0, #58, #57
    // Encoding: sf=1, opc=10, N=1, immr=58, imms=57, Rn=0, Rd=0
    // Bits: 1 10 1 00110 111010 111001 00000 00000
    //       sf opc N (fixed) immr   imms   Rn    Rd
    uint32_t opcode = 0xD37AF400;  // LSL x0, x0, #6
    
    char buffer[256];
    uint32_t pc = 0;
    memset(buffer, 0, sizeof(buffer));
    
    auto* entry = ARM64Disassembler::findInstruction(opcode);
    if (entry) {
        printf("Opcode: 0x%08x\n", opcode);
        printf("Name: %s\n", entry->name);
        printf("Mnemonic: %s\n", entry->mnemonic);
        printf("Operand count: %u\n\n", entry->operandCount);
        
        for (unsigned i = 0; i < entry->operandCount; i++) {
            const auto& op = ARM64Disassembler::g_operandTable[entry->operandOffset + i];
            printf("Operand %u: type=%u, subtype=%u, f1[%u:%u], f2[%u:%u]\n",
                   i, op.type, op.subtype, op.field1_start, op.field1_width,
                   op.field2_start, op.field2_width);
        }
        
        ARM64Disassembler::formatInstruction(entry, opcode, &pc, &pc, &pc, buffer, sizeof(buffer));
        printf("\nFormatted: %s\n", buffer);
        printf("Expected:     lsl      x0, x0, #6\n");
    } else {
        printf("Instruction NOT FOUND\n");
    }
    
    return 0;
}
