#include "A64InstructionTable.h"
#include <cstdio>
#include <cstring>

using namespace JSC;

int main() {
    char buffer[256];
    uint32_t pc = 0;

    // Test a few common INS instruction patterns
    // INS Vd.B[index], Wn
    uint32_t ins_opcodes[] = {
        0x4E011C01,  // INS v1.B[0], w0
        0x4E081C01,  // INS v1.H[0], w0
        0x4E041C01,  // INS v1.S[0], w0
        0x6E081C01,  // INS v1.D[0], x0
        0x6E044401,  // INS v1.S[1], v0.S[0]
    };

    for (int i = 0; i < 5; i++) {
        uint32_t opcode = ins_opcodes[i];
        memset(buffer, 0, sizeof(buffer));
        
        auto* entry = ARM64Disassembler::findInstruction(opcode);
        if (entry) {
            printf("Opcode 0x%08x:\n", opcode);
            printf("  Name: %s\n", entry->name);
            printf("  Mnemonic: %s\n", entry->mnemonic);
            ARM64Disassembler::formatInstruction(entry, opcode, &pc, &pc, &pc, buffer, sizeof(buffer));
            printf("  Formatted: %s\n\n", buffer);
        } else {
            printf("Opcode 0x%08x: NOT FOUND\n\n", opcode);
        }
    }

    return 0;
}
