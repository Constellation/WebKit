#include "A64InstructionTable.h"
#include <stdio.h>

using namespace JSC::ARM64Disassembler;

int main() {
    uint32_t tests[] = {
        0x083C7FAA,  // casp w28, w29, w10, w11, [fp]
        0x88AD7E57,  // cas w13, w23, [x18]
        0x0B3D0294,  // add w20, w20, w29, uxtb
        0xD53FE297,  // mrs x23, s3_7_c14_c2_4
    };

    for (auto opcode : tests) {
        auto* entry = findInstruction(opcode);
        if (!entry) {
            printf("0x%08X: NOT FOUND\n", opcode);
            continue;
        }

        char buffer[256];
        uint32_t pc = 0;
        formatInstruction(entry, opcode, &pc, nullptr, nullptr, buffer, sizeof(buffer));
        printf("0x%08X: %s\n", opcode, buffer);
    }

    return 0;
}
