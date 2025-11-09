#include "A64InstructionTable.h"
#include <cstdio>
#include <cstring>

using namespace JSC;

void test_ldstr(const char* description, uint32_t opcode, const char* expected) {
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
    printf("Testing comprehensive load/store addressing modes\n");
    printf("==================================================\n\n");

    // LDR variants
    printf("LDR variants:\n");
    test_ldstr("LDR w0, [x1, x2]", 0xB8626820, "   ldr      w0, [x1, x2]");
    test_ldstr("LDR x0, [x1, x2]", 0xF8626820, "   ldr      x0, [x1, x2]");
    test_ldstr("LDR w0, [x1, x2, lsl #2]", 0xB8627820, "   ldr      w0, [x1, x2, lsl #2]");
    test_ldstr("LDR x0, [x1, x2, lsl #3]", 0xF8627820, "   ldr      x0, [x1, x2, lsl #3]");

    // STR variants
    printf("\nSTR variants:\n");
    test_ldstr("STR w0, [x1, x2]", 0xB8226820, "   str      w0, [x1, x2]");
    test_ldstr("STR x0, [x1, x2]", 0xF8226820, "   str      x0, [x1, x2]");
    test_ldstr("STR w0, [x1, x2, lsl #2]", 0xB8227820, "   str      w0, [x1, x2, lsl #2]");
    test_ldstr("STR x0, [x1, x2, lsl #3]", 0xF8227820, "   str      x0, [x1, x2, lsl #3]");

    // With W register extends
    printf("\nWith W register extends:\n");
    test_ldstr("LDR w0, [x1, w2, uxtw]", 0xB8624820, "   ldr      w0, [x1, w2, uxtw]");
    test_ldstr("LDR w0, [x1, w2, sxtw]", 0xB862C820, "   ldr      w0, [x1, w2, sxtw]");
    test_ldstr("STR w0, [x1, w2, uxtw]", 0xB8224820, "   str      w0, [x1, w2, uxtw]");
    test_ldstr("STR w0, [x1, w2, sxtw #2]", 0xB822D820, "   str      w0, [x1, w2, sxtw #2]");

    return 0;
}
