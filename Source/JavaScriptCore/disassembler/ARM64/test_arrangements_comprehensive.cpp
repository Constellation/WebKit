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
    const char* mnemonicHint;  // Help identify aliases
};

int main() {
    TestCase tests[] = {
        // SIMD Arithmetic (size:Q) - ADD uses bits[23:22] for size
        {"ADD (vector)", 0x0e228400, 0x4e228400, "8b", "16b", "add"},
        {"ADD (vector)", 0x0e628400, 0x4e628400, "4h", "8h", "add"},
        {"ADD (vector)", 0x0ea28400, 0x4ea28400, "2s", "4s", "add"},
        {"ADD (vector)", 0x0ee28400, 0x4ee28400, "1d", "2d", "add"},
        
        // FP SIMD (sz:Q) - FMUL uses bit[22] for sz
        {"FMUL (vector)", 0x2e20dc00, 0x6e20dc00, "2s", "4s", "fmul"},
        {"FMUL (vector)", 0x2e60dc00, 0x6e60dc00, "1d", "2d", "fmul"},
        
        // FAMAX/FAMIN - half-precision (opcode=0111)
        {"FAMAX (half)", 0x0ec01c00, 0x4ec01c00, "4h", "8h", "famax"},
        {"FAMIN (half)", 0x0ec0dc00, 0x4ec0dc00, "4h", "8h", "famin"},
        
        // FAMAX/FAMIN - single/double (opcode=11011, size<0>:Q)
        {"FAMAX (single)", 0x0ea1dc00, 0x4ea1dc00, "2s", "4s", "famax"},
        {"FAMIN (single)", 0x0ea19c00, 0x4ea19c00, "2s", "4s", "famin"},
        {"FAMAX (double)", 0, 0x4ee1dc00, "skip", "2d", "famax"},  // Q=0 is RESERVED
        
        // Logical - always bytes
        {"AND (vector)", 0x0e201c00, 0x4e201c00, "8b", "16b", "and"},
        {"EOR (vector)", 0x2e201c00, 0x6e201c00, "8b", "16b", "eor"},
        
        // TBL/TBX - always bytes
        {"TBL", 0x0e000000, 0x4e000000, "8b", "16b", "tbl"},
        {"TBX", 0x0e001000, 0x4e001000, "8b", "16b", "tbx"},
        
        // EXT - always bytes
        {"EXT", 0x2e000000, 0x6e000000, "8b", "16b", "ext"},
        
        // MOVI - bytes
        {"MOVI", 0x0f00e400, 0x4f00e400, "8b", "16b", "movi"},
        
        // FMOV immediate - singles
        {"FMOV (imm)", 0x0f00f400, 0x4f00f400, "2s", "4s", "fmov"},
    };
    
    int numTests = sizeof(tests) / sizeof(tests[0]);
    int passed = 0;
    int failed = 0;
    
    printf("=== Comprehensive Arrangement Test ===\n\n");
    
    for (int i = 0; i < numTests; i++) {
        // Test Q=0 variant
        if (strcmp(tests[i].expectedQ0, "skip") \!= 0) {
            const auto* entry0 = findInstruction(tests[i].opcodeQ0);
            if (entry0) {
                char buffer[256];
                uint32_t pc = 0;
                formatInstruction(entry0, tests[i].opcodeQ0, &pc, nullptr, nullptr, buffer, sizeof(buffer));
                
                if (strstr(buffer, tests[i].expectedQ0)) {
                    printf("PASS: %s (Q=0) -> %s\n", tests[i].family, buffer);
                    passed++;
                } else {
                    printf("FAIL: %s (Q=0) 0x%08x\n", tests[i].family, tests[i].opcodeQ0);
                    printf("  Got: %s\n  Expected arrangement: %s\n", buffer, tests[i].expectedQ0);
                    failed++;
                }
            } else {
                printf("FAIL: %s (Q=0) 0x%08x - NOT FOUND\n", tests[i].family, tests[i].opcodeQ0);
                failed++;
            }
        }
        
        // Test Q=1 variant
        const auto* entry1 = findInstruction(tests[i].opcodeQ1);
        if (entry1) {
            char buffer[256];
            uint32_t pc = 0;
            formatInstruction(entry1, tests[i].opcodeQ1, &pc, nullptr, nullptr, buffer, sizeof(buffer));
            
            if (strstr(buffer, tests[i].expectedQ1)) {
                printf("PASS: %s (Q=1) -> %s\n", tests[i].family, buffer);
                passed++;
            } else {
                printf("FAIL: %s (Q=1) 0x%08x\n", tests[i].family, tests[i].opcodeQ1);
                printf("  Got: %s\n  Expected arrangement: %s\n", buffer, tests[i].expectedQ1);
                failed++;
            }
        } else {
            printf("FAIL: %s (Q=1) 0x%08x - NOT FOUND\n", tests[i].family, tests[i].opcodeQ1);
            failed++;
        }
    }
    
    printf("\n=== Results ===\n");
    printf("Passed: %d/%d\n", passed, passed + failed);
    printf("Failed: %d/%d\n", failed, passed + failed);
    
    return failed == 0 ? 0 : 1;
}
