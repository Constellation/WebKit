#include "A64InstructionTable.h"
#include <cstdio>
#include <cstring>

using namespace JSC;

void test_fcvtas() {
    char buffer[256];

    printf("Testing FCVTAS instructions...\n\n");

    // FCVTAS (scalar) - Convert scalar single to signed integer
    // fcvtas w0, s1
    uint32_t opcode1 = 0x1E240020;  // FCVTAS W0, S1
    memset(buffer, 0, sizeof(buffer));
    auto* entry1 = ARM64Disassembler::findInstruction(opcode1);
    if (entry1) {
        uint32_t pc = 0;
        ARM64Disassembler::formatInstruction(entry1, opcode1, &pc, &pc, &pc, buffer, sizeof(buffer));
        printf("0x%08x: %s\n", opcode1, buffer);
        printf("Expected: fcvtas w0, s1\n\n");
    } else {
        printf("0x%08x: NOT FOUND\n\n", opcode1);
    }

    // FCVTAS (scalar) - Convert scalar double to signed integer
    // fcvtas x2, d3
    uint32_t opcode2 = 0x9E640062;  // FCVTAS X2, D3
    memset(buffer, 0, sizeof(buffer));
    auto* entry2 = ARM64Disassembler::findInstruction(opcode2);
    if (entry2) {
        uint32_t pc = 0;
        ARM64Disassembler::formatInstruction(entry2, opcode2, &pc, &pc, &pc, buffer, sizeof(buffer));
        printf("0x%08x: %s\n", opcode2, buffer);
        printf("Expected: fcvtas x2, d3\n\n");
    } else {
        printf("0x%08x: NOT FOUND\n\n", opcode2);
    }

    // FCVTAS (vector) - will show as generic instruction for now
    // We'll need more work to support vector register lists
}

int main() {
    test_fcvtas();
    return 0;
}
