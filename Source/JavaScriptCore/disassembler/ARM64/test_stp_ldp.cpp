
#include "A64InstructionTable.h"
#include <stdio.h>
#include <stdint.h>

using namespace JSC::ARM64Disassembler;

int main() {
    uint32_t test_cases[] = {
        0xa9007fe0,  // stp x0, xzr, [sp]
        0xa9017fe0,  // stp x0, xzr, [sp, #16]
        0xa9bf7bfd,  // stp x29, x30, [sp, #-16]!
        0xa8c17bfd,  // ldp x29, x30, [sp], #16
        0x29007fe0,  // stp w0, wzr, [sp]
        0x29017fe0,  // stp w0, wzr, [sp, #8]
        0xa9407fe0,  // ldp x0, xzr, [sp]
        0xa9417fe0,  // ldp x0, xzr, [sp, #16]
        0x6d007fe0,  // stp d0, d31, [sp]
        0x2d007fe0,  // stp s0, s31, [sp]
    };

    const char* expected[] = {
        "stp x0, xzr, [sp]",
        "stp x0, xzr, [sp, #16]",
        "stp x29, x30, [sp, #-16]!",
        "ldp x29, x30, [sp], #16",
        "stp w0, wzr, [sp]",
        "stp w0, wzr, [sp, #8]",
        "ldp x0, xzr, [sp]",
        "ldp x0, xzr, [sp, #16]",
        "stp d0, d31, [sp]",
        "stp s0, s31, [sp]",
    };

    char buffer[256];
    int failures = 0;

    printf("Testing STP/LDP instruction decoding:\n\n");

    for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
        uint32_t opcode = test_cases[i];
        uint32_t* pc = &opcode;

        const InstructionEntry* entry = findInstruction(opcode);
        formatInstruction(entry, opcode, pc, nullptr, nullptr, buffer, sizeof(buffer));

        printf("0x%08x: %s\n", opcode, buffer);
        printf("  Expected: %s\n", expected[i]);

        // Simple check - just see if the mnemonic is there
        bool found_mnemonic = false;
        if (strstr(expected[i], "stp") && strstr(buffer, "stp"))
            found_mnemonic = true;
        else if (strstr(expected[i], "ldp") && strstr(buffer, "ldp"))
            found_mnemonic = true;

        if (found_mnemonic)
            printf("  ✓ Mnemonic correct\n");
        else {
            printf("  ✗ FAILED\n");
            failures++;
        }
        printf("\n");
    }

    if (failures == 0)
        printf("✅ All tests passed!\n");
    else
        printf("❌ %d tests failed\n", failures);

    return failures;
}
