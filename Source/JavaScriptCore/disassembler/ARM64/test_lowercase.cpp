#include "A64InstructionTable.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

using namespace JSC::ARM64Disassembler;

int main() {
    printf("Testing lowercase mnemonics in table:\n\n");

    // Test various instruction types
    struct TestCase {
        uint32_t opcode;
        const char* expected_prefix;
        const char* description;
    };

    TestCase tests[] = {
        { 0x8b2242c2, "add", "ADD instruction" },
        { 0xc8e3fc41, "casal", "CASAL instruction" },
        { 0x54000000, "b.eq", "Conditional branch" },
        { 0xa9bf7bfd, "stp", "STP instruction" },
        { 0xf8450318, "ldur", "LDUR instruction" },
    };

    int passed = 0;
    int failed = 0;

    for (const auto& test : tests) {
        uint32_t* pc = (uint32_t*)&test.opcode;
        const InstructionEntry* entry = findInstruction(test.opcode);

        char buffer[256];
        formatInstruction(entry, test.opcode, pc, nullptr, nullptr, buffer, sizeof(buffer));

        // Extract mnemonic from formatted output
        const char* mnemonic = buffer;
        while (*mnemonic == ' ') mnemonic++;  // Skip leading spaces

        bool matches = strncmp(mnemonic, test.expected_prefix, strlen(test.expected_prefix)) == 0;

        printf("%-30s: %s\n", test.description, buffer);
        printf("  Expected prefix: %s\n", test.expected_prefix);
        printf("  Status: %s\n\n", matches ? "✅ PASS" : "❌ FAIL");

        if (matches) passed++;
        else failed++;
    }

    printf("==================\n");
    printf("Results: %d passed, %d failed\n\n", passed, failed);

    if (failed == 0) {
        printf("✅ All lowercase mnemonic tests passed\!\n");
        printf("   - Mnemonics are lowercase in table\n");
        printf("   - No runtime conversion needed\n");
        return 0;
    } else {
        printf("❌ Some tests failed\!\n");
        return 1;
    }
}
