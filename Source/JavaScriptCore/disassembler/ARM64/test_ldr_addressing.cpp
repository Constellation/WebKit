#include "A64InstructionTable.h"
#include <cstdio>
#include <cstring>

using namespace JSC;

void test_ldr(const char* description, uint32_t opcode, const char* expected) {
    char buffer[256];
    uint32_t pc = 0;
    memset(buffer, 0, sizeof(buffer));

    auto* entry = ARM64Disassembler::findInstruction(opcode);
    if (entry) {
        ARM64Disassembler::formatInstruction(entry, opcode, &pc, &pc, &pc, buffer, sizeof(buffer));

        bool match = (strcmp(buffer, expected) == 0);
        printf("%s: %s\n", match ? "OK" : "FAIL", description);
        if (!match) {
            printf("  Opcode: 0x%08X\n", opcode);
            printf("  Output: '%s'\n", buffer);
            printf("  Expect: '%s'\n", expected);
        }
    } else {
        printf("FAIL: %s - not found\n", description);
    }
}

int main() {
    printf("Testing LDR register offset addressing modes\n");
    printf("==============================================\n\n");

    // No shift (S=0)
    test_ldr("LDR w0, [x1, x2]", 0xB8626820, "   ldr      w0, [x1, x2]");
    test_ldr("LDR w0, [x1, w2, uxtw]", 0xB8624820, "   ldr      w0, [x1, w2, uxtw]");
    test_ldr("LDR w0, [x1, w2, sxtw]", 0xB862C820, "   ldr      w0, [x1, w2, sxtw]");

    // With shift (S=1)
    test_ldr("LDR w0, [x1, x2, lsl #2]", 0xB8627820, "   ldr      w0, [x1, x2, lsl #2]");
    test_ldr("LDR w0, [x1, w2, uxtw #2]", 0xB8625820, "   ldr      w0, [x1, w2, uxtw #2]");
    test_ldr("LDR w0, [x1, w2, sxtw #2]", 0xB862D820, "   ldr      w0, [x1, w2, sxtw #2]");

    return 0;
}
