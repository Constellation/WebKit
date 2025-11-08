// Simple integration test
#include "A64DOpcode.h"
#include <stdio.h>
#include <stdint.h>

using namespace JSC::ARM64Disassembler;

int main() {
    // Test opcodes
    uint32_t opcodes[] = {
        0x91000420,  // add x0, x1, #1
        0xf9400020,  // ldr x0, [x1]
        0x14000001,  // b +4
        0xd61f0000,  // br x0
    };

    A64DOpcode disasm;

    printf("Testing A64DOpcode wrapper integration:\n\n");

    for (size_t i = 0; i < sizeof(opcodes) / sizeof(opcodes[0]); i++) {
        const char* result = disasm.disassemble(&opcodes[i]);
        printf("0x%08x: %s\n", opcodes[i], result);
    }

    printf("\n✅ Integration test complete!\n");
    return 0;
}
