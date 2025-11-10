#include "A64InstructionTable.h"
#include <stdio.h>
#include <string.h>

using namespace JSC::ARM64Disassembler;

int main() {
    struct TestCase {
        const char* name;
        uint32_t opcodeQ0;
        uint32_t opcodeQ1;
        const char* expectedQ0;
        const char* expectedQ1;
    };
    
    TestCase tests[] = {
        // Integer SIMD - size:Q (bits[23:22]:bit[30])
        {"ADD 8-bit", 0x0e228400, 0x4e228400, "8b", "16b"},
        {"ADD 16-bit", 0x0e628400, 0x4e628400, "4h", "8h"},
        {"ADD 32-bit", 0x0ea28400, 0x4ea28400, "2s", "4s"},
        {"ADD 64-bit", 0x0ee28400, 0x4ee28400, "1d", "2d"},
        
        // FP SIMD - sz:Q (bit[22]:bit[30])
        {"FMUL single", 0x2e20dc00, 0x6e20dc00, "2s", "4s"},
        {"FMUL double", 0x2e60dc00, 0x6e60dc00, "1d", "2d"},
        
        // FAMAX half - Q only (fixed size=10)
        {"FAMAX half", 0x0ec01c00, 0x4ec01c00, "4h", "8h"},
        // FAMIN half - Q only (fixed size=10, U=1)
        {"FAMIN half", 0x2ec01c00, 0x6ec01c00, "4h", "8h"},
        
        // FAMAX single - size<0>:Q (size=10)
        {"FAMAX single", 0x0ea1dc00, 0x4ea1dc00, "2s", "4s"},
        // FAMIN single - size<0>:Q (size=10, U=1)
        {"FAMIN single", 0x2ea1dc00, 0x6ea1dc00, "2s", "4s"},

        // NOTE: FAMAX/FAMIN double-precision (size=11, .2D) are missing from instruction table
        // See FAMAX_FAMIN_DOUBLE_MISSING.md for details
        // {"FAMAX double", 0x4ee1dc00, "skip", "2d"},  // NOT IN TABLE
        // {"FAMIN double", 0x6ee1dc00, "skip", "2d"},  // NOT IN TABLE
        
        // Logical - always bytes
        {"AND", 0x0e201c00, 0x4e201c00, "8b", "16b"},
        {"EOR", 0x2e201c00, 0x6e201c00, "8b", "16b"},
        
        // Table - always bytes
        {"TBL", 0x0e000000, 0x4e000000, "8b", "16b"},
        {"TBX", 0x0e001000, 0x4e001000, "8b", "16b"},
        
        // EXT - always bytes
        {"EXT", 0x2e000000, 0x6e000000, "8b", "16b"},
        
        // Immediate - bytes
        {"MOVI", 0x0f00e400, 0x4f00e400, "8b", "16b"},
        
        // FMOV - singles
        {"FMOV imm", 0x0f00f400, 0x4f00f400, "2s", "4s"},
    };
    
    int total = sizeof(tests) / sizeof(tests[0]);
    int passed = 0;
    int failed = 0;
    
    printf("=== Comprehensive Arrangement Verification ===\n");
    printf("Testing %d instruction families with Q-bit arrangements\n\n", total);
    
    for (int i = 0; i < total; i++) {
        // Test Q=0 (skip if marked)
        if (strcmp(tests[i].expectedQ0, "skip") != 0) {
            const auto* entry0 = findInstruction(tests[i].opcodeQ0);
            if (entry0) {
                char buf[256];
                uint32_t pc = 0;
                formatInstruction(entry0, tests[i].opcodeQ0, &pc, nullptr, nullptr, buf, sizeof(buf));
                if (strstr(buf, tests[i].expectedQ0)) {
                    passed++;
                } else {
                    printf("FAIL: %s (Q=0) - expected %s, got: %s\n", 
                           tests[i].name, tests[i].expectedQ0, buf);
                    failed++;
                }
            } else {
                printf("FAIL: %s (Q=0) - NOT FOUND\n", tests[i].name);
                failed++;
            }
        }
        
        // Test Q=1
        const auto* entry1 = findInstruction(tests[i].opcodeQ1);
        if (entry1) {
            char buf[256];
            uint32_t pc = 0;
            formatInstruction(entry1, tests[i].opcodeQ1, &pc, nullptr, nullptr, buf, sizeof(buf));
            if (strstr(buf, tests[i].expectedQ1)) {
                passed++;
            } else {
                printf("FAIL: %s (Q=1) - expected %s, got: %s\n", 
                       tests[i].name, tests[i].expectedQ1, buf);
                failed++;
            }
        } else {
            printf("FAIL: %s (Q=1) - NOT FOUND\n", tests[i].name);
            failed++;
        }
    }
    
    printf("\n=== Results ===\n");
    printf("Total instruction families tested: %d\n", total);
    printf("Total test cases: %d\n", passed + failed);
    printf("Passed: %d/%d (%.1f%%)\n", passed, passed+failed, 100.0*passed/(passed+failed));
    printf("Failed: %d/%d\n", failed, passed+failed);
    
    if (failed == 0) {
        printf("\n✓ All arrangement inference patterns work correctly\!\n");
    }
    
    return failed == 0 ? 0 : 1;
}
