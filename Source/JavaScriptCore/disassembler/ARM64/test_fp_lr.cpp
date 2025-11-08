
#include "A64InstructionTable.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

using namespace JSC::ARM64Disassembler;

int main() {
    uint32_t test_cases[] = {
        0xa9bf7bfd,
        0xa8c17bfd,
        0x910003fd,
        0xd65f03c0,
        0xf81f0ffe,
        0xf94003fd,
        0x8b1d03e0,
        0x91000420,
        0x8b010000,
    };

    const char* descriptions[] = {
        "stp, fp, lr",
        "ldp, fp, lr",
        "mov, fp, sp",
        "ret, lr",
        "str, lr",
        "ldr, fp",
        "add, x0, fp",
        "add, x0, x1",
        "add, x0, x0, x1",
    };

    char buffer[256];
    int passed = 0;
    int failed = 0;

    printf("Testing fp/lr register alias formatting:\n\n");

    for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
        uint32_t opcode = test_cases[i];
        uint32_t* pc = &opcode;

        const InstructionEntry* entry = findInstruction(opcode);
        formatInstruction(entry, opcode, pc, nullptr, nullptr, buffer, sizeof(buffer));

        printf("0x%08x: %s\n", opcode, buffer);

        // Check if expected strings are in output
        bool all_found = true;
        const char* desc = descriptions[i];
        char desc_copy[256];
        strncpy(desc_copy, desc, sizeof(desc_copy));

        char* token = strtok(desc_copy, ",");
        while (token) {
            // Trim whitespace
            while (*token == ' ') token++;
            char* end = token + strlen(token) - 1;
            while (end > token && *end == ' ') *end-- = 0;

            if (strstr(buffer, token) == nullptr) {
                all_found = false;
                printf("  ✗ Missing: '%s'\n", token);
            }
            token = strtok(nullptr, ",");
        }

        if (all_found) {
            printf("  ✓ Contains: %s\n\n", descriptions[i]);
            passed++;
        } else {
            printf("  Expected: %s\n\n", descriptions[i]);
            failed++;
        }
    }

    printf("====================\n");
    printf("Results: %d passed, %d failed\n", passed, failed);

    return (failed > 0) ? 1 : 0;
}
