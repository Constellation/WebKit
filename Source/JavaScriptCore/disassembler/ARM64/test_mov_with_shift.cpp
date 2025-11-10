#include "A64InstructionTable.h"
#include <stdio.h>

using namespace JSC::ARM64Disassembler;

int main() {
    char buffer[256];
    uint32_t pc = 0;

    printf("Testing MOVZ with LSL shifts:\n\n");

    uint32_t opcodes[] = {
        0xd2a0014a,  // movz x10, #0xa, lsl #16
        0xd2c0016b,  // movz x11, #0xb, lsl #32
        0xd2e0018c   // movz x12, #0xc, lsl #48
    };

    const char* expected[] = {
        "movz x10, #0xa, lsl #16",
        "movz x11, #0xb, lsl #32",
        "movz x12, #0xc, lsl #48"
    };

    for (int i = 0; i < 3; i++) {
        auto* entry = findInstruction(opcodes[i]);
        if (entry) {
            formatInstruction(entry, opcodes[i], &pc, nullptr, nullptr, buffer, sizeof(buffer));
            printf("0x%08x: %s\n", opcodes[i], buffer);
            printf("            Expected: %s\n\n", expected[i]);
        }
    }

    return 0;
}
