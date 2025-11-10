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
        // Integer SIMD - size:Q encoding (bits[23:22]=size, bit[30]=Q)
        {"ADD 8-bit", 0x0e228400, 0x4e228400, "8b", "16b"},
        {"ADD 16-bit", 0x0e628400, 0x4e628400, "4h", "8h"},
        {"ADD 32-bit", 0x0ea28400, 0x4ea28400, "2s", "4s"},
        {"ADD 64-bit", 0x0ee28400, 0x4ee28400, "1d", "2d"},
        
        // FP SIMD - sz:Q encoding (bit[22]=sz, bit[30]=Q)
        {"FMUL single", 0x2e20dc00, 0x6e20dc00, "2s", "4s"},
        {"FMUL double", 0x2e60dc00, 0x6e60dc00, "1d", "2d"},
        
        // FAMAX/FAMIN half - Q only (bits[23:22]=10 fixed)
        {"FAMAX half", 0x0ec01c00, 0x4ec01c00, "4h", "8h"},
        {"FAMIN half", 0x2ec01c00, 0x6ec01c00, "4h", "8h"},
        
        // FAMAX/FAMIN single/double - size<0>:Q (bits[23:22]=1x)
        {"FAMAX single", 0x0ea1dc00, 0x4ea1dc00, "2s", "4s"},
        {"FAMIN single", 0x2ea1dc00, 0x6ea1dc00, "2s", "4s"},
        
        // Logical - always bytes (Q determines vector width)
        {"AND", 0x0e201c00, 0x4e201c00, "8b", "16b"},
        {"EOR", 0x2e201c00, 0x6e201c00, "8b", "16b"},
        
        // Table lookup - always bytes
        {"TBL", 0x0e000000, 0x4e000000, "8b", "16b"},
        {"TBX", 0x0e001000, 0x4e001000, "8b", "16b"},
        
        // EXT - always bytes
        {"EXT", 0x2e000000, 0x6e000000, "8b", "16b"},
        
        // Immediate forms - bytes
        {"MOVI", 0x0f00e400, 0x4f00e400, "8b", "16b"},
        
        // FMOV immediate - singles (cmode determines type)
        {"FMOV imm", 0x0f00f400, 0x4f00f400, "2s", "4s"},
    };
    
    int total = sizeof(tests) / sizeof(tests[0]);
    int passed = 0;
    int failed = 0;
    
    printf("=== Final Comprehensive Arrangement Test ===\n\n");
    
    for (int i = 0; i < total; i++) {
        // Test Q=0
        const auto* entry0 = findInstruction(tests[i].opcodeQ0);
        if (entry0) {
            char buf[256];
            uint32_t pc = 0;
            formatInstruction(entry0, tests[i].opcodeQ0, &pc, nullptr, nullptr, buf, sizeof(buf));
            if (strstr(buf, tests[i].expectedQ0)) {
                printf("PASS: %s (Q=0) -> %s\n", tests[i].name, buf);
                passed++;
            } else {
                printf("FAIL: %s (Q=0) expected %s, got %s\n", 
                       tests[i].name, tests[i].expectedQ0, buf);
                failed++;
            }
        } else {
            printf("FAIL: %s (Q=0) NOT FOUND\n", tests[i].name);
            failed++;
        }
        
        // Test Q=1
        const auto* entry1 = findInstruction(tests[i].opcodeQ1);
        if (entry1) {
            char buf[256];
            uint32_t pc = 0;
            formatInstruction(entry1, tests[i].opcodeQ1, &pc, nullptr, nullptr, buf, sizeof(buf));
            if (strstr(buf, tests[i].expectedQ1)) {
                printf("PASS: %s (Q=1) -> %s\n", tests[i].name, buf);
                passed++;
            } else {
                printf("FAIL: %s (Q=1) expected %s, got %s\n", 
                       tests[i].name, tests[i].expectedQ1, buf);
                failed++;
            }
        } else {
            printf("FAIL: %s (Q=1) NOT FOUND\n", tests[i].name);
            failed++;
        }
    }
    
    printf("\n=== Summary ===\n");
    printf("Passed: %d/%d (%.1f%%)\n", passed, passed+failed, 100.0*passed/(passed+failed));
    printf("Failed: %d/%d\n", failed, passed+failed);
    
    return failed == 0 ? 0 : 1;
}
