#include "A64InstructionTable.h"
#include <cstdio>
#include <cstring>

using namespace JSC;

void test_q_only(const char* description, uint32_t opcode, const char* expected_arrangement) {
    char buffer[256];
    uint32_t pc = 0;
    memset(buffer, 0, sizeof(buffer));

    auto* entry = ARM64Disassembler::findInstruction(opcode);
    if (entry) {
        ARM64Disassembler::formatInstruction(entry, opcode, &pc, &pc, &pc, buffer, sizeof(buffer));

        // Check if output contains expected arrangement
        bool match = (strstr(buffer, expected_arrangement) != nullptr);
        printf("%s: %s\n", match ? "✓" : "✗", description);
        if (!match) {
            printf("  Opcode: 0x%08X\n", opcode);
            printf("  Output: %s\n", buffer);
            printf("  Expected arrangement: %s\n", expected_arrangement);
        }
    } else {
        printf("✗ %s: Instruction not found! (0x%08X)\n", description, opcode);
    }
}

int main() {
    printf("Testing Q-only arrangement comprehensive coverage\n");
    printf("=================================================\n\n");

    // Category 1: FP16 operations (already tested with FMUL)
    printf("Category 1: FP16 operations - .4H/.8H\n");
    printf("--------------------------------------\n");
    test_q_only("FMUL .4H", 0x2E401C00, ".4h");
    test_q_only("FMUL .8H", 0x6E401C00, ".8h");

    // Category 2: TBL (already tested)
    printf("\nCategory 2: Table operations - .8B/.16B\n");
    printf("----------------------------------------\n");
    test_q_only("TBL .8B", 0x0E000001, ".8b");
    test_q_only("TBL .16B", 0x4E000001, ".16b");

    // Category 3: Logical operations - should show .8B/.16B
    printf("\nCategory 3: Logical operations - .8B/.16B\n");
    printf("------------------------------------------\n");
    test_q_only("AND .8B", 0x0E201C00, ".8b");
    test_q_only("AND .16B", 0x4E201C00, ".16b");
    test_q_only("EOR .8B", 0x2E201C00, ".8b");
    test_q_only("EOR .16B", 0x6E201C00, ".16b");

    // Category 4: FP conversions with FP16 - should show .4H/.8H
    printf("\nCategory 4: FP16 conversions - .4H/.8H\n");
    printf("---------------------------------------\n");
    test_q_only("FCVTAS .4H", 0x0E79C800, ".4h");
    test_q_only("FCVTAS .8H", 0x4E79C800, ".8h");
    test_q_only("FRINTN .4H", 0x0E798800, ".4h");
    test_q_only("FRINTN .8H", 0x4E798800, ".8h");

    // Category 5: FMOV immediate - .4H/.8H or .2S/.4S
    printf("\nCategory 5: FMOV immediate\n");
    printf("--------------------------\n");
    test_q_only("FMOV .4H #1.0", 0x0F03FE00, ".4h");
    test_q_only("FMOV .8H #1.0", 0x4F03FE00, ".8h");
    test_q_only("FMOV .2S #1.0", 0x0F03F600, ".2s");
    test_q_only("FMOV .4S #1.0", 0x4F03F600, ".4s");

    printf("\n=== Summary ===\n");
    printf("Tested Q-only instructions from all major categories\n");
    printf("All arrangements should be correctly inferred from opcode bits\n");

    return 0;
}
