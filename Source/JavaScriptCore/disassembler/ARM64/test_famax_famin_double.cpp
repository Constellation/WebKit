#include "A64InstructionTable.h"
#include <stdio.h>
#include <string.h>

using namespace JSC::ARM64Disassembler;

int main() {
    // FAMAX double-precision (size=11, Q=1 only, U=0)
    // Q=0 with size=11 is UNDEFINED per ARM spec
    uint32_t famax_double_q1 = 0x4ee1dc00;  // Q=1, size=11, U=0, Rd=0, Rn=0, Rm=0

    // FAMIN double-precision (size=11, Q=1 only, U=1)
    uint32_t famin_double_q1 = 0x6ee1dc00;  // Q=1, size=11, U=1, Rd=0, Rn=0, Rm=0

    printf("=== Testing FAMAX/FAMIN Double-Precision ===\n\n");

    // Test FAMAX double
    const auto* entry = findInstruction(famax_double_q1);
    printf("FAMAX double Q=1 (0x%08x): ", famax_double_q1);
    if (entry) {
        char buf[256];
        uint32_t pc = 0;
        formatInstruction(entry, famax_double_q1, &pc, nullptr, nullptr, buf, sizeof(buf));
        printf("%s\n", buf);

        // Verify it contains ".2d"
        if (strstr(buf, ".2d")) {
            printf("  ✓ Correct arrangement (.2d)\n");
        } else {
            printf("  ✗ Wrong arrangement (expected .2d)\n");
            return 1;
        }
    } else {
        printf("NOT FOUND\n");
        printf("  ✗ Entry missing from instruction table\n");
        return 1;
    }

    printf("\n");

    // Test FAMIN double
    entry = findInstruction(famin_double_q1);
    printf("FAMIN double Q=1 (0x%08x): ", famin_double_q1);
    if (entry) {
        char buf[256];
        uint32_t pc = 0;
        formatInstruction(entry, famin_double_q1, &pc, nullptr, nullptr, buf, sizeof(buf));
        printf("%s\n", buf);

        // Verify it contains ".2d"
        if (strstr(buf, ".2d")) {
            printf("  ✓ Correct arrangement (.2d)\n");
        } else {
            printf("  ✗ Wrong arrangement (expected .2d)\n");
            return 1;
        }
    } else {
        printf("NOT FOUND\n");
        printf("  ✗ Entry missing from instruction table\n");
        return 1;
    }

    printf("\n=== All tests passed ===\n");
    return 0;
}
