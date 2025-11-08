
#include "A64InstructionTable.h"
#include <stdio.h>
#include <stdint.h>

using namespace JSC::ARM64Disassembler;

int main() {
    uint32_t test_cases[] = {
        0x91000420,
        0x11000420,
        0x8b010020,
        0xd1000420,
        0x51000420,
        0xaa0103e0,
        0x2a0103e0,
        0xd2800000,
        0xf9400020,
        0xb9400020,
        0xf9000020,
        0xb9000020,
        0x14000001,
        0x94000001,
        0xd61f0000,
        0xd63f0000,
        0xd65f0000,
        0x54000001,
        0x54000000,
        0x8a010000,
        0xaa010000,
        0xca010000,
        0x1e602000,
        0x1e202800,
    };

    char buffer[256];

    for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
        uint32_t opcode = test_cases[i];
        uint32_t* pc = &opcode;

        const InstructionEntry* entry = findInstruction(opcode);
        formatInstruction(entry, opcode, pc, nullptr, nullptr, buffer, sizeof(buffer));

        printf("0x%08x: %s\n", opcode, buffer);
    }

    return 0;
}
