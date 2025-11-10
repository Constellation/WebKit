#include "A64InstructionTable.h"
#include <stdio.h>

using namespace JSC::ARM64Disassembler;

int main() {
    // FAMIN opcodes (constructed from table patterns)
    uint32_t famin_tests[] = {
        0x2ea0dc00,  // FAMIN v0.2s, v0.2s, v0.2s (single, Q=0)
        0x6ea0dc00,  // FAMIN v0.4s, v0.4s, v0.4s (single, Q=1)
        0x2ec01c00,  // FAMIN v0.4h, v0.4h, v0.4h (half, Q=0)
        0x6ec01c00,  // FAMIN v0.8h, v0.8h, v0.8h (half, Q=1)
    };

    const char* expected[] = {
        "famin    v0.2s, v0.2s, v0.2s",
        "famin    v0.4s, v0.4s, v0.4s",
        "famin    v0.4h, v0.4h, v0.4h",
        "famin    v0.8h, v0.8h, v0.8h",
    };

    printf("=== FAMIN Pattern-Based Detection Test ===\n\n");

    int passed = 0;
    for (int i = 0; i < 4; i++) {
        const auto* entry = findInstruction(famin_tests[i]);
        if (!entry) {
            printf("FAIL 0x%08x: Not found\n", famin_tests[i]);
            continue;
        }

        char buffer[256];
        uint32_t pc = 0;
        formatInstruction(entry, famin_tests[i], &pc, nullptr, nullptr, buffer, sizeof(buffer));

        printf("0x%08x: %s\n", famin_tests[i], buffer);
        passed++;
    }

    printf("\n=== Results: %d/4 tests found and formatted ===\n", passed);
    return passed == 4 ? 0 : 1;
}
