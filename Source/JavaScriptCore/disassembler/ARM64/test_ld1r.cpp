#include "A64InstructionTable.h"
#include <cstdio>
#include <cstring>

using namespace JSC;

void test_ld1r(const char* description, uint32_t opcode, const char* expected) {
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
    printf("Testing LD1R instruction with all arrangements\n");
    printf("================================================\n\n");

    // Correct opcodes from objdump
    test_ld1r("LD1R 8b", 0x0D40C000, "{ v0.8b }, [x0]");
    test_ld1r("LD1R 16b", 0x4D40C000, "{ v0.16b }, [x0]");
    test_ld1r("LD1R 4h", 0x0D40C400, "{ v0.4h }, [x0]");
    test_ld1r("LD1R 8h", 0x4D40C400, "{ v0.8h }, [x0]");
    test_ld1r("LD1R 2s", 0x0D40C800, "{ v0.2s }, [x0]");
    test_ld1r("LD1R 4s", 0x4D40C800, "{ v0.4s }, [x0]");
    test_ld1r("LD1R 1d", 0x0D40CC00, "{ v0.1d }, [x0]");
    test_ld1r("LD1R 2d", 0x4D40CC00, "{ v0.2d }, [x0]");

    printf("Testing complete!\n");
    return 0;
}
