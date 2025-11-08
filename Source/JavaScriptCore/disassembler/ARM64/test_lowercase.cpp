
#include "A64InstructionTable.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

using namespace JSC::ARM64Disassembler;

int main() {
    struct TestCase {
        uint32_t opcode;
        const char* expected_mnemonic;
        const char* description;
    } tests[] = {
        { 0x54000001, "b.ne", "Conditional branch ne" },
        { 0x54000000, "b.eq", "Conditional branch eq" },
        { 0x5400000a, "b.ge", "Conditional branch ge" },
        { 0x5400000b, "b.lt", "Conditional branch lt" },
        { 0x14000001, "b", "Unconditional branch" },
        { 0x94000001, "bl", "Branch with link" },
        { 0xd61f0000, "br", "Branch to register" },
        { 0xd63f0000, "blr", "Branch to register with link" },
        { 0xd65f03c0, "ret", "Return" },
        { 0x91000420, "add", "Add immediate" },
        { 0x8b010020, "add", "Add register" },
        { 0xd1000420, "sub", "Subtract immediate" },
        { 0xcb010020, "sub", "Subtract register" },
        { 0xf9400020, "ldr", "Load register" },
        { 0xf9000020, "str", "Store register" },
        { 0xa9bf7bfd, "stp", "Store pair" },
        { 0xa8c17bfd, "ldp", "Load pair" },
        { 0x34000001, "cbz", "Compare and branch if zero" },
        { 0x35000001, "cbnz", "Compare and branch if non-zero" },
    };

    char buffer[256];
    int passed = 0;
    int failed = 0;

    printf("Testing lowercase mnemonics and conditional branch formatting:\n\n");

    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        uint32_t opcode = tests[i].opcode;
        uint32_t* pc = &opcode;

        const InstructionEntry* entry = findInstruction(opcode);
        formatInstruction(entry, opcode, pc, nullptr, nullptr, buffer, sizeof(buffer));

        // Trim leading whitespace for comparison
        char* trimmed = buffer;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;

        // Check if expected mnemonic is at the start (lowercase, no extra spaces)
        bool mnemonic_ok = strncmp(trimmed, tests[i].expected_mnemonic,
                                   strlen(tests[i].expected_mnemonic)) == 0;

        // For conditional branches, verify no space between mnemonic and condition
        bool format_ok = true;
        if (strstr(tests[i].expected_mnemonic, ".") != nullptr) {
            // Should be "b.ne" not "b.       ne"
            if (strstr(trimmed, ".       ") != nullptr) {
                format_ok = false;
            }
        }

        if (mnemonic_ok && format_ok) {
            printf("✅ 0x%08x: %s\n", opcode, buffer);
            printf("   Expected: %s (%s)\n\n", tests[i].expected_mnemonic, tests[i].description);
            passed++;
        } else {
            printf("❌ 0x%08x: %s\n", opcode, buffer);
            printf("   Expected: %s (%s)\n", tests[i].expected_mnemonic, tests[i].description);
            if (!mnemonic_ok)
                printf("   Error: Mnemonic mismatch\n");
            if (!format_ok)
                printf("   Error: Conditional format has extra spaces\n");
            printf("\n");
            failed++;
        }
    }

    printf("====================\n");
    printf("Results: %d passed, %d failed\n", passed, failed);

    if (failed == 0) {
        printf("\n✅ All formatting tests passed!\n");
        printf("   - Mnemonics are lowercase\n");
        printf("   - Conditional branches use 'b.cond' format\n");
    }

    return (failed > 0) ? 1 : 0;
}
