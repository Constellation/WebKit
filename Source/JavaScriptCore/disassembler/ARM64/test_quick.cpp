#include "A64InstructionTable.h"
#include <stdio.h>
#include <string.h>

using namespace JSC::ARM64Disassembler;

int main() {
    // Test a few key instructions
    struct { uint32_t opcode; const char* desc; } tests[] = {
        {0x0b000824, "ADD shifted"},
        {0x4f08a401, "SXTL2"},
        {0x4e010420, "DUP"},
        {0x4ea28420, "ADD SIMD"},
        {0x1e602000, "FMUL/FCMP"},
    };
    
    for (const auto& test : tests) {
        const auto* entry = findInstruction(test.opcode);
        char buffer[128];
        uint32_t pc = 0;
        formatInstruction(entry, test.opcode, &pc, nullptr, nullptr, buffer, sizeof(buffer));
        printf("0x%08x (%s): %s\n", test.opcode, test.desc, buffer);
    }
    
    return 0;
}
