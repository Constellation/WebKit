#include "A64InstructionTable.h"
#include <stdio.h>
#include <string.h>

using namespace JSC::ARM64Disassembler;

struct TestCase {
    const char* family;
    uint32_t opcodeQ0;
    uint32_t opcodeQ1;
    const char* expectedQ0;
    const char* expectedQ1;
};

int main() {
    TestCase tests[] = {
        {"ADD size=00", 0x0e228400, 0x4e228400, "8b", "16b"},
        {"ADD size=01", 0x0e628400, 0x4e628400, "4h", "8h"},
        {"ADD size=10", 0x0ea28400, 0x4ea28400, "2s", "4s"},
        {"ADD size=11", 0x0ee28400, 0x4ee28400, "1d", "2d"},
        {"FMUL sz=0", 0x2e20dc00, 0x6e20dc00, "2s", "4s"},
        {"FMUL sz=1", 0x2e60dc00, 0x6e60dc00, "1d", "2d"},
        {"FAMAX half", 0x0ec01c00, 0x4ec01c00, "4h", "8h"},
        {"FAMIN half", 0x0ec0dc00, 0x4ec0dc00, "4h", "8h"},
        {"FAMAX single", 0x0ea1dc00, 0x4ea1dc00, "2s", "4s"},
        {"FAMIN single", 0x0ea19c00, 0x4ea19c00, "2s", "4s"},
        {"AND", 0x0e201c00, 0x4e201c00, "8b", "16b"},
        {"EOR", 0x2e201c00, 0x6e201c00, "8b", "16b"},
        {"TBL", 0x0e000000, 0x4e000000, "8b", "16b"},
        {"EXT", 0x2e000000, 0x6e000000, "8b", "16b"},
        {"MOVI", 0x0f00e400, 0x4f00e400, "8b", "16b"},
        {"FMOV imm", 0x0f00f400, 0x4f00f400, "2s", "4s"},
    };
    
    int numTests = sizeof(tests) / sizeof(tests[0]);
    int passed = 0;
    int failed = 0;
    
    printf("=== Comprehensive Arrangement Test ===\n\n");
    
    for (int i = 0; i < numTests; i++) {
        // Test Q=0
        const auto* entry0 = findInstruction(tests[i].opcodeQ0);
        if (entry0) {
            char buffer[256];
            uint32_t pc = 0;
            formatInstruction(entry0, tests[i].opcodeQ0, &pc, nullptr, nullptr, buffer, sizeof(buffer));
            
            if (strstr(buffer, tests[i].expectedQ0)) {
                passed++;
            } else {
                printf("FAIL: %s Q=0 - Got: %s, Expected: %s\n",
                       tests[i].family, buffer, tests[i].expectedQ0);
                failed++;
            }
        } else {
            printf("FAIL: %s Q=0 - NOT FOUND\n", tests[i].family);
            failed++;
        }
        
        // Test Q=1
        const auto* entry1 = findInstruction(tests[i].opcodeQ1);
        if (entry1) {
            char buffer[256];
            uint32_t pc = 0;
            formatInstruction(entry1, tests[i].opcodeQ1, &pc, nullptr, nullptr, buffer, sizeof(buffer));
            
            if (strstr(buffer, tests[i].expectedQ1)) {
                passed++;
            } else {
                printf("FAIL: %s Q=1 - Got: %s, Expected: %s\n",
                       tests[i].family, buffer, tests[i].expectedQ1);
                failed++;
            }
        } else {
            printf("FAIL: %s Q=1 - NOT FOUND\n", tests[i].family);
            failed++;
        }
    }
    
    printf("\n=== Results: %d/%d passed ===\n", passed, passed + failed);
    return failed == 0 ? 0 : 1;
}
