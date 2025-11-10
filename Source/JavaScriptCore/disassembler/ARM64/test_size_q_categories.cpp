#include "A64InstructionTable.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

using namespace JSC::ARM64Disassembler;

struct TestCase {
    const char* category;
    const char* mnemonic;
    uint32_t opcode_q0;
    uint32_t opcode_q1;
    const char* expected_q0;
    const char* expected_q1;
};

int main() {
    TestCase tests[] = {
        // Category 1: SIZE_Q_STANDARD (8 arrangements)
        {"SIZE_Q_STANDARD", "ADD", 0x0e210400, 0x4e210400, "v0.8b", "v0.16b"},   // size=00
        {"SIZE_Q_STANDARD", "ADD", 0x0e610400, 0x4e610400, "v0.4h", "v0.8h"},    // size=01
        {"SIZE_Q_STANDARD", "ADD", 0x0ea10400, 0x4ea10400, "v0.2s", "v0.4s"},    // size=10
        {"SIZE_Q_STANDARD", "ADD", 0x0ee10400, 0x4ee10400, "skip", "v0.2d"},     // size=11 (Q=0 reserved)

        {"SIZE_Q_STANDARD", "SUB", 0x2e218400, 0x6e218400, "v0.8b", "v0.16b"},
        {"SIZE_Q_STANDARD", "SQADD", 0x0e210c00, 0x4e210c00, "v0.8b", "v0.16b"},
        {"SIZE_Q_STANDARD", "CMGT", 0x0e213400, 0x4e213400, "v0.8b", "v0.16b"},

        // Category 2A: SIZE_Q_FP_STYLE - FAMAX/FAMIN (4 arrangements)
        {"SIZE_Q_FP_STYLE", "FAMAX", 0x0ea1dc00, 0x4ea1dc00, "v0.2s", "v0.4s"},  // size=10 (single)
        {"SIZE_Q_FP_STYLE", "FAMAX", 0x0ee1dc00, 0x4ee1dc00, "skip", "v0.2d"},   // size=11 (double, Q=0 reserved)
        {"SIZE_Q_FP_STYLE", "FAMIN", 0x2ea1dc00, 0x6ea1dc00, "v0.2s", "v0.4s"},
        {"SIZE_Q_FP_STYLE", "FAMIN", 0x2ee1dc00, 0x6ee1dc00, "skip", "v0.2d"},

        // Category 2B: SIZE_Q_FP_STYLE - Byte-only (CNT, RBIT)
        {"SIZE_Q_FP_STYLE", "CNT", 0x0e205800, 0x4e205800, "v0.8b", "v0.16b"},   // size=00 only
        {"SIZE_Q_FP_STYLE", "RBIT", 0x2e20b800, 0x6e20b800, "v0.8b", "v0.16b"},  // FIXED: size=00 only

        // Category 3: OTHER_7_WAYS
        {"OTHER_7_WAYS", "CLS", 0x0e204800, 0x4e204800, "v0.8b", "v0.16b"},      // size=00
        {"OTHER_7_WAYS", "CLS", 0x0e604800, 0x4e604800, "v0.4h", "v0.8h"},       // size=01
        {"OTHER_7_WAYS", "CLS", 0x0ea04800, 0x4ea04800, "v0.2s", "v0.4s"},       // size=10
        {"OTHER_7_WAYS", "ADDV", 0x0e31b800, 0x4e31b800, "v0.8b", "v0.16b"},     // size=00
        {"OTHER_7_WAYS", "ADDV", 0x0e71b800, 0x4e71b800, "v0.4h", "v0.8h"},      // size=01
        {"OTHER_7_WAYS", "ADDV", 0x0eb1b800, 0x4eb1b800, "skip", "v0.4s"},       // size=10 (Q=0 reserved)

        // Category 4: OTHER_6_WAYS (element ops, no byte, no double)
        {"OTHER_6_WAYS", "MUL (elem)", 0x0f428000, 0x4f428000, "v0.4h", "v0.8h"}, // size=01, H=0, L=0, M=0, Rm=2
        {"OTHER_6_WAYS", "MUL (elem)", 0x0f828000, 0x4f828000, "v0.2s", "v0.4s"}, // size=10
        {"OTHER_6_WAYS", "MLA (elem)", 0x0f420000, 0x4f420000, "v0.4h", "v0.8h"}, // FIXED: size=01, Rm=2
        {"OTHER_6_WAYS", "MLA (elem)", 0x0f820000, 0x4f820000, "v0.2s", "v0.4s"}, // FIXED: size=10, Rm=2

        // Category 5: OTHER_5_WAYS (REV32)
        {"OTHER_5_WAYS", "REV32", 0x2e200800, 0x6e200800, "v0.8b", "v0.16b"},    // size=00
        {"OTHER_5_WAYS", "REV32", 0x2e600800, 0x6e600800, "v0.4h", "v0.8h"},     // size=01

        // Category 6: NO_ARRANGEMENT_TABLE
        // 6A: Narrowing operations
        {"NO_ARRANGEMENT_TABLE", "ADDHN", 0x0e214000, 0x4e214000, "v0.8b", "v0.16b"},  // size=00
        {"NO_ARRANGEMENT_TABLE", "ADDHN", 0x0e614000, 0x4e614000, "v0.4h", "v0.8h"},   // size=01

        // 6B: Widening operations (show source arrangement)
        {"NO_ARRANGEMENT_TABLE", "SADDL", 0x0e210000, 0x4e210000, "v0.8b", "v0.16b"},  // FIXED: size=00 (reads 8B/16B)
        {"NO_ARRANGEMENT_TABLE", "SADDL", 0x0e610000, 0x4e610000, "v0.4h", "v0.8h"},   // FIXED: size=01 (reads 4H/8H)

        // 6C: Logical operations (byte-only)
        {"NO_ARRANGEMENT_TABLE", "AND", 0x0e211c00, 0x4e211c00, "v0.8b", "v0.16b"},
        {"NO_ARRANGEMENT_TABLE", "ORR", 0x0ea21c20, 0x4ea21c20, "v0.8b", "v0.16b"},    // FIXED: Rd=0, Rn=1, Rm=2 (no MOV alias)

        // 6D: Dot products (show input arrangement, not accumulator)
        {"NO_ARRANGEMENT_TABLE", "SDOT (vec)", 0x0e019400, 0x4e019400, "v0.8b", "v0.16b"}, // Shows input .8B/.16B, not accumulator .2S/.4S
    };

    int total = 0;
    int passed = 0;
    int failed = 0;

    printf("Testing size:Q encoding patterns across all 6 categories...\n\n");

    for (const auto& test : tests) {
        total++;

        char buffer[256];
        uint32_t pc = 0;

        // Test Q=0
        if (strcmp(test.expected_q0, "skip") != 0) {
            auto* entry = findInstruction(test.opcode_q0);
            if (!entry) {
                printf("❌ %s: %s Q=0 - Instruction not found (opcode=0x%08x)\n",
                       test.category, test.mnemonic, test.opcode_q0);
                failed++;
                continue;
            }

            formatInstruction(entry, test.opcode_q0, &pc, nullptr, nullptr, buffer, sizeof(buffer));

            if (strstr(buffer, test.expected_q0) != nullptr) {
                printf("✅ %s: %s Q=0 → %s\n", test.category, test.mnemonic, test.expected_q0);
                passed++;
            } else {
                printf("❌ %s: %s Q=0 - Expected '%s', got '%s' (opcode=0x%08x)\n",
                       test.category, test.mnemonic, test.expected_q0, buffer, test.opcode_q0);
                failed++;
            }
        }

        // Test Q=1
        if (strcmp(test.expected_q1, "skip") != 0) {
            total++;
            auto* entry = findInstruction(test.opcode_q1);
            if (!entry) {
                printf("❌ %s: %s Q=1 - Instruction not found (opcode=0x%08x)\n",
                       test.category, test.mnemonic, test.opcode_q1);
                failed++;
                continue;
            }

            formatInstruction(entry, test.opcode_q1, &pc, nullptr, nullptr, buffer, sizeof(buffer));

            if (strstr(buffer, test.expected_q1) != nullptr) {
                printf("✅ %s: %s Q=1 → %s\n", test.category, test.mnemonic, test.expected_q1);
                passed++;
            } else {
                printf("❌ %s: %s Q=1 - Expected '%s', got '%s' (opcode=0x%08x)\n",
                       test.category, test.mnemonic, test.expected_q1, buffer, test.opcode_q1);
                failed++;
            }
        }
    }

    printf("\n");
    printf("======================================================================\n");
    printf("Results: %d/%d tests passed (%.1f%%)\n", passed, total, (passed * 100.0) / total);
    printf("Failed: %d\n", failed);

    return (failed == 0) ? 0 : 1;
}
