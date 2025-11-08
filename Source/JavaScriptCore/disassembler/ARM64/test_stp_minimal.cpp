// Minimal STP test without WebKit dependencies
#include <stdio.h>
#include <stdint.h>
#include <string.h>

// Minimal definitions from A64InstructionTable.h
struct InstructionEntry {
    const char* name;
    const char* mnemonic;
    uint32_t mask;
    uint32_t pattern;
    uint16_t operandOffset;
    uint8_t operandCount;
    uint8_t flags;
};

struct OperandDesc {
    uint8_t type;
    uint8_t subtype;
    uint8_t field1;
    uint8_t field2;
};

struct FieldMeta {
    const char* name;
    uint8_t bitStart;
    uint8_t bitWidth;
};

// External declarations
extern const InstructionEntry g_instructionTable[];
extern const size_t g_instructionTableSize;
extern const OperandDesc g_operandTable[];
extern const FieldMeta g_fieldMetadata[];
extern const size_t g_fieldMetadataSize;

// Functions
const InstructionEntry* findInstruction(uint32_t opcode);
void formatInstruction(const InstructionEntry* entry, uint32_t opcode,
                      uint32_t* pc, uint32_t* startPC, uint32_t* endPC,
                      char* buffer, size_t bufferSize);

int main() {
    // Test STP instructions
    struct TestCase {
        uint32_t opcode;
        const char* expected_mnemonic;
        const char* description;
    } tests[] = {
        // 64-bit STP
        { 0xa9007fe0, "STP", "stp x0, xzr, [sp]" },
        { 0xa9017fe0, "STP", "stp x0, xzr, [sp, #16]" },
        { 0xa9bf7bfd, "STP", "stp x29, x30, [sp, #-16]!" },

        // 32-bit STP
        { 0x29007fe0, "STP", "stp w0, wzr, [sp]" },
        { 0x29017fe0, "STP", "stp w0, wzr, [sp, #8]" },

        // 64-bit LDP
        { 0xa9407fe0, "LDP", "ldp x0, xzr, [sp]" },
        { 0xa9417fe0, "LDP", "ldp x0, xzr, [sp, #16]" },
        { 0xa8c17bfd, "LDP", "ldp x29, x30, [sp], #16" },

        // 32-bit LDP
        { 0x29407fe0, "LDP", "ldp w0, wzr, [sp]" },
    };

    printf("Testing STP/LDP instruction decoding:\n\n");

    int passed = 0;
    int failed = 0;

    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        uint32_t opcode = tests[i].opcode;
        uint32_t* pc = &opcode;
        char buffer[256];

        const InstructionEntry* entry = findInstruction(opcode);

        if (!entry) {
            printf("❌ 0x%08x: NOT FOUND\n", opcode);
            printf("   Expected: %s\n\n", tests[i].description);
            failed++;
            continue;
        }

        formatInstruction(entry, opcode, pc, nullptr, nullptr, buffer, sizeof(buffer));

        bool mnemonic_match = strcmp(entry->mnemonic, tests[i].expected_mnemonic) == 0;

        if (mnemonic_match) {
            printf("✅ 0x%08x: %s\n", opcode, buffer);
            printf("   Expected: %s\n\n", tests[i].description);
            passed++;
        } else {
            printf("❌ 0x%08x: %s\n", opcode, buffer);
            printf("   Expected mnemonic: %s, Got: %s\n",
                   tests[i].expected_mnemonic, entry->mnemonic);
            printf("   Expected: %s\n\n", tests[i].description);
            failed++;
        }
    }

    printf("====================\n");
    printf("Results: %d passed, %d failed\n", passed, failed);

    return (failed > 0) ? 1 : 0;
}
