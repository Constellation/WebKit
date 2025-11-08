#include "A64InstructionTable.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

using namespace JSC::ARM64Disassembler;

int main() {
    struct TestCase {
        uint32_t opcode;
        const char* expectedPrefix;
        const char* description;
    } tests[] = {
        { 0x5400000b, "b.lt", "Branch if less than" },
        { 0x5400000a, "b.ge", "Branch if greater or equal" },
        { 0x54000001, "b.ne", "Branch if not equal" },
        { 0x54000000, "b.eq", "Branch if equal" },
        { 0x54000008, "b.hi", "Branch if higher" },
        { 0x5400000c, "b.gt", "Branch if greater than" },
        { 0x5400000d, "b.le", "Branch if less or equal" },
    };

    printf("Testing conditional branch formatting:\n\n");

    int passed = 0;
    int failed = 0;

    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        uint32_t opcode = tests[i].opcode;
        uint32_t* pc = &opcode;

        const InstructionEntry* entry = findInstruction(opcode);
        char buffer[256];
        formatInstruction(entry, opcode, pc, nullptr, nullptr, buffer, sizeof(buffer));

        // Trim leading whitespace
        char* trimmed = buffer;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;

        printf("0x%08x: %s\n", opcode, buffer);
        printf("  Expected prefix: %s\n", tests[i].expectedPrefix);
        printf("  Description: %s\n", tests[i].description);

        // Check if it starts with expected prefix and has NO double dots
        bool starts_correctly = strncmp(trimmed, tests[i].expectedPrefix, strlen(tests[i].expectedPrefix)) == 0;
        bool no_double_dots = strstr(trimmed, "..") == nullptr;

        if (starts_correctly && no_double_dots) {
            printf("  Status: ✅ PASS\n\n");
            passed++;
        } else {
            printf("  Status: ❌ FAIL");
            if (!starts_correctly) printf(" (wrong prefix)");
            if (!no_double_dots) printf(" (has double dots)");
            printf("\n\n");
            failed++;
        }
    }

    printf("==================\n");
    printf("Results: %d passed, %d failed\n", passed, failed);

    if (failed == 0) {
        printf("\n✅ All conditional branch tests passed!\n");
        printf("   - Mnemonics use 'b.cond' format (single dot)\n");
        printf("   - No double dots in output\n");
    }

    return (failed > 0) ? 1 : 0;
}
