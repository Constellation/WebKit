#include "A64InstructionTable.h"
#include <stdio.h>
#include <string.h>

using namespace JSC::ARM64Disassembler;

int main() {
    struct TestCase {
        uint32_t opcode;
        const char* description;
        const char* expected;
    };

    TestCase tests[] = {
        // CMP with various immediates (should be hex)
        { 0x7100281f, "CMP w0, #10", "   cmp      w0, #0xa" },
        { 0x7100fc1f, "CMP w0, #63", "   cmp      w0, #0x3f" },
        { 0x710ffc1f, "CMP w0, #1023", "   cmp      w0, #0x3ff" },

        // ADD with larger immediates (should be hex)
        { 0x91002800, "ADD x0, x0, #10", "   add      x0, x0, #0xa" },
        { 0x9103fc00, "ADD x0, x0, #255", "   add      x0, x0, #0xff" },

        // SUB with larger immediates (should be hex)
        { 0xd1002800, "SUB x0, x0, #10", "   sub      x0, x0, #0xa" },

        // Small values (0 and 1 look same in hex/decimal)
        { 0x91000400, "ADD x0, x0, #1", "   add      x0, x0, #0x1" },
    };

    int passed = 0;
    int total = sizeof(tests) / sizeof(tests[0]);

    printf("Testing ADD/SUB/CMP Hex Formatting\n");
    printf("===================================\n\n");

    for (int i = 0; i < total; i++) {
        auto* entry = findInstruction(tests[i].opcode);
        if (entry) {
            char buffer[256];
            uint32_t pc = 0;
            formatInstruction(entry, tests[i].opcode, &pc, nullptr, nullptr, buffer, sizeof(buffer));

            bool match = strcmp(buffer, tests[i].expected) == 0;
            printf("%s: %s\n", match ? "✓" : "✗", tests[i].description);
            printf("  Got:      %s\n", buffer);
            if (!match) {
                printf("  Expected: %s\n", tests[i].expected);
            }
            printf("\n");

            if (match) passed++;
        } else {
            printf("✗: %s - No match found!\n\n", tests[i].description);
        }
    }

    printf("Result: %d/%d tests passed\n", passed, total);
    return (passed == total) ? 0 : 1;
}
