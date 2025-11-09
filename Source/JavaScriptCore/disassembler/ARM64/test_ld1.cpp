#include "A64InstructionTable.h"
#include <cstdio>
#include <cstring>

using namespace JSC;

void test_ld1(const char* description, uint32_t opcode, const char* expected) {
    char buffer[256];
    uint32_t pc = 0;
    memset(buffer, 0, sizeof(buffer));

    auto* entry = ARM64Disassembler::findInstruction(opcode);
    if (entry) {
        ARM64Disassembler::formatInstruction(entry, opcode, &pc, &pc, &pc, buffer, sizeof(buffer));

        // Check if output contains expected substring
        bool match = (strstr(buffer, expected) != nullptr);
        printf("%s: %s\n", match ? "✓" : "✗", description);
        printf("  Opcode: 0x%08X\n", opcode);
        printf("  Output: %s\n", buffer);
        printf("  Expect: %s\n", expected);
        if (!match) {
            printf("  MISMATCH!\n");
        }
        printf("\n");
    } else {
        printf("✗ %s: Instruction not found!\n", description);
        printf("  Opcode: 0x%08X\n\n", opcode);
    }
}

int main() {
    printf("Testing LD1 instruction with variable arrangements\n");
    printf("===================================================\n\n");

    // 8-bit elements
    test_ld1("LD1 8b (Q=0)", 0x0C407020, "{ v0.8b }, [x1]");
    test_ld1("LD1 16b (Q=1)", 0x4C407020, "{ v0.16b }, [x1]");

    // 16-bit elements
    test_ld1("LD1 4h (Q=0)", 0x0C407462, "{ v2.4h }, [x3]");
    test_ld1("LD1 8h (Q=1)", 0x4C407462, "{ v2.8h }, [x3]");

    // 32-bit elements
    test_ld1("LD1 2s (Q=0)", 0x0C407945, "{ v5.2s }, [x10]");
    test_ld1("LD1 4s (Q=1)", 0x4C407945, "{ v5.4s }, [x10]");

    // 64-bit elements
    test_ld1("LD1 1d (Q=0)", 0x0C407E87, "{ v7.1d }, [x20]");
    test_ld1("LD1 2d (Q=1)", 0x4C407E87, "{ v7.2d }, [x20]");

    printf("Testing complete!\n");
    return 0;
}
